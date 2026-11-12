/**
 * @file ntc_thermistor_presets.hpp
 * @brief Example curves as data. The driver includes this header nowhere: a
 * preset is a convenience, and a part is added by writing a `Curve`, never by
 * changing the driver.
 * @copyright Copyright (c) 2026 The driver-ntc-thermistor authors.
 * SPDX-License-Identifier: MIT
 *
 * @details
 * `presets::kTdkNtcg163jf103ft1s` is the worked example of the documentation: a
 * 10 kOhm 0603 chip thermistor with a 39-row table. Its rows are a secondary
 * listing of the vendor's nominal column and are **unverified until the
 * datasheet is supplied** (docs/presets.md, docs/worked-example.md).
 * `presets::kGeneric10kB3950` holds the typical values of the common 10 kOhm /
 * 3950 K class: there is no single datasheet for it, so read yours and
 * override. Neither carries a valid window: a `Config` still declares
 * `min_temperature_mc` and `max_temperature_mc` (beta mode requires them).
 *
 * `Config{.curve = presets::kTdkNtcg163jf103ft1s}` is a valid table-mode
 * configuration; `Config{.curve = presets::kGeneric10kB3950,
 * .min_temperature_mc = ..., .max_temperature_mc =
 * ...}` a valid beta-mode one.
 */
#ifndef NTC_THERMISTOR_PRESETS_HPP
#define NTC_THERMISTOR_PRESETS_HPP

#include <array>
#include <cstdint>

#include "ntc_thermistor_types.hpp"

/** @brief Example curves; see `ntc_thermistor_presets.hpp`. */
namespace ntc_thermistor::presets {

/**
 * @brief The 39 nominal rows of the TDK NTCG163JF103FT1S, -40 to +150 degC in 5
 * degC steps.
 *
 * Source: a secondary listing of the nominal column of the sheet's
 * resistance-temperature table, to be confirmed against the PDF (its sha256
 * goes into `docs/datasheets.yml`). **Unverified until the datasheet is
 * supplied.**
 */
inline constexpr std::array<TablePoint, 39> kTdkNtcg163jf103ft1sRows{{
    {.resistance_ohm = 188'500, .temperature_mc = -40'000},
    {.resistance_ohm = 144'300, .temperature_mc = -35'000},
    {.resistance_ohm = 111'300, .temperature_mc = -30'000},
    {.resistance_ohm = 86'600, .temperature_mc = -25'000},
    {.resistance_ohm = 67'800, .temperature_mc = -20'000},
    {.resistance_ohm = 53'460, .temperature_mc = -15'000},
    {.resistance_ohm = 42'450, .temperature_mc = -10'000},
    {.resistance_ohm = 33'930, .temperature_mc = -5'000},
    {.resistance_ohm = 27'280, .temperature_mc = 0},
    {.resistance_ohm = 22'070, .temperature_mc = 5'000},
    {.resistance_ohm = 17'960, .temperature_mc = 10'000},
    {.resistance_ohm = 14'700, .temperature_mc = 15'000},
    {.resistance_ohm = 12'090, .temperature_mc = 20'000},
    {.resistance_ohm = 10'000, .temperature_mc = 25'000},
    {.resistance_ohm = 8'310, .temperature_mc = 30'000},
    {.resistance_ohm = 6'940, .temperature_mc = 35'000},
    {.resistance_ohm = 5'826, .temperature_mc = 40'000},
    {.resistance_ohm = 4'911, .temperature_mc = 45'000},
    {.resistance_ohm = 4'158, .temperature_mc = 50'000},
    {.resistance_ohm = 3'536, .temperature_mc = 55'000},
    {.resistance_ohm = 3'019, .temperature_mc = 60'000},
    {.resistance_ohm = 2'588, .temperature_mc = 65'000},
    {.resistance_ohm = 2'227, .temperature_mc = 70'000},
    {.resistance_ohm = 1'924, .temperature_mc = 75'000},
    {.resistance_ohm = 1'668, .temperature_mc = 80'000},
    {.resistance_ohm = 1'451, .temperature_mc = 85'000},
    {.resistance_ohm = 1'267, .temperature_mc = 90'000},
    {.resistance_ohm = 1'110, .temperature_mc = 95'000},
    {.resistance_ohm = 975, .temperature_mc = 100'000},
    {.resistance_ohm = 860, .temperature_mc = 105'000},
    {.resistance_ohm = 760, .temperature_mc = 110'000},
    {.resistance_ohm = 674, .temperature_mc = 115'000},
    {.resistance_ohm = 599, .temperature_mc = 120'000},
    {.resistance_ohm = 534, .temperature_mc = 125'000},
    {.resistance_ohm = 478, .temperature_mc = 130'000},
    {.resistance_ohm = 428, .temperature_mc = 135'000},
    {.resistance_ohm = 385, .temperature_mc = 140'000},
    {.resistance_ohm = 346, .temperature_mc = 145'000},
    {.resistance_ohm = 313, .temperature_mc = 150'000},
}};

/**
 * @brief TDK NTCG163JF103FT1S: R25 10 kOhm, B25/85 3435 K, table mode
 * (**unverified until the datasheet is supplied**). Tolerances 1 % on R25 and 1
 * % on B are the product page's, also unverified.
 */
inline constexpr Curve kTdkNtcg163jf103ft1s{
    .table = kTdkNtcg163jf103ft1sRows,
    .r0_ohm = 10'000,
    .ref_temperature_mc = 25'000,
    .b_kelvin = 3'435,
    .b_t2_mc = 85'000,
    .r_tol_ppm = 10'000,
    .b_tol_ppm = 10'000,
};

/**
 * @brief The common 10 kOhm / B 3950 class (B25/50), beta mode: typical values
 * of probes and beads, not a datasheet claim. No tolerance is declared; read
 * your part's sheet and override.
 */
inline constexpr Curve kGeneric10kB3950{
    .table = {},
    .r0_ohm = 10'000,
    .ref_temperature_mc = 25'000,
    .b_kelvin = 3'950,
    .b_t2_mc = 50'000,
    .r_tol_ppm = 0,
    .b_tol_ppm = 0,
};

} // namespace ntc_thermistor::presets

#endif // NTC_THERMISTOR_PRESETS_HPP
