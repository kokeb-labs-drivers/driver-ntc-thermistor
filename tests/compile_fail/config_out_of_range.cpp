/**
 * @file config_out_of_range.cpp
 * @brief Must NOT compile: a constant `Config` out of range is refused at build
 * time by `Validate()` in a `static_assert`.
 *
 * EXPECT: Validate
 */
#include "ntc_thermistor_presets.hpp"
#include "ntc_thermistor_types.hpp"

constexpr ntc_thermistor::Config kBad{.curve = ntc_thermistor::presets::kGeneric10kB3950,
                                      .adc_bits = 30,
                                      .min_temperature_mc = -40'000,
                                      .max_temperature_mc = 125'000};
static_assert(ntc_thermistor::Validate(kBad) == ntc_thermistor::Error::Ok,
              "Validate refuses 30 bits");

int main() {
  return 0;
}
