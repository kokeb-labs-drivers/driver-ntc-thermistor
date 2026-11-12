/**
 * @file esp32_ntc_thermistor_port.hpp
 * @brief ESP-IDF port for the NTC thermistor driver: the one-shot ADC (raw
 * count), an optional GPIO that powers the divider, and esp_rom delays.
 */
#ifndef NTC_THERMISTOR_ESP32_NTC_THERMISTOR_PORT_HPP
#define NTC_THERMISTOR_ESP32_NTC_THERMISTOR_PORT_HPP

#include <cstdint>

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_rom_sys.h"

#include "ntc_thermistor.hpp"

/// Pins of the ESP-IDF port.
struct Esp32NtcThermistorPins {
  adc_unit_t unit = ADC_UNIT_1;
  adc_bitwidth_t bitwidth = ADC_BITWIDTH_12; ///< Must equal `Config::adc_bits`
                                             ///< (the chip must offer it).
  int bias_gpio = -1; ///< A GPIO that powers the divider (it drives the supply
                      ///< side); -1 when it is always powered.
};

/**
 * @brief The ADC port: the raw count of the divider node.
 *
 * The ESP32 ADC is not ratiometric against the 3.3 V supply: its full scale
 * depends on the attenuation. The raw count is returned as it is, so declare
 * the divider with `divider_supply_mv` (the supply) and `adc_vref_mv` (the full
 * scale of the chosen attenuation, from the chip's datasheet or a calibration
 * of your own board), as docs/configuration.md describes.
 * `Esp32NtcThermistorPins::bitwidth` must equal `Config::adc_bits`. Without a
 * bias GPIO `SetBias()` does nothing and succeeds, so `settle_us` only adds a
 * wait. A failed `EnsureInitialized()` can be called again.
 */
class Esp32NtcThermistorPort : public ntc_thermistor::AdcInterface<Esp32NtcThermistorPort> {
public:
  explicit Esp32NtcThermistorPort(const Esp32NtcThermistorPins& pins = {}) noexcept : pins_(pins) {}

  bool EnsureInitialized() noexcept {
    if (ready_) {
      return true;
    }
    // The widths the ESP-IDF one-shot driver names; a chip offers some of them (12 on the
    // ESP32-C6).
    if ((pins_.bitwidth < ADC_BITWIDTH_9) || (pins_.bitwidth > ADC_BITWIDTH_13)) {
      return false;
    }
    if (adc_ == nullptr) {
      adc_oneshot_unit_init_cfg_t unit{};
      unit.unit_id = pins_.unit;
      if (adc_oneshot_new_unit(&unit, &adc_) != ESP_OK) {
        adc_ = nullptr;
        return false;
      }
    }
    if ((pins_.bias_gpio >= 0) && !configureBias()) {
      return false; // not ready: the next call configures the pin again
    }
    ready_ = true;
    return true;
  }

  bool ReadRaw(uint8_t channel, uint32_t& code) noexcept {
    const auto ch = static_cast<adc_channel_t>(channel);
    if (configured_ != static_cast<int>(channel)) {
      adc_oneshot_chan_cfg_t cfg{};
      cfg.atten = ADC_ATTEN_DB_12;
      cfg.bitwidth = pins_.bitwidth;
      if (adc_oneshot_config_channel(adc_, ch, &cfg) != ESP_OK) {
        return false;
      }
      configured_ = static_cast<int>(channel);
    }
    int raw = 0;
    if (adc_oneshot_read(adc_, ch, &raw) != ESP_OK || raw < 0) {
      return false;
    }
    code = static_cast<uint32_t>(raw);
    return true;
  }

  void DelayUs(uint32_t us) noexcept {
    esp_rom_delay_us(us);
  }

  bool SetBias(bool on) noexcept {
    if (pins_.bias_gpio < 0) {
      return true; // no switch fitted: the divider is always powered, so on and
                   // off are no-ops
    }
    return gpio_set_level(static_cast<gpio_num_t>(pins_.bias_gpio), on ? 1U : 0U) == ESP_OK;
  }

private:
  /// The divider starts off: the level is set before the output is enabled.
  bool configureBias() noexcept {
    gpio_config_t io{};
    io.pin_bit_mask = 1ULL << static_cast<unsigned>(pins_.bias_gpio);
    io.mode = GPIO_MODE_OUTPUT;
    return (gpio_set_level(static_cast<gpio_num_t>(pins_.bias_gpio), 0) == ESP_OK) &&
           (gpio_config(&io) == ESP_OK);
  }

  Esp32NtcThermistorPins pins_;
  bool ready_ = false;
  adc_oneshot_unit_handle_t adc_ = nullptr;
  int configured_ = -1;
};

#endif // NTC_THERMISTOR_ESP32_NTC_THERMISTOR_PORT_HPP
