/**
 * @file my_port.hpp
 * @brief The ADC port shown on the Quickstart and Porting pages, compiled by
 * the host build.
 *
 * Every platform call goes through the `platform_*` functions declared below; a
 * real port replaces them with its SDK (ESP-IDF oneshot ADC, STM32 HAL, Zephyr,
 * Linux IIO).
 */
#ifndef NTC_THERMISTOR_MY_PORT_HPP
#define NTC_THERMISTOR_MY_PORT_HPP

#include <cstdint>

#include "ntc_thermistor_adc_interface.hpp"

// Your SDK's calls. Declared only: the docs build checks that the port
// compiles.
bool platform_adc_init() noexcept; // 12-bit, input range covers the supply
bool platform_adc_read_raw(uint8_t channel,
                           uint32_t& code) noexcept; // one raw conversion, 0 to 4095
void platform_delay_us(uint32_t us) noexcept;        // busy-wait or sleep

// --8<-- [start:port]
class MyPort : public ntc_thermistor::AdcInterface<MyPort> {
public:
  // Bring the ADC up once; later calls return true at once.
  bool EnsureInitialized() noexcept {
    if (!ready_) {
      ready_ = platform_adc_init();
    }
    return ready_;
  }

  // One conversion of `channel`: the raw count, as many bits as
  // Config::adc_bits says. No averaging.
  bool ReadRaw(uint8_t channel, uint32_t& code) noexcept {
    return platform_adc_read_raw(channel, code);
  }

  // Optional: wait at least `us` microseconds (sample_delay_us, settle_us).
  void DelayUs(uint32_t us) noexcept {
    platform_delay_us(us);
  }

private:
  bool ready_ = false;
};
// --8<-- [end:port]

#endif // NTC_THERMISTOR_MY_PORT_HPP
