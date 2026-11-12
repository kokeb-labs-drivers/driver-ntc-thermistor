#ifndef HOST_STUB_DRIVER_GPIO_H
#define HOST_STUB_DRIVER_GPIO_H

#include <cstdint>

using esp_err_t = int;
constexpr esp_err_t ESP_OK = 0;
constexpr esp_err_t ESP_FAIL = -1;
using gpio_num_t = int;
enum gpio_mode_t { GPIO_MODE_OUTPUT = 1 };

struct gpio_config_t {
  uint64_t pin_bit_mask;
  gpio_mode_t mode;
};

/// The last level written by `gpio_set_level()`, for the host check.
inline uint32_t g_stub_gpio_level = 0U;
/// How many times `gpio_config()` succeeded: the pin is configured only then.
inline int g_stub_gpio_configured = 0;
/// Makes `gpio_config()` fail, to check that a failed start-up can be retried.
inline bool g_stub_gpio_config_fails = false;

inline esp_err_t gpio_set_level(gpio_num_t /*pin*/, uint32_t level) {
  g_stub_gpio_level = level;
  return ESP_OK;
}
inline esp_err_t gpio_config(const gpio_config_t* /*config*/) {
  if (g_stub_gpio_config_fails) {
    return ESP_FAIL;
  }
  ++g_stub_gpio_configured;
  return ESP_OK;
}

#endif // HOST_STUB_DRIVER_GPIO_H
