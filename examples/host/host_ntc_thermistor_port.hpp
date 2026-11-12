/**
 * @file host_ntc_thermistor_port.hpp
 * @brief The host port: the driver's ADC hooks over a simulated thermistor and
 * divider, so the example runs on any PC.
 *
 * `ntc_thermistor_test::PortBase` (tests/mock_ntc_thermistor.hpp) supplies the
 * required hooks over the converter model the host tests use. This class adds
 * what a real port adds: `DelayUs()` (here it returns at once: the model
 * converts instantly) and a way to set the simulated temperature. A port for a
 * real ADC on a PC (a USB DAQ, an IIO device) implements the same hooks: see
 * docs/porting.md.
 */
#ifndef NTC_THERMISTOR_HOST_NTC_THERMISTOR_PORT_HPP
#define NTC_THERMISTOR_HOST_NTC_THERMISTOR_PORT_HPP

#include <cstdint>

#include "mock_ntc_thermistor.hpp"

/// The host port: a simulated thermistor behind a simulated divider and
/// converter.
class HostNtcThermistorPort : public ntc_thermistor_test::PortBase<HostNtcThermistorPort> {
public:
  /// The model needs no time; nothing sleeps.
  void DelayUs(uint32_t us) noexcept {
    (void)us;
  }

  /// Put the simulated `part` at `celsius`; the next conversions show what
  /// `divider` makes of it.
  void SetTemperature(const ntc_thermistor_test::Part& part,
                      const ntc_thermistor_test::Divider& divider, long double celsius) {
    state.Fixed(ntc_thermistor_test::CodeFor(divider, ntc_thermistor_test::TrueR(part, celsius)));
  }

  /// Make the converter return `code`, whatever the thermistor does (a wiring
  /// fault, a bad port).
  void SetCode(uint32_t code) {
    state.Fixed(code);
  }
};

#endif // NTC_THERMISTOR_HOST_NTC_THERMISTOR_PORT_HPP
