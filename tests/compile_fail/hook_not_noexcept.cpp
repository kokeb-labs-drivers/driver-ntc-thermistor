/**
 * @file hook_not_noexcept.cpp
 * @brief Must NOT compile: a ReadRaw hook that is not noexcept breaks the port
 * contract, so the port concept rejects it (handbook KB-CPP20-006).
 *
 * EXPECT: AdcPort
 */
#include <cstddef>
#include <cstdint>

#include "ntc_thermistor.hpp"
#include "ntc_thermistor_presets.hpp"

class ThrowingPort : public ntc_thermistor::AdcInterface<ThrowingPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  bool ReadRaw(uint8_t channel, uint32_t& code) {
    code = 0U;
    return channel == 0U;
  }
  void DelayUs(uint32_t us) noexcept {
    (void)us;
  }
};

int main() {
  ThrowingPort port;
  ntc_thermistor::Config c{};
  c.curve = ntc_thermistor::presets::kGeneric10kB3950;
  c.min_temperature_mc = -40'000;
  c.max_temperature_mc = 125'000;
  ntc_thermistor::NtcThermistor<ThrowingPort> sensor(port, c);
  return static_cast<int>(sensor.EnsureInitialized().error);
}
