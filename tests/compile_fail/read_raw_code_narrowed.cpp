/**
 * @file read_raw_code_narrowed.cpp
 * @brief Must NOT compile: a `ReadRaw` that takes the code as `uint16_t&` would
 * truncate a 24-bit count, so the port concept rejects it.
 *
 * EXPECT: AdcPort
 */
#include <cstdint>

#include "ntc_thermistor.hpp"
#include "ntc_thermistor_presets.hpp"

class NarrowCodePort : public ntc_thermistor::AdcInterface<NarrowCodePort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  bool ReadRaw(uint8_t channel, uint16_t& code) noexcept {
    code = channel;
    return true;
  }
};

int main() {
  NarrowCodePort port;
  ntc_thermistor::Config c{};
  c.curve = ntc_thermistor::presets::kGeneric10kB3950;
  c.min_temperature_mc = -40'000;
  c.max_temperature_mc = 125'000;
  ntc_thermistor::NtcThermistor<NarrowCodePort> sensor(port, c);
  return static_cast<int>(sensor.EnsureInitialized().error);
}
