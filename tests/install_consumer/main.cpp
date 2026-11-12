/**
 * @file main.cpp
 * @brief Builds the driver from the installed package only: headers, version
 * header and .ipp.
 */
#include <cstdint>

#include "ntc_thermistor.hpp"
#include "ntc_thermistor_presets.hpp"

namespace {

// An ADC that always reads half scale: enough to instantiate the whole driver
// template.
class HalfScalePort : public ntc_thermistor::AdcInterface<HalfScalePort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  bool ReadRaw(uint8_t channel, uint32_t& code) noexcept {
    (void)channel;
    code = 2048;
    return true;
  }
};

} // namespace

int main() {
  HalfScalePort port;
  const ntc_thermistor::Config config{.curve = ntc_thermistor::presets::kTdkNtcg163jf103ft1s};
  ntc_thermistor::NtcThermistor<HalfScalePort> sensor(port, config);
  // Half scale on a 10 kOhm divider is the thermistor's R25: exactly 25 degC.
  const auto init = sensor.EnsureInitialized();
  const auto reading = sensor.Read();
  return (init.ok() && reading.ok() && reading.value.temperature_mc == 25'000 &&
          ntc_thermistor::GetDriverVersion()[0] != '\0')
             ? 0
             : 1;
}
