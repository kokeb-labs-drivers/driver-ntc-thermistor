/**
 * @file ntc_thermistor_conversion.hpp
 * @brief From an ADC code to a temperature: the divider, the curve, the bound,
 * and the helpers around them. Pure functions of a `Config` and a code; no
 * port, no state.
 * @copyright Copyright (c) 2026 The driver-ntc-thermistor authors.
 * SPDX-License-Identifier: MIT
 *
 * @details
 * The pipeline (docs/conversion.md): classify the code (open, short, out of
 * scale), the divider to ohms, the curve to milli-degrees Celsius (a table
 * interpolated in ln R, or the beta equation), the window, the offset, and the
 * uncertainty bound. Only integer arithmetic: no libm, no floating point, no
 * loop without a constant bound.
 */
#ifndef NTC_THERMISTOR_CONVERSION_HPP
#define NTC_THERMISTOR_CONVERSION_HPP

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

#include "ntc_thermistor_detail.hpp"
#include "ntc_thermistor_math.hpp"
#include "ntc_thermistor_types.hpp"

namespace ntc_thermistor::detail {

/// 2^32 times 1000, the numerator of 1/T in Q32 per kelvin for a temperature in
/// milli-kelvin.
inline constexpr int64_t kInvScale = 4'294'967'296'000;
/// Milli-kelvin at 0 degC.
inline constexpr int64_t kZeroCelsiusMk = 273'150;
/// Iterations of the table's binary search: ceil(log2(255 segments)).
inline constexpr uint32_t kSearchSteps = 8U;
/// Parts per million.
inline constexpr uint64_t kPpm = 1'000'000U;
/// The largest value a 32-bit unsigned number holds, as a 64-bit one.
inline constexpr uint64_t kMaxU32 = std::numeric_limits<uint32_t>::max();

/// The ADC's full scale, 2^bits (callers pass 8 to 24).
[[nodiscard]] constexpr uint32_t FullScale(uint8_t bits) noexcept {
  return uint32_t{1U} << (bits & 31U);
}

/// Whether the divider is ratiometric (supply and reference cancel).
[[nodiscard]] constexpr bool Ratiometric(const Config& config) noexcept {
  return (config.adc_vref_mv == 0U) || (config.divider_supply_mv == 0U);
}

/// The node's share of the supply as a fraction `num / den`.
struct Ratio {
  uint64_t num{0}; ///< Numerator.
  uint64_t den{1}; ///< Denominator.
};

/// The node ratio of `code`: `code / 2^bits`, or scaled by reference over
/// supply.
[[nodiscard]] constexpr Ratio NodeRatio(const Config& config, uint32_t code) noexcept {
  const uint64_t scale = FullScale(config.adc_bits);
  if (Ratiometric(config)) {
    return Ratio{.num = code, .den = scale};
  }
  return Ratio{.num = static_cast<uint64_t>(code) * config.adc_vref_mv,
               .den = scale * config.divider_supply_mv};
}

/// The highest code the node can reach: the ADC's top code, or fewer when the
/// reference is above the divider's supply (the node then cannot read the codes
/// above `2^bits * supply / reference`).
[[nodiscard]] constexpr uint32_t RailTop(const Config& config) noexcept {
  const uint64_t scale = FullScale(config.adc_bits);
  uint64_t rail = scale;
  if (!Ratiometric(config) && (config.adc_vref_mv > config.divider_supply_mv)) {
    rail = (scale * config.divider_supply_mv) / config.adc_vref_mv;
  }
  return static_cast<uint32_t>((rail > 0U) ? (rail - 1U) : 0U);
}

/**
 * @brief What a code says about the wiring, before any maths.
 * @param config A valid configuration.
 * @param code A sample or mean code.
 * @return `Ok`, `CodeOutOfScale`, `OpenCircuit` or `ShortCircuit`.
 */
[[nodiscard]] constexpr Error ClassifyCode(const Config& config, uint32_t code) noexcept {
  const uint32_t scale = FullScale(config.adc_bits);
  if (code >= scale) {
    return Error::CodeOutOfScale;
  }
  const uint32_t top = RailTop(config);
  const bool low_side = config.topology == Topology::NtcLowSide;
  // The margins belong to the faults, not to the rails: the rails swap with the
  // topology.
  const uint32_t high_margin = low_side ? config.open_margin_codes : config.short_margin_codes;
  const uint32_t low_margin = low_side ? config.short_margin_codes : config.open_margin_codes;
  const Error high_fault = low_side ? Error::OpenCircuit : Error::ShortCircuit;
  const Error low_fault = low_side ? Error::ShortCircuit : Error::OpenCircuit;
  if ((high_margin >= top) || (code >= (top - high_margin))) {
    return high_fault;
  }
  // Below the high margin the node ratio is below 1 (the top is the supply's,
  // not the ADC's, when the reference is above the supply), and above the low
  // margin it is above 0.
  return (code <= low_margin) ? low_fault : Error::Ok;
}

/// `numerator / divisor` in fine ohms (1/256 ohm), rounded; saturates when it
/// does not fit.
[[nodiscard]] constexpr uint64_t FineQuotient(uint64_t numerator, uint64_t divisor) noexcept {
  if (divisor == 0U) {
    return kMaxU64; // not reachable from a classified code; keeps the division
                    // defined
  }
  const uint64_t whole = numerator / divisor;
  const uint64_t part = DivRoundU((numerator % divisor) * kFineScale, divisor);
  return SatAdd(SatMul(whole, kFineScale), part);
}

/**
 * @brief The thermistor's resistance for a code `ClassifyCode()` accepted, in
 * fine ohms (1/256 ohm).
 * @param config A valid configuration.
 * @param code A code with `0 < num < den` (the classification guarantees it).
 * @return Fine ohms, rounded; the largest `uint64_t` when it does not fit.
 */
[[nodiscard]] constexpr uint64_t DividerFine(const Config& config, uint32_t code) noexcept {
  const Ratio r = NodeRatio(config, code);
  const uint64_t series = config.series_ohm;
  if (config.topology == Topology::NtcLowSide) {
    return FineQuotient(SatMul(series, r.num), r.den - r.num);
  }
  return FineQuotient(SatMul(series, r.den - r.num), r.num);
}

/// A table row's resistance in fine ohms.
[[nodiscard]] constexpr uint64_t RowFine(const TablePoint& row) noexcept {
  return static_cast<uint64_t>(row.resistance_ohm) * kFineScale;
}

/// The segment [i, i + 1] of `t` with `t[i].R >= r > t[i + 1].R`, by a search
/// of constant bound.
[[nodiscard]] constexpr std::size_t FindSegment(std::span<const TablePoint> t,
                                                uint64_t r) noexcept {
  std::size_t lo = 0U;
  std::size_t hi = t.size() - 1U;
  for (uint32_t step = 0U; step < kSearchSteps; ++step) {
    if ((hi - lo) <= 1U) {
      break;
    }
    const std::size_t mid = lo + ((hi - lo) / 2U);
    if (RowFine(t[mid]) >= r) {
      lo = mid;
    } else {
      hi = mid;
    }
  }
  return lo;
}

/**
 * @brief The temperature between two rows, linear in ln R.
 * @param a The row at the higher resistance (colder).
 * @param b The row at the lower resistance (hotter).
 * @param r The resistance in fine ohms, `b.R < r <= a.R`.
 * @return Milli-degrees Celsius, rounded; `a`'s temperature if the rows'
 * logarithms agree.
 */
[[nodiscard]] constexpr int32_t Interpolate(const TablePoint& a, const TablePoint& b,
                                            uint64_t r) noexcept {
  const int64_t la = Ln16(RowFine(a));
  const int64_t lb = Ln16(RowFine(b));
  const int64_t lr = Ln16(r);
  if (la <= lb) {
    return a.temperature_mc;
  }
  const int64_t span_t = static_cast<int64_t>(b.temperature_mc) - a.temperature_mc;
  const int64_t t = static_cast<int64_t>(a.temperature_mc) + DivRound(span_t * (la - lr), la - lb);
  return static_cast<int32_t>(t);
}

/// The temperature of `r` on a table; colder or hotter than its ends is an
/// error.
[[nodiscard]] constexpr Result<int32_t> TableTemperature(std::span<const TablePoint> t,
                                                         uint64_t r) noexcept {
  const TablePoint& first = t[0];
  const TablePoint& last = t[t.size() - 1U];
  if (r > RowFine(first)) {
    return Result<int32_t>::Failure(Error::BelowRange);
  }
  if (r < RowFine(last)) {
    return Result<int32_t>::Failure(Error::AboveRange);
  }
  if (r == RowFine(last)) {
    return Result<int32_t>::Success(last.temperature_mc);
  }
  const std::size_t i = FindSegment(t, r);
  return Result<int32_t>::Success(Interpolate(t[i], t[i + 1U], r));
}

/// The temperature of `r` by the beta equation, 1/T = 1/T0 + ln(R/R0) / B, in
/// Q32 per kelvin.
[[nodiscard]] constexpr Result<int32_t> BetaTemperature(const Curve& c, uint64_t r) noexcept {
  const int64_t x = static_cast<int64_t>(Ln16(r)) -
                    static_cast<int64_t>(Ln16(static_cast<uint64_t>(c.r0_ohm) * kFineScale));
  const int64_t term = DivRound(x * 65'536, static_cast<int64_t>(c.b_kelvin));
  const int64_t t0_mk = static_cast<int64_t>(c.ref_temperature_mc) + kZeroCelsiusMk;
  const int64_t sum = DivRound(kInvScale, t0_mk) + term;
  if (sum <= 0) {
    return Result<int32_t>::Failure(Error::AboveRange);
  }
  const int64_t t_mc = DivRound(kInvScale, sum) - kZeroCelsiusMk;
  if (t_mc > std::numeric_limits<int32_t>::max()) {
    return Result<int32_t>::Failure(Error::AboveRange);
  }
  return Result<int32_t>::Success(static_cast<int32_t>(t_mc));
}

/// 2^40 fine ohms (2^32 ohms): the largest resistance the Steinhart-Hart maths
/// takes.
inline constexpr uint64_t kShMaxFine = uint64_t{1U} << 40U;
/// 1e18: the numerator of T in milli-kelvin when 1/T is in units of 1e-15 per
/// kelvin.
inline constexpr int64_t kShInvScale = 1'000'000'000'000'000'000;
/// 1 / 65536, as the divisor of a Q16 product.
inline constexpr int64_t kQ16 = 65'536;

/// The temperature of `r` by Steinhart-Hart, 1/T = A + B ln R + C (ln R)^3,
/// with R in ohms.
[[nodiscard]] constexpr Result<int32_t> ShTemperature(const Curve& c, uint64_t r) noexcept {
  const uint64_t bounded = (r > kShMaxFine) ? kShMaxFine : r;
  const int64_t x = static_cast<int64_t>(Ln16(bounded)) - static_cast<int64_t>(Ln16(kFineScale));
  const int64_t x2 = DivRound(x * x, kQ16);
  const int64_t x3 = DivRound(x2 * x, kQ16);
  const int64_t inverse =
      c.sh_a_e15 + DivRound(c.sh_b_e15 * x, kQ16) + DivRound(c.sh_c_e15 * x3, kQ16);
  if (inverse <= 0) {
    return Result<int32_t>::Failure(Error::AboveRange);
  }
  const int64_t t_mc = DivRound(kShInvScale, inverse) - kZeroCelsiusMk;
  if (t_mc > std::numeric_limits<int32_t>::max()) {
    return Result<int32_t>::Failure(Error::AboveRange);
  }
  return Result<int32_t>::Success(static_cast<int32_t>(t_mc));
}

/// The temperature of `r` (fine ohms) on the curve, before window and offset.
[[nodiscard]] constexpr Result<int32_t> CurveTemperature(const Curve& c, uint64_t r) noexcept {
  if (r == 0U) {
    return Result<int32_t>::Failure(Error::AboveRange);
  }
  if (!c.table.empty()) {
    return TableTemperature(c.table, r);
  }
  return (c.sh_b_e15 != 0) ? ShTemperature(c, r) : BetaTemperature(c, r);
}

/// `a + b` clamped to the `int32_t` range.
[[nodiscard]] constexpr int32_t SaturatingAdd(int32_t a, int32_t b) noexcept {
  return ClampToInt32(static_cast<int64_t>(a) + b);
}

/// The temperature of `r` (fine ohms) for a bound: a resistance beyond the
/// curve's reach counts as its end.
[[nodiscard]] constexpr int64_t ClampedTemperature(const Curve& c, uint64_t r) noexcept {
  uint64_t clamped = r;
  if (!c.table.empty()) {
    const uint64_t hot = RowFine(c.table[c.table.size() - 1U]);
    const uint64_t cold = RowFine(c.table[0]);
    clamped = (r < hot) ? hot : r;
    clamped = (clamped > cold) ? cold : clamped;
  }
  const Result<int32_t> t = CurveTemperature(c, clamped);
  if (t) {
    return t.value;
  }
  return (t.error == Error::BelowRange) ? std::numeric_limits<int32_t>::min()
                                        : std::numeric_limits<int32_t>::max();
}

/// The temperature spread when R (fine ohms) is off by `ppm`, rounded outward
/// to a fine ohm.
[[nodiscard]] constexpr uint64_t ResistanceTerm(const Curve& c, uint64_t r, int32_t t_mc,
                                                uint32_t ppm) noexcept {
  if (ppm == 0U) {
    return 0U;
  }
  const uint64_t delta = (SatMul(r, ppm) / kPpm) + 1U;
  const uint64_t cold_r = SatAdd(r, delta);
  const uint64_t hot_r = (delta >= r) ? uint64_t{1U} : (r - delta);
  const uint64_t up = AbsDiff(ClampedTemperature(c, cold_r), t_mc);
  const uint64_t down = AbsDiff(ClampedTemperature(c, hot_r), t_mc);
  return (up > down) ? up : down;
}

/**
 * @brief The temperature spread from the tolerance of B, for a table as well as
 * for beta mode: T^2 |1/T - 1/T0| tol, which is T^2 |ln(R/R0)| / B * tol when
 * the curve is a beta curve and uses the part's own implied B at this point
 * when it is a table.
 */
[[nodiscard]] constexpr uint64_t BetaTerm(const Curve& c, int32_t t_mc) noexcept {
  if (c.b_tol_ppm == 0U) {
    return 0U;
  }
  const auto t_mk = static_cast<uint64_t>(static_cast<int64_t>(t_mc) + kZeroCelsiusMk);
  const auto t0_mk =
      static_cast<uint64_t>(static_cast<int64_t>(c.ref_temperature_mc) + kZeroCelsiusMk);
  const uint64_t per_unit = DivRoundU(
      SatMul(t_mk, AbsDiff(static_cast<int64_t>(t_mk), static_cast<int64_t>(t0_mk))), t0_mk);
  return DivRoundU(SatMul(per_unit, c.b_tol_ppm), kPpm);
}

/// Whether `code` has a node ratio strictly between the rails (a neighbour
/// usable for the LSB).
[[nodiscard]] constexpr bool CodeUsable(const Config& config, uint32_t code) noexcept {
  const bool inside = (code >= 1U) && (code < FullScale(config.adc_bits));
  if (!inside) {
    return false;
  }
  const Ratio r = NodeRatio(config, code);
  return (r.num >= 1U) && (r.num < r.den);
}

/// The temperature change when the code moves by one step; 0 when that
/// neighbour is unusable.
[[nodiscard]] constexpr uint64_t NeighbourTerm(const Config& config, uint32_t neighbour,
                                               int32_t t_mc) noexcept {
  if (!CodeUsable(config, neighbour)) {
    return 0U;
  }
  return AbsDiff(ClampedTemperature(config.curve, DividerFine(config, neighbour)), t_mc);
}

/**
 * @brief The bound on the temperature, in milli-degrees: the linear sum of the
 * declared tolerances (R, series resistor, B), the curve error and one ADC
 * code, at least 1.
 * @param config A valid configuration.
 * @param code The mean code.
 * @param r The resistance computed from it, in fine ohms (1/256 ohm).
 * @param t_mc The temperature from the curve, before the window and the offset.
 * @return The bound, saturated at the largest `uint32_t`.
 */
[[nodiscard]] constexpr uint32_t Uncertainty(const Config& config, uint32_t code, uint64_t r,
                                             int32_t t_mc) noexcept {
  const uint64_t up = NeighbourTerm(config, code + 1U, t_mc);
  const uint64_t down = NeighbourTerm(config, code - 1U, t_mc);
  uint64_t total = (up > down) ? up : down;
  total = SatAdd(total, ResistanceTerm(config.curve, r, t_mc, config.curve.r_tol_ppm));
  total = SatAdd(total, ResistanceTerm(config.curve, r, t_mc, config.series_tol_ppm));
  total = SatAdd(total, BetaTerm(config.curve, t_mc));
  total = SatAdd(total, config.curve.curve_error_mc);
  total = (total < 1U) ? uint64_t{1U} : total;
  return static_cast<uint32_t>((total > kMaxU32) ? kMaxU32 : total);
}

/// The resistance of a code in fine ohms (1/256 ohm), or the code's fault.
[[nodiscard]] constexpr Result<uint64_t> FineOfCode(const Config& config, uint32_t code) noexcept {
  const Error fault = ClassifyCode(config, code);
  if (fault != Error::Ok) {
    return Result<uint64_t>::Failure(fault);
  }
  return Result<uint64_t>::Success(DividerFine(config, code));
}

/// Fine ohms rounded to whole ohms.
[[nodiscard]] constexpr uint64_t WholeOhms(uint64_t fine) noexcept {
  return DivRoundU(fine, kFineScale);
}

/// The resistance for a code in ohms, or the code's fault, or `BelowRange` when
/// it does not fit 32 bits.
[[nodiscard]] constexpr Result<uint32_t> ResistanceOfCode(const Config& config,
                                                          uint32_t code) noexcept {
  const Result<uint64_t> fine = FineOfCode(config, code);
  if (!fine) {
    return Result<uint32_t>::Failure(fine.error);
  }
  const uint64_t ohms = WholeOhms(fine.value);
  if (ohms > kMaxU32) {
    return Result<uint32_t>::Failure(Error::BelowRange);
  }
  return Result<uint32_t>::Success(static_cast<uint32_t>(ohms));
}

/// The conversion of a mean code of an already validated configuration.
[[nodiscard]] constexpr Result<Reading> ConvertCode(const Config& config, uint32_t code) noexcept {
  const Result<uint64_t> fine = FineOfCode(config, code);
  if (!fine) {
    return Result<Reading>::Failure(fine.error);
  }
  const uint64_t ohms = WholeOhms(fine.value);
  if (ohms > kMaxU32) {
    return Result<Reading>::Failure(Error::BelowRange);
  }
  const Result<int32_t> t = CurveTemperature(config.curve, fine.value);
  if (!t) {
    return Result<Reading>::Failure(t.error);
  }
  if (t.value < config.min_temperature_mc) {
    return Result<Reading>::Failure(Error::BelowRange);
  }
  if (t.value > config.max_temperature_mc) {
    return Result<Reading>::Failure(Error::AboveRange);
  }
  return Result<Reading>::Success(
      Reading{.temperature_mc = SaturatingAdd(t.value, config.offset_mc),
              .resistance_ohm = static_cast<uint32_t>(ohms),
              .code = code,
              .uncertainty_mc = Uncertainty(config, code, fine.value, t.value)});
}

} // namespace ntc_thermistor::detail

namespace ntc_thermistor {

/**
 * @brief The pure conversion: the maths for an ADC code, with no port and no
 * state.
 *
 * Use it to turn a code the application already has into a temperature, or to
 * test a configuration. A code is the mean code of one reading.
 *
 * @param config The configuration; it is validated first.
 * @param mean_code The ADC code, 0 to 2^`adc_bits` - 1.
 * @return The reading, or `InvalidArgument`, `CodeOutOfScale`, `OpenCircuit`,
 * `ShortCircuit`, `BelowRange` or `AboveRange`.
 */
[[nodiscard]] constexpr Result<Reading> Convert(const Config& config, uint32_t mean_code) noexcept {
  if (Validate(config) != Error::Ok) {
    return Result<Reading>::Failure(Error::InvalidArgument);
  }
  return detail::ConvertCode(config, mean_code);
}

/**
 * @brief The thermistor's resistance for a code: the divider arithmetic only,
 * with no curve and no window.
 * @param config The configuration; it is validated first.
 * @param mean_code The ADC code.
 * @return Ohms, or `InvalidArgument`, `CodeOutOfScale`, `OpenCircuit`,
 * `ShortCircuit` or `BelowRange` (the resistance does not fit 32 bits).
 */
[[nodiscard]] constexpr Result<uint32_t> ConvertResistance(const Config& config,
                                                           uint32_t mean_code) noexcept {
  if (Validate(config) != Error::Ok) {
    return Result<uint32_t>::Failure(Error::InvalidArgument);
  }
  return detail::ResistanceOfCode(config, mean_code);
}

/**
 * @brief A temperature in milli-kelvin.
 * @param mc Milli-degrees Celsius.
 * @return `mc + 273150`, saturated at the `int32_t` range.
 */
[[nodiscard]] constexpr int32_t ToMilliKelvin(int32_t mc) noexcept {
  return detail::SaturatingAdd(mc, static_cast<int32_t>(detail::kZeroCelsiusMk));
}

/**
 * @brief A temperature in milli-degrees Fahrenheit.
 * @param mc Milli-degrees Celsius.
 * @return `mc * 9 / 5 + 32000`, rounded, saturated at the `int32_t` range.
 */
[[nodiscard]] constexpr int32_t ToMilliFahrenheit(int32_t mc) noexcept {
  const int64_t scaled = detail::DivRound(static_cast<int64_t>(mc) * 9, int64_t{5});
  return detail::ClampToInt32(scaled + 32'000);
}

/**
 * @brief The power the divider puts into the thermistor, in microwatts: V^2 *
 * Rn / (Rs + Rn)^2.
 *
 * Needs `divider_supply_mv`; a ratiometric configuration does not know the
 * supply and gets 0. Divide by the part's dissipation constant (mW/K in your
 * mounting) for the temperature rise.
 *
 * @param config The configuration.
 * @param r_ntc_ohm The thermistor's resistance.
 * @return Microwatts, rounded; 0 when the supply is not declared.
 */
[[nodiscard]] constexpr uint32_t SelfHeatingMicrowatts(const Config& config,
                                                       uint32_t r_ntc_ohm) noexcept {
  if (config.divider_supply_mv == 0U) {
    return 0U;
  }
  const uint64_t supply = config.divider_supply_mv;
  const uint64_t total = static_cast<uint64_t>(config.series_ohm) + r_ntc_ohm;
  const uint64_t volts_squared_r = (supply * supply) * r_ntc_ohm;
  const uint64_t p = detail::DivRoundU(detail::DivRoundU(volts_squared_r, total), total);
  return static_cast<uint32_t>((p > detail::kMaxU32) ? detail::kMaxU32 : p);
}

} // namespace ntc_thermistor

#endif // NTC_THERMISTOR_CONVERSION_HPP
