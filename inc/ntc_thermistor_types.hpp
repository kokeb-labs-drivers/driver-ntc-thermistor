/**
 * @file ntc_thermistor_types.hpp
 * @brief Public types of the NTC thermistor driver: errors, `Result<T>`, the
 * curve and the configuration, the reading, and `Validate()`.
 * @copyright Copyright (c) 2026 The driver-ntc-thermistor authors.
 * SPDX-License-Identifier: MIT
 *
 * @details
 * The driver knows no part. A thermistor is described by a `Curve` (a table of
 * resistance and temperature rows, or R25 and B for the beta equation) and a
 * divider by the rest of `Config`. Every quantity is an integer: resistance in
 * ohms, temperature in milli-degrees Celsius, voltage in millivolts, tolerance
 * in parts per million.
 */
#ifndef NTC_THERMISTOR_TYPES_HPP
#define NTC_THERMISTOR_TYPES_HPP

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

#include "ntc_thermistor_detail.hpp"
#include "ntc_thermistor_math.hpp"
#include "ntc_thermistor_version.h"

/** @brief The NTC thermistor driver: everything it declares lives here;
 * internals in `detail`. */
namespace ntc_thermistor {

/**
 * @defgroup ntc_thermistor_types Driver types
 * @brief Errors, results, the curve, the configuration and `Validate()`.
 * @{
 */

/**
 * @brief Why a call failed. Values 0 to 9 are the shared base set; this
 * driver's own values start at 16.
 *
 * A thermistor read through an ADC has no bus protocol, no identity and no
 * checksum, so `Timeout`, `DeviceIdMismatch`, `Crc`, `Busy`, `DeviceFault` and
 * `Unsupported` are never returned.
 */
enum class [[nodiscard]] Error : uint8_t {
  Ok = 0,               ///< The call succeeded.
  NotInitialized = 1,   ///< `EnsureInitialized()` has not succeeded yet.
  InvalidArgument = 2,  ///< A `Config` is out of range or inconsistent; nothing was read.
  Bus = 3,              ///< A port hook returned `false` (ADC read or bias switch failed).
  Timeout = 4,          ///< Not returned by this driver.
  DeviceIdMismatch = 5, ///< Not returned by this driver.
  Crc = 6,              ///< Not returned by this driver.
  Busy = 7,             ///< Not returned by this driver.
  DeviceFault = 8,      ///< Not returned by this driver.
  Unsupported = 9,      ///< Not returned by this driver.
  OpenCircuit = 16,     ///< The node is at the open rail: broken thermistor, cut
                        ///< wire, missing part.
  ShortCircuit = 17,    ///< The node is at the short rail: shorted part or wire,
                        ///< collapsed supply.
  BelowRange = 18,      ///< Colder than the curve or the window, or the resistance
                        ///< is above it.
  AboveRange = 19,      ///< Hotter than the curve or the window, or the resistance
                        ///< is below it.
  Unstable = 20,        ///< The samples of one reading differ by more than
                        ///< `max_spread_codes`.
  CodeOutOfScale = 21,  ///< A sample is at or above 2^`adc_bits`: the port and
                        ///< `adc_bits` disagree.
};

/**
 * @brief The name of an error, for logs.
 * @param error The error.
 * @return A static string; `"Unknown"` for a value that is not an enumerator.
 */
constexpr const char* ToString(Error error) noexcept {
  switch (error) {
  case Error::Ok:
    return "Ok";
  case Error::NotInitialized:
    return "NotInitialized";
  case Error::InvalidArgument:
    return "InvalidArgument";
  case Error::Bus:
    return "Bus";
  case Error::Timeout:
    return "Timeout";
  case Error::DeviceIdMismatch:
    return "DeviceIdMismatch";
  case Error::Crc:
    return "Crc";
  case Error::Busy:
    return "Busy";
  case Error::DeviceFault:
    return "DeviceFault";
  case Error::Unsupported:
    return "Unsupported";
  case Error::OpenCircuit:
    return "OpenCircuit";
  case Error::ShortCircuit:
    return "ShortCircuit";
  case Error::BelowRange:
    return "BelowRange";
  case Error::AboveRange:
    return "AboveRange";
  case Error::Unstable:
    return "Unstable";
  case Error::CodeOutOfScale:
    return "CodeOutOfScale";
  default:
    break;
  }
  return "Unknown";
}

/**
 * @brief The value of a call, or why there is none.
 * @tparam T The value type. `Result<void>` carries only the error.
 */
template <typename T>
struct [[nodiscard]] Result {
  T value{};              ///< The value; `T{}` when the call failed.
  Error error{Error::Ok}; ///< `Error::Ok` on success.

