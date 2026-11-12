/**
 * @file hook_narrowed_parameter.cpp
 * @brief Must NOT compile: a ReadRaw hook whose channel is a wider type accepts
 * what the contract does not, so the port concept rejects it (handbook
 * KB-CPP20-006).
 *
 * EXPECT: AdcPort
 */
#include <cstddef>
#include <cstdint>

#include "ntc_thermistor.hpp"
#include "ntc_thermistor_presets.hpp"

class NarrowPort : public ntc_thermistor::AdcInterface<NarrowPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  bool ReadRaw(uint16_t channel, uint32_t& code) noexcept {
    code = 0U;
    return channel == 0U;
  }
  void DelayUs(uint32_t us) noexcept {
    (void)us;
  }
};

int main() {
  NarrowPort port;
  ntc_thermistor::Config c{};
  c.curve = ntc_thermistor::presets::kGeneric10kB3950;
  c.min_temperature_mc = -40'000;
  c.max_temperature_mc = 125'000;
  ntc_thermistor::NtcThermistor<NarrowPort> sensor(port, c);
  return static_cast<int>(sensor.EnsureInitialized().error);
}
