/**
 * @file ports.cpp
 * @brief The Porting page's examples, compiled by the host build (never run).
 */
#include <cstdint>

#include "ntc_thermistor_adc_interface.hpp"

// Your SDK's calls.
bool platform_adc_init() noexcept;
bool platform_adc_read_raw(uint8_t channel, uint32_t& code) noexcept;
void platform_delay_us(uint32_t us) noexcept;
void platform_gpio_write(int pin, bool level) noexcept;

// --8<-- [start:full-port]
// A port with both optional hooks: the divider is powered from a GPIO only
// while sampling.
class FullPort : public ntc_thermistor::AdcInterface<FullPort> {
public:
  bool EnsureInitialized() noexcept {
    return platform_adc_init();
  }
  bool ReadRaw(uint8_t channel, uint32_t& code) noexcept {
    return platform_adc_read_raw(channel, code);
  }
  void DelayUs(uint32_t us) noexcept {
    platform_delay_us(us);
  }
  // true when the pin was driven; the driver treats false as Bus and still
  // switches the divider off.
  bool SetBias(bool on) noexcept {
    platform_gpio_write(kBiasPin, on);
    return true;
  }

private:
  static constexpr int kBiasPin = 4;
};
// --8<-- [end:full-port]

// --8<-- [start:millivolt-port]
// A port that only has calibrated millivolts: map them back to a code of
// `adc_bits` bits.
bool platform_adc_read_mv(uint8_t channel, uint32_t& millivolts) noexcept;

class MillivoltPort : public ntc_thermistor::AdcInterface<MillivoltPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  bool ReadRaw(uint8_t channel, uint32_t& code) noexcept {
    uint32_t mv = 0;
    if (!platform_adc_read_mv(channel, mv)) {
      return false;
    }
    code = (mv * 4096U) / 3300U; // 12 bits over the 3.3 V full scale: loses under one code
    code = (code > 4095U) ? 4095U : code;
    return true;
  }
};
// --8<-- [end:millivolt-port]