  /** @return true when the call succeeded. */
  [[nodiscard]] constexpr bool ok() const noexcept {
    return error == Error::Ok;
  }
  /** @return true when the call succeeded. */
  constexpr explicit operator bool() const noexcept {
    return ok();
  }
  /**
   * @brief A successful result.
   * @param v The value.
   * @return `{v, Error::Ok}`.
   */
  static constexpr Result Success(T v) noexcept {
    return {v, Error::Ok};
  }
  /**
   * @brief A failed result.
   * @param e Why it failed; never `Error::Ok`.
   * @return `{T{}, e}`.
   */
  static constexpr Result Failure(Error e) noexcept {
    return {T{}, e};
  }
};

/** @brief The result of a call that returns no value. */
template <>
struct [[nodiscard]] Result<void> {
  Error error{Error::Ok}; ///< `Error::Ok` on success.

  /** @return true when the call succeeded. */
  [[nodiscard]] constexpr bool ok() const noexcept {
    return error == Error::Ok;
  }
  /** @return true when the call succeeded. */
  constexpr explicit operator bool() const noexcept {
    return ok();
  }
  /** @return A successful result. */
  static constexpr Result Success() noexcept {
    return {Error::Ok};
  }
  /**
   * @brief A failed result.
   * @param e Why it failed; never `Error::Ok`.
   * @return `{e}`.
   */
  static constexpr Result Failure(Error e) noexcept {
    return {e};
  }
};

/** @brief Where the thermistor sits in the divider. */
enum class Topology : uint8_t {
  NtcLowSide = 0,  ///< Supply, series resistor, node, thermistor, ground: the
                   ///< node falls as it heats.
  NtcHighSide = 1, ///< Supply, thermistor, node, series resistor, ground: the
                   ///< node rises as it heats.
};

namespace detail {
/// The last `Topology` enumerator, for `IsEnumerator()`.
template <>
struct EnumLast<Topology> {
  /// The value of the last enumerator.
  static constexpr Topology kValue = Topology::NtcHighSide;
};
} // namespace detail

/// Lowest ADC resolution `Config::adc_bits` accepts.
inline constexpr uint8_t kMinAdcBits = 8;
/// Highest ADC resolution `Config::adc_bits` accepts.
inline constexpr uint8_t kMaxAdcBits = 24;
/// Largest series resistor, in ohms (2^24): the bound that keeps the divider
/// arithmetic in 64 bits.
inline constexpr uint32_t kMaxSeriesOhm = 16'777'216;
/// Largest `Curve::r0_ohm`, in ohms.
inline constexpr uint32_t kMaxR0Ohm = 10'000'000;
/// Smallest `Curve::b_kelvin`.
inline constexpr uint32_t kMinBetaKelvin = 1000;
/// Largest `Curve::b_kelvin`.
inline constexpr uint32_t kMaxBetaKelvin = 8000;
/// Fewest rows of a table.
inline constexpr std::size_t kMinTableRows = 2;
/// Most rows of a table.
inline constexpr std::size_t kMaxTableRows = 256;
/// Largest `Config::sample_delay_us` and `Config::settle_us`, in microseconds:
/// bounds the time of one call.
inline constexpr uint32_t kMaxWaitUs = 1'000'000;
/// Largest magnitude of `Curve::sh_a_e15` (1e-2 per kelvin, in units of 1e-15
/// per kelvin).
inline constexpr int64_t kMaxShAE15 = 10'000'000'000'000;
/// Largest magnitude of `Curve::sh_b_e15` (1e-3 per kelvin).
inline constexpr int64_t kMaxShBE15 = 1'000'000'000'000;
/// Largest magnitude of `Curve::sh_c_e15` (2e-6 per kelvin).
inline constexpr int64_t kMaxShCE15 = 2'000'000'000;
/// Largest `Curve::curve_error_mc`, in milli-degrees.
inline constexpr uint32_t kMaxCurveErrorMc = 100'000;
/// Most samples one reading averages.
inline constexpr uint8_t kMaxSampleCount = 16;
/// Largest divider supply or ADC reference, in millivolts.
inline constexpr uint32_t kMaxSupplyMv = 20'000;
/// Largest `Config::offset_mc` magnitude, in milli-degrees Celsius.
inline constexpr int32_t kMaxOffsetMc = 20'000;
/// Largest tolerance, in parts per million (50 %).
inline constexpr uint32_t kMaxTolerancePpm = 500'000;
/// Absolute zero in milli-degrees Celsius: no temperature, window or row may be
/// at or below it.
inline constexpr int32_t kAbsoluteZeroMc = -273'150;
/// `Config::min_temperature_mc` when no lower window is set.
inline constexpr int32_t kNoMinTemperatureMc = std::numeric_limits<int32_t>::min();
/// `Config::max_temperature_mc` when no upper window is set.
inline constexpr int32_t kNoMaxTemperatureMc = std::numeric_limits<int32_t>::max();

/** @brief One row of a resistance and temperature table. */
struct TablePoint {
  uint32_t resistance_ohm{0}; ///< Resistance in ohms at `temperature_mc`.
  int32_t temperature_mc{0};  ///< Temperature in milli-degrees Celsius.
};

/**
 * @brief What the thermistor is: a table, Steinhart-Hart coefficients, or R at
 * a reference temperature and the beta value.
 *
 * A non-empty `table` selects table mode. Otherwise a non-zero `sh_b_e15`
 * selects Steinhart-Hart mode (`1/T = A + B ln R + C (ln R)^3`, R in ohms, T in
 * kelvin) and a zero one selects beta mode, which uses `r0_ohm`,
 * `ref_temperature_mc` and `b_kelvin`. A curve is data: a part is added by
 * writing a `Curve`, never by changing the driver. `tools/ntc_table.py` makes a
 * table from any of these descriptions or from a vendor list.
 */
struct Curve {
  // The initializer stays: aggregates built with designated initializers that
  // omit `table` would otherwise trip -Wmissing-field-initializers.
  /// 2 to 256 rows, any spacing, resistance strictly falling as temperature
  /// strictly rises. The span is not copied: the rows must outlive the driver
  /// (a `static constexpr` array).
  std::span<const TablePoint> table = {}; // NOLINT(readability-redundant-member-init)
  uint32_t r0_ohm{0}; ///< R at `ref_temperature_mc` (R25), 1 to `kMaxR0Ohm`; beta mode.
  int32_t ref_temperature_mc{25'000}; ///< T0 of `r0_ohm`: 25 degC for nearly every part.
  uint32_t b_kelvin{0};       ///< Beta mode: B in kelvin, `kMinBetaKelvin` to `kMaxBetaKelvin`.
  int32_t b_t2_mc{85'000};    ///< The second temperature of the B pair (B25/85: 85 degC).
  uint32_t r_tol_ppm{0};      ///< Tolerance of R at the reference and of the rows; 0
                              ///< = not declared.
  uint32_t b_tol_ppm{0};      ///< Tolerance of B (beta and table modes; 0 = not
                              ///< declared); not with SH.
  int64_t sh_a_e15{0};        ///< Steinhart-Hart A in units of 1e-15 per kelvin
                              ///< (0.00087676 is 876'760'000'000).
  int64_t sh_b_e15{0};        ///< Steinhart-Hart B in units of 1e-15 per kelvin;
                              ///< non-zero selects SH mode.
  int64_t sh_c_e15{0};        ///< Steinhart-Hart C in units of 1e-15 per kelvin (the cubic term).
  uint32_t curve_error_mc{0}; ///< Error of the curve itself (fit,
                              ///< interpolation), added to the bound.
};

/**
 * @brief Everything one thermistor and its divider need. There is no default
 * part: a `Config{}` fails `Validate()` until a `Curve` is declared.
 */
struct Config {
  Curve curve{};                           ///< The thermistor.
  uint8_t adc_channel{0};                  ///< Passed to the port's `ReadRaw()`.
  uint8_t adc_bits{12};                    ///< ADC resolution, `kMinAdcBits` to `kMaxAdcBits`.
  Topology topology{Topology::NtcLowSide}; ///< Where the thermistor sits in the divider.
  uint32_t series_ohm{10'000};             ///< The other divider resistor, 1 to `kMaxSeriesOhm`.
  uint32_t series_tol_ppm{0};              ///< Its tolerance; 0 = not declared.
  uint32_t adc_vref_mv{0};                 ///< ADC reference, mV; 0 = ratiometric.
  uint32_t divider_supply_mv{0}; ///< Divider supply, mV; 0 = ratiometric; both or neither.
  int32_t offset_mc{0};          ///< Added to the temperature; magnitude at most `kMaxOffsetMc`.
  uint8_t sample_count{1};       ///< Codes averaged per reading, 1 to `kMaxSampleCount`.
  uint32_t sample_delay_us{0};   ///< Wait between samples; needs the port's `DelayUs()`.
  uint32_t settle_us{0};         ///< Wait after `SetBias(true)`; needs `SetBias()` and `DelayUs()`.
  uint32_t max_spread_codes{0};  ///< Largest max-min of the samples; 0 = not checked.
  uint32_t open_margin_codes{2}; ///< Codes at the open rail that count as an open circuit.
  uint32_t short_margin_codes{2}; ///< Codes at the short rail that count as a short circuit.
  int32_t min_temperature_mc{kNoMinTemperatureMc}; ///< Lower end of the valid window; mandatory in
                                                   ///< beta mode.
  int32_t max_temperature_mc{kNoMaxTemperatureMc}; ///< Upper end of the valid window; mandatory in
                                                   ///< beta mode.
};

/** @brief One reading: the temperature and what it was computed from. */
struct Reading {
  int32_t temperature_mc{0};  ///< Milli-degrees Celsius, window and offset applied.
  uint32_t resistance_ohm{0}; ///< The thermistor's resistance computed from the mean code.
  uint32_t code{0};           ///< The mean ADC code.
  uint32_t uncertainty_mc{0}; ///< Bound on the declared tolerances and one LSB.
};

namespace detail {

/// Whether `t` is a usable table: size, strictly falling resistance, strictly
/// rising temperature.
[[nodiscard]] constexpr bool TableValid(std::span<const TablePoint> t) noexcept {
  const std::size_t rows = t.size();
  if ((rows < kMinTableRows) || (rows > kMaxTableRows)) {
    return false;
  }
  bool ok = (t[0].resistance_ohm > 0U) && (t[0].temperature_mc > kAbsoluteZeroMc);
  for (std::size_t i = 1; i < rows; ++i) {
    const TablePoint& prev = t[i - 1U];
    const TablePoint& row = t[i];
    // Two rows whose logarithms agree to Q16 would divide by zero in the
    // interpolation.
    ok = ok && (Ln16(prev.resistance_ohm * kFineScale) > Ln16(row.resistance_ohm * kFineScale)) &&
         (row.resistance_ohm > 0U) && (row.temperature_mc > prev.temperature_mc);
  }
  return ok;
}

/// Whether the window is declared and above absolute zero (an equation
/// extrapolates, so its window is the part's rated range; a window above
/// absolute zero keeps 1/T positive at both ends).
[[nodiscard]] constexpr bool WindowDeclared(const Config& config) noexcept {
  return (config.min_temperature_mc != kNoMinTemperatureMc) &&
         (config.max_temperature_mc != kNoMaxTemperatureMc) &&
         (config.min_temperature_mc > kAbsoluteZeroMc);
}

/// Whether the Steinhart-Hart fields are all zero (beta and table modes).
[[nodiscard]] constexpr bool NoShCoefficients(const Curve& c) noexcept {
  return (c.sh_a_e15 == 0) && (c.sh_b_e15 == 0) && (c.sh_c_e15 == 0);
}

/// ln(2^32)^2 in Q16, rounded up: the largest square of ln R the Steinhart-Hart
/// maths sees.
inline constexpr int64_t kShLnSquaredQ16 = 32'300'000;

/// Whether the Steinhart-Hart coefficients and the window of `config` are
/// usable: bounded, and 1/T rising with ln R over the whole resistance range (B
/// > 0 and B + 3 C ln(R)^2 > 0 at both ends).
[[nodiscard]] constexpr bool ShValid(const Config& config) noexcept {
  const Curve& c = config.curve;
  const bool bounded = (c.sh_a_e15 >= -kMaxShAE15) && (c.sh_a_e15 <= kMaxShAE15) &&
                       (c.sh_b_e15 >= 1) && (c.sh_b_e15 <= kMaxShBE15) &&
                       (c.sh_c_e15 >= -kMaxShCE15) && (c.sh_c_e15 <= kMaxShCE15);
  const bool unused = (c.r0_ohm == 0U) && (c.b_kelvin == 0U) && (c.b_tol_ppm == 0U);
  const bool rising = ((c.sh_b_e15 * 65'536) + (3 * c.sh_c_e15 * kShLnSquaredQ16)) > 0;
  return bounded && unused && rising && WindowDeclared(config);
}

/// Whether the beta parameters and the window of `config` are usable.
[[nodiscard]] constexpr bool BetaValid(const Config& config) noexcept {
  const Curve& c = config.curve;
  const bool parameters = (c.r0_ohm >= 1U) && (c.b_kelvin >= kMinBetaKelvin) &&
                          (c.b_kelvin <= kMaxBetaKelvin) && (c.b_t2_mc != c.ref_temperature_mc) &&
                          (c.b_t2_mc > kAbsoluteZeroMc);
  return parameters && WindowDeclared(config) && NoShCoefficients(c);
}

/// Whether the curve of `config` is exactly one of a valid table,
/// Steinhart-Hart or beta description.
[[nodiscard]] constexpr bool CurveValid(const Config& config) noexcept {
  const Curve& c = config.curve;
  const bool common =
      (c.r0_ohm <= kMaxR0Ohm) && (c.ref_temperature_mc > kAbsoluteZeroMc) &&
      (c.r_tol_ppm <= kMaxTolerancePpm) && (c.b_tol_ppm <= kMaxTolerancePpm) &&
      (c.curve_error_mc <= kMaxCurveErrorMc) &&
      ((c.b_kelvin == 0U) || ((c.b_kelvin >= kMinBetaKelvin) && (c.b_kelvin <= kMaxBetaKelvin)));
  if (!common) {
    return false;
  }
  if (!c.table.empty()) {
    return TableValid(c.table) && NoShCoefficients(c);
  }
  return (c.sh_b_e15 != 0) ? ShValid(config) : BetaValid(config);
}

/// Whether the divider fields of `config` are usable.
[[nodiscard]] constexpr bool DividerValid(const Config& config) noexcept {
  const bool bits = (config.adc_bits >= kMinAdcBits) && (config.adc_bits <= kMaxAdcBits) &&
                    IsEnumerator(config.topology);
  const bool series = (config.series_ohm >= 1U) && (config.series_ohm <= kMaxSeriesOhm) &&
                      (config.series_tol_ppm <= kMaxTolerancePpm);
  const bool ratiometric = (config.adc_vref_mv == 0U) && (config.divider_supply_mv == 0U);
  const bool separate = (config.adc_vref_mv >= 1U) && (config.adc_vref_mv <= kMaxSupplyMv) &&
                        (config.divider_supply_mv >= 1U) &&
                        (config.divider_supply_mv <= kMaxSupplyMv) &&
                        (config.open_margin_codes >= 1U) && (config.short_margin_codes >= 1U);
  return bits && series && (ratiometric || separate);
}

/// Whether the sampling, margin, window and offset fields of `config` are
/// usable.
[[nodiscard]] constexpr bool SamplingValid(const Config& config) noexcept {
  const bool samples = (config.sample_count >= 1U) && (config.sample_count <= kMaxSampleCount);
  const uint64_t margins =
      static_cast<uint64_t>(config.open_margin_codes) + config.short_margin_codes;
  const uint32_t top = (uint32_t{1U} << (config.adc_bits & 31U)) - 1U;
  const bool window = (config.min_temperature_mc < config.max_temperature_mc) &&
                      (config.offset_mc >= -kMaxOffsetMc) && (config.offset_mc <= kMaxOffsetMc);
  const bool waits = (config.sample_delay_us <= kMaxWaitUs) && (config.settle_us <= kMaxWaitUs);
  return samples && window && waits && (margins < static_cast<uint64_t>(top));
}

} // namespace detail

/**
 * @brief Check a configuration without touching the hardware.
 *
 * A curve is exactly one of table mode (2 to 256 rows, resistance strictly
 * falling, temperature strictly rising), Steinhart-Hart mode (bounded
 * coefficients, 1/T rising with ln R, a declared window) or beta mode
 * (`r0_ohm`, `b_kelvin` in range, `b_t2_mc` not the reference, and a window
 * that is declared). An empty curve is refused: no part is ever assumed. A wait
 * is at most `kMaxWaitUs`.
 *
 * @param config The configuration.
 * @return `Error::Ok` or `Error::InvalidArgument`.
 */
[[nodiscard]] constexpr Error Validate(const Config& config) noexcept {
  const bool ok =
      detail::CurveValid(config) && detail::DividerValid(config) && detail::SamplingValid(config);
  return ok ? Error::Ok : Error::InvalidArgument;
}

/** @return The driver version, e.g. `"0.1.0"`. No I/O. */
constexpr const char* GetDriverVersion() noexcept {
  return NTC_THERMISTOR_VERSION_STRING;
}
/** @return The major version. No I/O. */
constexpr uint32_t GetDriverVersionMajor() noexcept {
  return NTC_THERMISTOR_VERSION_MAJOR;
}
/** @return The minor version. No I/O. */
constexpr uint32_t GetDriverVersionMinor() noexcept {
  return NTC_THERMISTOR_VERSION_MINOR;
}
/** @return The patch version. No I/O. */
constexpr uint32_t GetDriverVersionPatch() noexcept {
  return NTC_THERMISTOR_VERSION_PATCH;
}

/** @} */

} // namespace ntc_thermistor

#endif // NTC_THERMISTOR_TYPES_HPP
