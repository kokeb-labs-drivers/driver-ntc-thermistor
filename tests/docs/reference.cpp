/**
 * @file reference.cpp
 * @brief The Hardware and Conversion pages' examples, compiled by the host
 * build (never run).
 */
#include <cstdint>

#include "ntc_thermistor.hpp"
#include "ntc_thermistor_presets.hpp"

// --8<-- [start:self-heating]
// How much a 3.3 V divider warms the part: P in microwatts, divided by the
// part's dissipation constant (about 1 mW/K for a chip part on a board: read
// your datasheet).
uint32_t SelfHeatingMillikelvin(uint32_t r_ntc_ohm, uint32_t dissipation_uw_per_k) {
  const ntc_thermistor::Config c{.curve = ntc_thermistor::presets::kTdkNtcg163jf103ft1s,
                                 .adc_vref_mv = 3300,
                                 .divider_supply_mv = 3300};
  const uint32_t microwatts = ntc_thermistor::SelfHeatingMicrowatts(c, r_ntc_ohm);
  return (microwatts * 1000U) / dissipation_uw_per_k; // 272 uW at 25 degC and 1000 uW/K: 272 mK
}
// --8<-- [end:self-heating]

// --8<-- [start:units]
// Milli-kelvin and milli-fahrenheit, saturating; no floating point anywhere.
int32_t ToFahrenheit(int32_t milli_celsius) {
  return ntc_thermistor::ToMilliFahrenheit(milli_celsius);
}
// --8<-- [end:units]
