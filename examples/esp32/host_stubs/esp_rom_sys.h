#ifndef HOST_STUB_ESP_ROM_SYS_H
#define HOST_STUB_ESP_ROM_SYS_H

#include <cstdint>

/// The total of the waits, for the host check.
inline uint32_t g_stub_delay_us = 0U;

inline void esp_rom_delay_us(uint32_t us) {
  g_stub_delay_us += us;
}

#endif // HOST_STUB_ESP_ROM_SYS_H
