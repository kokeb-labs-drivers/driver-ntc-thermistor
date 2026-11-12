/**
 * @file not_a_port.cpp
 * @brief Must NOT compile: the driver is constrained by the port concept.
 *
 * EXPECT: Port
 */
#include "ntc_thermistor.hpp"
#include "ntc_thermistor_presets.hpp"

struct NotAPort {
  bool EnsureInitialized() noexcept {
    return true;
  }
};

int main() {
  NotAPort port;
  ntc_thermistor::Config c{};
  c.curve = ntc_thermistor::presets::kGeneric10kB3950;
  c.min_temperature_mc = -40'000;
  c.max_temperature_mc = 125'000;
  ntc_thermistor::NtcThermistor<NotAPort> sensor(port, c);
  return static_cast<int>(sensor.EnsureInitialized().error);
}
