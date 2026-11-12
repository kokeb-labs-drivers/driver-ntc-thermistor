/**
 * @file ntc_thermistor.hpp
 * @brief NTC thermistor driver: the one header to include.
 * @copyright Copyright (c) 2026 The driver-ntc-thermistor authors.
 * SPDX-License-Identifier: MIT
 *
 * @details
 * `ntc_thermistor::NtcThermistor<Port>` reads any NTC thermistor wired in a
 * resistor divider to an ADC channel of your port (see
 * `ntc_thermistor_adc_interface.hpp`) and returns the temperature in
 * milli-degrees Celsius. The part is data (`Config::curve`); the driver knows
 * none. No platform code, no heap, no exceptions, no lock, no floating point.
 */
#ifndef NTC_THERMISTOR_HPP
#define NTC_THERMISTOR_HPP

#include <cstdint>

#include "ntc_thermistor_adc_interface.hpp"
#include "ntc_thermistor_conversion.hpp"
#include "ntc_thermistor_detail.hpp"
#include "ntc_thermistor_types.hpp"
#include "ntc_thermistor_version.h"

namespace ntc_thermistor {

namespace detail {

// Optional hooks: a `Declares*` concept only tests the name, the `Has*` concept
// the exact signature, `noexcept` and the result type; the driver rejects a
// hook that is declared but does not match (handbook KB-CPP20-007).

/// The port declares something called `SetBias`.
template <typename Port>
concept DeclaresSetBias = requires(Port& port) { port.SetBias(true); };

/// The port powers the divider: `bool SetBias(bool on) noexcept`.
template <typename Port>
concept HasSetBias = requires { ExactHook<Port, bool(bool) noexcept>::Take(&Port::SetBias); };

/// The port declares something called `DelayUs`.
template <typename Port>
concept DeclaresDelayUs = requires(Port& port) { port.DelayUs(1_u32); };

/// The port waits: `void DelayUs(uint32_t us) noexcept`.
template <typename Port>
concept HasDelayUs = requires { ExactHook<Port, void(uint32_t) noexcept>::Take(&Port::DelayUs); };

} // namespace detail

/**
 * @defgroup ntc_thermistor_driver NTC thermistor driver
 * @brief The driver class.
 * @{
 */

/**
 * @brief Driver for one NTC thermistor in a resistor divider on an ADC port.
 *
 * Lifecycle: construct with the configuration (no I/O), then
 * `EnsureInitialized()`, the one way to start; use; replace the configuration
 * with `SetConfig()`. Every call that touches the port returns
 * `Error::NotInitialized` until `EnsureInitialized()` has succeeded; no call
 * starts it as a side effect.
 *
 * Optional hooks: `void DelayUs(uint32_t)` (the wait between samples,
 * `sample_delay_us`, and the settling time, `settle_us`; a `Config` that needs
 * a wait the port cannot make is `InvalidArgument`) and `bool SetBias(bool)`
 * (powers the divider only while sampling).
 *
 * @tparam Port Your class derived from `AdcInterface<Port>`.
 */
template <AdcPort Port>
class NtcThermistor {
  static_assert(!detail::DeclaresSetBias<Port> || detail::HasSetBias<Port>,
                "ntc_thermistor: the optional SetBias hook must be `bool "
                "SetBias(bool on) "
                "noexcept`, so that a failed pin write is reported");
  static_assert(!detail::DeclaresDelayUs<Port> || detail::HasDelayUs<Port>,
                "ntc_thermistor: the DelayUs hook must be `void "
                "DelayUs(uint32_t us) noexcept`, "
                "so that no wait is truncated");

public:
  /**
   * @brief Bind the driver to a port and store the configuration. No I/O,
   * cannot fail.
   * @param port The port; it must outlive the driver.
   * @param config The thermistor and its divider; checked by
   * `EnsureInitialized()` before any I/O. The curve's table, if any, must
   * outlive the driver.
   */
  NtcThermistor(Port& port, const Config& config) noexcept : port_(port), config_(config) {}

  /** @brief One driver object per thermistor. */
  NtcThermistor(const NtcThermistor&) = delete;
  /** @brief One driver object per thermistor. */
  NtcThermistor& operator=(const NtcThermistor&) = delete;

  /**
   * @brief Start the driver, once: the only way to initialise it.
   *
   * Checks the configuration (and that the port has the hooks it needs), brings
   * the ADC up and, when the port has `SetBias()`, switches the divider off so
   * that the first reading starts from a known state. After it succeeds, later
   * calls return `Ok` at once with no I/O. After it fails, the driver stays
   * uninitialised and the next call starts again.
   *
   * @return `Ok`, `InvalidArgument` (the configuration fails `Validate()` or
   * needs a hook the port lacks; no port call) or `Bus`.
   */
  Result<void> EnsureInitialized() noexcept;

  /** @return true after a successful `EnsureInitialized()`. No I/O. */
  [[nodiscard]] bool IsInitialized() const noexcept {
    return initialized_;
  }

  /**
   * @brief Read the temperature: bias on, settle, `sample_count` conversions,
   * bias off, convert.
   * @return The reading, or `NotInitialized`, `Bus` (an ADC read or the bias
   * switch failed; no retry), `CodeOutOfScale`, `Unstable`, `OpenCircuit`,
   * `ShortCircuit`, `BelowRange` or `AboveRange`.
   */
  [[nodiscard]] Result<Reading> Read() noexcept;

  /**
   * @brief Read the thermistor's resistance in ohms: the divider arithmetic
   * only, no curve and no window.
   * @return Ohms, or the errors of `Read()` that come before the curve.
   */
  [[nodiscard]] Result<uint32_t> ReadResistance() noexcept;

  /**
   * @brief One raw conversion, with no averaging and no conversion: bias on,
   * settle, one sample, bias off. A diagnostic: it is the code the port
   * returned, and it has no retry.
   * @return The code, or `NotInitialized`, `Bus` or `CodeOutOfScale` (the code
   * is still kept for `GetLastCode()`). It never starts anything and changes no
   * state but `GetLastCode()`.
   */
  [[nodiscard]] Result<uint32_t> ReadRawCode() noexcept;

  /**
   * @brief Replace the configuration. It is validated first; on failure the old
   * one stays.
   * @param config The new configuration; its table, if any, must outlive the
   * driver.
   * @return `Ok` or `InvalidArgument`. No port call.
   */
  Result<void> SetConfig(const Config& config) noexcept;

  /** @return The configuration in use. No I/O. */
  [[nodiscard]] const Config& GetConfig() const& noexcept {
    return config_;
  }
  /** @brief Not on a temporary driver: the reference would dangle (handbook
   * KB-CPP20-044). */
  void GetConfig() const&& = delete;

  /**
   * @return The last code the port returned, even one at or above scale; 0
   * before any sample. No I/O.
   */
  [[nodiscard]] uint32_t GetLastCode() const noexcept {
    return last_code_;
  }

private:
  static Error check(const Config& config) noexcept;
  Result<uint32_t> acquire(uint32_t count) noexcept;
  Result<uint32_t> acquireBiased(uint32_t count) noexcept;
  Result<uint32_t> sampleMean(uint32_t count) noexcept;

  Port& port_;
  Config config_;
  bool initialized_{false};
  uint32_t last_code_{0}; ///< The last code the port returned (`GetLastCode()`).
};

/** @} */

} // namespace ntc_thermistor

// codeql::misra_deviation_next_line(MD-CPP-009)
#include "../src/ntc_thermistor.ipp" // IWYU pragma: export

#endif // NTC_THERMISTOR_HPP
