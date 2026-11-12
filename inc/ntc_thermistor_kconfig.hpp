/**
 * @file ntc_thermistor_kconfig.hpp
 * @brief Optional ESP-IDF layer: a `Config` built from the
 * `CONFIG_NTC_THERMISTOR_*` Kconfig options.
 * @copyright Copyright (c) 2026 The driver-ntc-thermistor authors.
 * SPDX-License-Identifier: MIT
 *
 * Include it only where menuconfig should choose the defaults; without Kconfig
 * `KconfigDefaults()` equals `Config{}` (no part: `Validate()` refuses it). One
 * configuration per program: every translation unit of one program must see the
 * same `CONFIG_` values, and no driver header includes this one (handbook
 * KB-CPP20-018).
 */
#ifndef NTC_THERMISTOR_KCONFIG_HPP
#define NTC_THERMISTOR_KCONFIG_HPP

#ifdef __has_include
#if __has_include("sdkconfig.h")
#include "sdkconfig.h"
#endif
#endif

#include "ntc_thermistor_presets.hpp"
#include "ntc_thermistor_types.hpp"

#ifndef CONFIG_NTC_THERMISTOR_ADC_CHANNEL
/** ADC channel when Kconfig does not set it: `Config{}.adc_channel`. */
#define CONFIG_NTC_THERMISTOR_ADC_CHANNEL 0
#endif
#ifndef CONFIG_NTC_THERMISTOR_ADC_BITS
/** ADC resolution when Kconfig does not set it: `Config{}.adc_bits`. */
#define CONFIG_NTC_THERMISTOR_ADC_BITS 12
#endif
#ifndef CONFIG_NTC_THERMISTOR_SERIES_OHM
/** Series resistor when Kconfig does not set it: `Config{}.series_ohm`. */
#define CONFIG_NTC_THERMISTOR_SERIES_OHM 10000
#endif
#ifndef CONFIG_NTC_THERMISTOR_SAMPLES
/** Samples averaged when Kconfig does not set it: `Config{}.sample_count`. */
#define CONFIG_NTC_THERMISTOR_SAMPLES 1
#endif

namespace ntc_thermistor {

namespace detail {

/// The curve menuconfig chose; empty (no part) when no curve option is set.
[[nodiscard]] constexpr Curve KconfigCurve() noexcept {
#ifdef CONFIG_NTC_THERMISTOR_CURVE_TDK_NTCG163JF103FT1S
  return presets::kTdkNtcg163jf103ft1s;
#elif defined(CONFIG_NTC_THERMISTOR_CURVE_GENERIC_B3950)
  return presets::kGeneric10kB3950;
#elif defined(CONFIG_NTC_THERMISTOR_CURVE_R25_B)
  Curve c{};
  c.r0_ohm = static_cast<uint32_t>(CONFIG_NTC_THERMISTOR_R25_OHM);
  c.b_kelvin = static_cast<uint32_t>(CONFIG_NTC_THERMISTOR_B_KELVIN);
  return c;
#else
  return Curve{};
#endif
}

} // namespace detail

/**
 * @brief The configuration menuconfig selected.
 * @return `Config{}` with the curve, channel, resolution, series resistor,
 * topology, averaging and temperature window from Kconfig; with no curve option
 * set, the empty curve of `Config{}`.
 */
[[nodiscard]] constexpr Config KconfigDefaults() noexcept {
  Config c{};
  c.curve = detail::KconfigCurve();
  c.adc_channel = static_cast<uint8_t>(CONFIG_NTC_THERMISTOR_ADC_CHANNEL);
  c.adc_bits = static_cast<uint8_t>(CONFIG_NTC_THERMISTOR_ADC_BITS);
  c.series_ohm = static_cast<uint32_t>(CONFIG_NTC_THERMISTOR_SERIES_OHM);
  c.sample_count = static_cast<uint8_t>(CONFIG_NTC_THERMISTOR_SAMPLES);
#ifdef CONFIG_NTC_THERMISTOR_HIGH_SIDE
  c.topology = Topology::NtcHighSide;
#endif
#if defined(CONFIG_NTC_THERMISTOR_MIN_TEMPERATURE_C) &&                                            \
    defined(CONFIG_NTC_THERMISTOR_MAX_TEMPERATURE_C)
  c.min_temperature_mc = static_cast<int32_t>(CONFIG_NTC_THERMISTOR_MIN_TEMPERATURE_C) * 1000;
  c.max_temperature_mc = static_cast<int32_t>(CONFIG_NTC_THERMISTOR_MAX_TEMPERATURE_C) * 1000;
#endif
  return c;
}

} // namespace ntc_thermistor

#endif // NTC_THERMISTOR_KCONFIG_HPP
