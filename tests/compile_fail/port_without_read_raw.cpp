/**
 * @file port_without_read_raw.cpp
 * @brief Must NOT compile: a port with no `ReadRaw` is not an `AdcPort`.
 *
 * EXPECT: AdcPort
 */
#include <cstdint>

#include "ntc_thermistor.hpp"
#include "ntc_thermistor_presets.hpp"

class NoReadPort : public ntc_thermistor::AdcInterface<NoReadPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
};

int main() {
  NoReadPort port;
  ntc_thermistor::Config c{};
  c.curve = ntc_thermistor::presets::kGeneric10kB3950;
  c.min_temperature_mc = -40'000;
  c.max_temperature_mc = 125'000;
  ntc_thermistor::NtcThermistor<NoReadPort> sensor(port, c);
  return static_cast<int>(sensor.EnsureInitialized().error);
}
