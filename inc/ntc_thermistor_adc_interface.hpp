// clang-format off: the table below must keep one row per line.
/**
 * @file ntc_thermistor_adc_interface.hpp
 * @brief The ADC port of the NTC thermistor driver: the CRTP base and the `AdcPort` concept.
 * @copyright Copyright (c) 2026 The driver-ntc-thermistor authors. SPDX-License-Identifier: MIT
 *
 * @details
 * A thermistor sits in a resistor divider whose node goes to an ADC channel of your MCU. The driver
 * calls hooks on a class you derive from `ntc_thermistor::AdcInterface<Self>`:
 *
 * | Hook | Contract |
 * | --- | --- |
 * | `bool EnsureInitialized() noexcept` | Bring the ADC up on the first call; `true` at once afterwards |
 * | `bool ReadRaw(uint8_t channel, uint32_t& code) noexcept` | One conversion of `channel`: the raw count, 0 to 2^bits - 1, `bits` = `Config::adc_bits`; no averaging, no volts; `false` on any ADC failure |
 * | `void DelayUs(uint32_t us) noexcept` | Optional: wait at least `us` microseconds; needed by `sample_delay_us` and `settle_us` |
 * | `bool SetBias(bool on) noexcept` | Optional: power the divider (from a GPIO) only while sampling; `false` when the pin could not be driven |
 *
 * The port returns the converter's raw count; the driver never configures the ADC (attenuation,
 * width, sampling time). A port that only has calibrated millivolts maps them back to a code of
 * `adc_bits` bits.
 */
// clang-format on
#ifndef NTC_THERMISTOR_ADC_INTERFACE_HPP
#define NTC_THERMISTOR_ADC_INTERFACE_HPP

#include <concepts>
#include <cstdint>

#include "ntc_thermistor_detail.hpp"

namespace ntc_thermistor {

/**
 * @brief CRTP base of the platform ADC port.
 *
 * The base declares no hook itself (a forwarding hook would turn a missing
 * override into infinite recursion); the driver checks your class with the
 * `AdcPort` concept.
 *
 * @tparam Derived Your port class.
 */
template <typename Derived>
class AdcInterface {
public:
  /** @brief Ports are not copied: the driver holds a reference. */
  AdcInterface(const AdcInterface&) = delete;
  /** @brief Ports are not copied: the driver holds a reference. */
  AdcInterface& operator=(const AdcInterface&) = delete;

protected:
  /** @brief Only the derived port is constructed. */
  AdcInterface() noexcept = default;
  /** @brief Destroyed through the derived port only. */
  ~AdcInterface() noexcept = default;
};

/**
 * @brief Satisfied by a class that derives from `AdcInterface<Port>` and
 * implements the required hooks with exactly the types above, `noexcept` and
 * returning `bool` (handbook KB-CPP20-006).
 * @tparam Port The port class.
 */
template <typename Port>
concept AdcPort = std::derived_from<Port, AdcInterface<Port>> && requires {
  detail::ExactHook<Port, bool() noexcept>::Take(&Port::EnsureInitialized);
  detail::ExactHook<Port, bool(uint8_t, uint32_t&) noexcept>::Take(&Port::ReadRaw);
};

} // namespace ntc_thermistor

#endif // NTC_THERMISTOR_ADC_INTERFACE_HPP
