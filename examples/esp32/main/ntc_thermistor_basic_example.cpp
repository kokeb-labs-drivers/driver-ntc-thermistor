/**
 * @file ntc_thermistor_basic_example.cpp
 * @brief Read an NTC thermistor divider from the menuconfig values and log the
 * temperature once a second.
 */
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp32_ntc_thermistor_port.hpp"
#include "ntc_thermistor.hpp"
#include "ntc_thermistor_kconfig.hpp"

namespace {
constexpr const char* kTag = "ntc_thermistor_example";
}

extern "C" void app_main(void) {
  ntc_thermistor::Config config = ntc_thermistor::KconfigDefaults();
  Esp32NtcThermistorPins pins;
  pins.bitwidth = static_cast<adc_bitwidth_t>(config.adc_bits); // the port and the driver agree
  static Esp32NtcThermistorPort port(pins);
  // The ESP32 ADC is not ratiometric: say what the divider is powered from and
  // what full scale the chosen attenuation gives (see
  // esp32_ntc_thermistor_port.hpp). Adjust both for your board.
  config.divider_supply_mv = 3300;
  config.adc_vref_mv = 3100;
  static ntc_thermistor::NtcThermistor<Esp32NtcThermistorPort> sensor(port, config); // no I/O

  ESP_LOGI(kTag, "NTC thermistor driver %s", ntc_thermistor::GetDriverVersion());
  if (const auto init = sensor.EnsureInitialized(); !init) {
    ESP_LOGE(kTag, "EnsureInitialized failed: %s", ntc_thermistor::ToString(init.error));
    return;
  }
  while (true) {
    const auto r = sensor.Read();
    if (r) {
      ESP_LOGI(kTag, "%ld mC +- %lu mK, %lu ohm", static_cast<long>(r.value.temperature_mc),
               static_cast<unsigned long>(r.value.uncertainty_mc),
               static_cast<unsigned long>(r.value.resistance_ohm));
    } else {
      ESP_LOGW(kTag, "read failed: %s", ntc_thermistor::ToString(r.error));
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
