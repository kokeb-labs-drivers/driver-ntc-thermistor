#ifndef HOST_STUB_ESP_ADC_ADC_ONESHOT_H
#define HOST_STUB_ESP_ADC_ADC_ONESHOT_H

#include "driver/gpio.h"

enum adc_unit_t { ADC_UNIT_1 };
enum adc_atten_t { ADC_ATTEN_DB_12 };
enum adc_bitwidth_t {
  ADC_BITWIDTH_DEFAULT = 0,
  ADC_BITWIDTH_9 = 9,
  ADC_BITWIDTH_10 = 10,
  ADC_BITWIDTH_11 = 11,
  ADC_BITWIDTH_12 = 12,
  ADC_BITWIDTH_13 = 13,
};
using adc_channel_t = int;
using adc_oneshot_unit_handle_t = void*;

struct adc_oneshot_unit_init_cfg_t {
  adc_unit_t unit_id;
};
struct adc_oneshot_chan_cfg_t {
  adc_atten_t atten;
  adc_bitwidth_t bitwidth;
};

/// The raw count every read returns.
inline constexpr int kStubRawCount = 750;
/// How many times a unit was created: a retried start-up must not create a second one.
inline int g_stub_units_created = 0;

inline esp_err_t adc_oneshot_new_unit(const adc_oneshot_unit_init_cfg_t* /*config*/,
                                      adc_oneshot_unit_handle_t* handle) {
  static int unit = 0;
  ++g_stub_units_created;
  *handle = &unit;
  return ESP_OK;
}
/// Only 12 bits, like the ESP32-C6: another width is refused.
inline esp_err_t adc_oneshot_config_channel(adc_oneshot_unit_handle_t /*handle*/,
                                            adc_channel_t /*channel*/,
                                            const adc_oneshot_chan_cfg_t* config) {
  return (config->bitwidth == ADC_BITWIDTH_12) ? ESP_OK : ESP_FAIL;
}
inline esp_err_t adc_oneshot_read(adc_oneshot_unit_handle_t /*handle*/, adc_channel_t /*channel*/,
                                  int* raw) {
  *raw = kStubRawCount;
  return ESP_OK;
}

#endif // HOST_STUB_ESP_ADC_ADC_ONESHOT_H
