/**
 * @file delay_us_narrowed.cpp
 * @brief Must NOT compile: a DelayUs hook with a narrower parameter would
 * truncate long waits, so the driver rejects it instead of using it (handbook
 * KB-CPP20-007).
 *
 * EXPECT: DelayUs hook must be
 */
#include <cstdint>

#include "mock_ntc_thermistor.hpp"
#include "ntc_thermistor.hpp"
#include "ntc_thermistor_presets.hpp"

class ShortDelayPort : public ntc_thermistor_test::PortBase<ShortDelayPort> {
public:
  void DelayUs(uint16_t us) noexcept {
    (void)us;
  }
};

int main() {
  ShortDelayPort port;
  ntc_thermistor::Config c{};
  c.curve = ntc_thermistor::presets::kGeneric10kB3950;
  c.min_temperature_mc = -40'000;
  c.max_temperature_mc = 125'000;
  ntc_thermistor::NtcThermistor<ShortDelayPort> sensor(port, c);
  return static_cast<int>(sensor.EnsureInitialized().error);
}
