/**
 * @file configuration.cpp
 * @brief The Configuration page's examples, compiled by the host build (never
 * run).
 */
#include <array>
#include <cstdint>

#include "ntc_thermistor.hpp"
#include "ntc_thermistor_presets.hpp"

// --8<-- [start:own-part]
// Your part is data: R25 and B from its datasheet, and the range it is rated
// for. No preset needed.
inline constexpr ntc_thermistor::Config kMyPart{
    .curve = {.r0_ohm = 4700,
              .b_kelvin = 3977,
              .b_t2_mc = 85'000,
              .r_tol_ppm = 10'000,
              .b_tol_ppm = 10'000},
    .series_ohm = 4700,
    .min_temperature_mc = -30'000,
    .max_temperature_mc = 100'000,
};
static_assert(ntc_thermistor::Validate(kMyPart) == ntc_thermistor::Error::Ok);
// --8<-- [end:own-part]

// --8<-- [start:steinhart-hart]
// Steinhart-Hart coefficients, in units of 1e-15 per kelvin: A = 0.8767633e-3,
// B = 2.527899e-4, C = 1.906681e-7. The window is the range the fit holds for;
// `curve_error_mc` is the fit's own error, added to the bound.
inline constexpr ntc_thermistor::Config kShPart{
    .curve = {.sh_a_e15 = 876'763'300'000,
              .sh_b_e15 = 252'789'900'000,
              .sh_c_e15 = 190'668'100,
              .curve_error_mc = 410},
    .min_temperature_mc = -40'000,
    .max_temperature_mc = 150'000,
};
static_assert(ntc_thermistor::Validate(kShPart) == ntc_thermistor::Error::Ok);
// --8<-- [end:steinhart-hart]

// --8<-- [start:table]
// A part with a resistance-temperature table: any 2 to 256 rows, any spacing,
// resistance falling as temperature rises. The rows must outlive the driver: a
// `static constexpr` array.
inline constexpr std::array<ntc_thermistor::TablePoint, 5> kRows{{
    {32'650, 0},
    {10'000, 25'000},
    {3'600, 50'000},
    {1'480, 75'000},
    {680, 100'000},
}};
inline constexpr ntc_thermistor::Config kTablePart{.curve = {.table = kRows, .r0_ohm = 10'000}};
static_assert(ntc_thermistor::Validate(kTablePart) == ntc_thermistor::Error::Ok);
// --8<-- [end:table]

// --8<-- [start:high-side]
// The thermistor to the supply and the series resistor to ground: the node
// rises as it heats.
inline constexpr ntc_thermistor::Config kHighSide{
    .curve = ntc_thermistor::presets::kTdkNtcg163jf103ft1s,
    .topology = ntc_thermistor::Topology::NtcHighSide};
// --8<-- [end:high-side]

// --8<-- [start:non-ratiometric]
// A 5 V divider read by an ADC with a 3.3 V reference: the driver needs both
// voltages. The node must stay inside the ADC's range: choose the series
// resistor so that 5 V * node share stays below 3.3 V.
inline constexpr ntc_thermistor::Config kFiveVolts{
    .curve = ntc_thermistor::presets::kTdkNtcg163jf103ft1s,
    .series_ohm = 20'000,
    .adc_vref_mv = 3300,
    .divider_supply_mv = 5000,
    .open_margin_codes = 4,
    .short_margin_codes = 4};
// --8<-- [end:non-ratiometric]

// --8<-- [start:averaging]
// 16 samples 50 microseconds apart, refuse a reading whose samples differ by
// more than 24 codes, and treat the last 8 codes at each rail as a wiring
// fault.
inline constexpr ntc_thermistor::Config kAveraged{
    .curve = ntc_thermistor::presets::kTdkNtcg163jf103ft1s,
    .sample_count = 16,
    .sample_delay_us = 50,
    .max_spread_codes = 24,
    .open_margin_codes = 8,
    .short_margin_codes = 8};
// --8<-- [end:averaging]

// --8<-- [start:kconfig]
#include "ntc_thermistor_kconfig.hpp"

// With ESP-IDF, menuconfig chooses the channel, bits, series resistor, curve
// and window.
inline const ntc_thermistor::Config kFromMenuconfig = ntc_thermistor::KconfigDefaults();
// --8<-- [end:kconfig]
