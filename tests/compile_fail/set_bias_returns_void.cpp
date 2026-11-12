/**
 * @file set_bias_returns_void.cpp
 * @brief Must NOT compile: an optional `SetBias` hook with the wrong signature
 * is rejected, not ignored.
 *
 * EXPECT: SetBias hook must be
 */
#include "mock_ntc_thermistor.hpp"
#include "ntc_thermistor.hpp"
#include "ntc_thermistor_presets.hpp"

class VoidBiasPort : public ntc_thermistor_test::PortBase<VoidBiasPort> {
public:
  void SetBias(bool on) noexcept {
    (void)on;
  }
};

int main() {
  VoidBiasPort port;
  ntc_thermistor::Config c{};
  c.curve = ntc_thermistor::presets::kGeneric10kB3950;
  c.min_temperature_mc = -40'000;
  c.max_temperature_mc = 125'000;
  ntc_thermistor::NtcThermistor<VoidBiasPort> sensor(port, c);
  return static_cast<int>(sensor.EnsureInitialized().error);
}
