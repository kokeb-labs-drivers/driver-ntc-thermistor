/**
 * @file read_raw_returns_void.cpp
 * @brief Must NOT compile: a `ReadRaw` that returns `void` cannot report an ADC
 * failure, so the port concept rejects it.
 *
 * EXPECT: AdcPort
 */
#include <cstdint>

#include "ntc_thermistor.hpp"
#include "ntc_thermistor_presets.hpp"

class VoidReadPort : public ntc_thermistor::AdcInterface<VoidReadPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  void ReadRaw(uint8_t channel, uint32_t& code) noexcept {
    code = channel;
  }
};

int main() {
  VoidReadPort port;
  ntc_thermistor::Config c{};
  c.curve = ntc_thermistor::presets::kGeneric10kB3950;
  c.min_temperature_mc = -40'000;
  c.max_temperature_mc = 125'000;
  ntc_thermistor::NtcThermistor<VoidReadPort> sensor(port, c);
  return static_cast<int>(sensor.EnsureInitialized().error);
}
