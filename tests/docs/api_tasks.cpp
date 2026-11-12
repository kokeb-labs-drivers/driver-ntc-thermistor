/**
 * @file api_tasks.cpp
 * @brief The API guide's examples, compiled by the host build (never run).
 */
#include <cstdint>

#include "my_port.hpp"
#include "ntc_thermistor.hpp"
#include "ntc_thermistor_presets.hpp"

namespace {
const ntc_thermistor::Config kConfig{.curve = ntc_thermistor::presets::kGeneric10kB3950,
                                     .min_temperature_mc = -40'000,
                                     .max_temperature_mc = 125'000};
} // namespace

// --8<-- [start:convert]
// The maths alone: a code the application already has, no port, no state.
int32_t TemperatureOfCode(uint32_t code) {
  const auto r = ntc_thermistor::Convert(kConfig, code);
  return r ? r.value.temperature_mc : INT32_MIN;
}
// --8<-- [end:convert]

// --8<-- [start:errors]
const char* Explain(ntc_thermistor::Error e) {
  switch (e) {
  case ntc_thermistor::Error::OpenCircuit:
    return "no thermistor: cut wire, missing part, or a divider that is not "
           "powered";
  case ntc_thermistor::Error::ShortCircuit:
    return "node at the other rail: shorted part or wire, or the divider "
           "supply collapsed";
  case ntc_thermistor::Error::BelowRange:
    return "colder than the curve or the window";
  case ntc_thermistor::Error::AboveRange:
    return "hotter than the curve or the window";
  case ntc_thermistor::Error::Unstable:
    return "the samples disagree: a floating or noisy input";
  case ntc_thermistor::Error::CodeOutOfScale:
    return "the port returns more bits than Config::adc_bits says";
  default:
    break;
  }
  return ntc_thermistor::ToString(e);
}
// --8<-- [end:errors]

// --8<-- [start:uncertainty]
// The reading is a value and a bound: the true temperature lies within it if
// the declared tolerances hold.
bool WithinOneKelvin(ntc_thermistor::NtcThermistor<MyPort>& sensor, int32_t& temperature_mc) {
  const auto t = sensor.Read();
  if (!t || (t.value.uncertainty_mc > 1000U)) {
    return false;
  }
  temperature_mc = t.value.temperature_mc;
  return true;
}
// --8<-- [end:uncertainty]

// --8<-- [start:replace-config]
// Change the offset at run time (a calibration): validated first, the old one
// stays on failure.
bool Calibrate(ntc_thermistor::NtcThermistor<MyPort>& sensor, int32_t offset_mc) {
  ntc_thermistor::Config c = sensor.GetConfig();
  c.offset_mc = offset_mc;
  return sensor.SetConfig(c).ok();
}
// --8<-- [end:replace-config]
