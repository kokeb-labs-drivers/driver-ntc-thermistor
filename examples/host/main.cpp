/**
 * @file main.cpp
 * @brief Host example: the driver reads three different thermistors with the
 * same code, then three wiring faults and both divider topologies, against a
 * simulated divider and converter.
 *
 * The part is data. The first part uses the example preset (a table), the
 * second the generic 10 kOhm / B 3950 preset (the beta equation), the third is
 * declared right here (100 kOhm, B 4250). The program exits with 0 only when
 * every reading is within the driver's own uncertainty bound of the simulated
 * temperature and every fault is named, so it doubles as a test in CTest.
 */
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "host_ntc_thermistor_port.hpp"
#include "ntc_thermistor.hpp"
#include "ntc_thermistor_presets.hpp"

namespace {

using ntc_thermistor::Config;
using ntc_thermistor::Error;
using ntc_thermistor::Topology;
using ntc_thermistor_test::Divider;
using ntc_thermistor_test::Part;

/// The number of readings that left their bound.
int& Failures() {
  static int count = 0;
  return count;
}

/// Milli-degrees as "-12.345", without floating point.
void PrintMilli(int32_t milli) {
  const int64_t a = (milli < 0) ? -static_cast<int64_t>(milli) : static_cast<int64_t>(milli);
  (void)std::printf("%s%lld.%03lld", (milli < 0) ? "-" : "", static_cast<long long>(a / 1000),
                    static_cast<long long>(a % 1000));
}

/// A 100 kOhm / B 4250 part, declared in this file only: no preset, no driver
/// change.
constexpr Config UserPart() {
  Config c{};
  c.curve.r0_ohm = 100'000;
  c.curve.b_kelvin = 4250;
  c.curve.b_t2_mc = 85'000;
  c.series_ohm = 100'000;
  c.min_temperature_mc = -40'000;
  c.max_temperature_mc = 125'000;
  return c;
}

/// Sweep one part: set the simulated temperature, read, compare.
void Sweep(const char* title, const Config& config, const Part& simulated, const Divider& divider) {
  HostNtcThermistorPort port;
  ntc_thermistor::NtcThermistor<HostNtcThermistorPort> sensor(port, config);
  if (!sensor.EnsureInitialized().ok()) {
    (void)std::printf("%s: EnsureInitialized failed\n", title);
    ++Failures();
    return;
  }
  (void)std::printf("%s\n", title);
  for (const int celsius : {-20, 0, 25, 60, 100}) {
    port.SetTemperature(simulated, divider, static_cast<long double>(celsius));
    const auto r = sensor.Read();
    if (!r.ok()) {
      (void)std::printf("  %4d C: %s\n", celsius, ntc_thermistor::ToString(r.error));
      ++Failures();
      continue;
    }
    (void)std::printf("  %4d C: read ", celsius);
    PrintMilli(r.value.temperature_mc);
    (void)std::printf(" C +- ");
    PrintMilli(static_cast<int32_t>(r.value.uncertainty_mc));
    (void)std::printf(" K, R %u ohm, code %u\n", r.value.resistance_ohm, r.value.code);
    const int64_t error =
        static_cast<int64_t>(r.value.temperature_mc) - (static_cast<int64_t>(celsius) * 1000);
    const int64_t bound =
        static_cast<int64_t>(r.value.uncertainty_mc) + 300; // the preset curves are approximations
    if ((error > bound) || (error < -bound)) {
      ++Failures();
    }
  }
}

/// Force a code and print what the driver says.
void Fault(const char* what, const Config& config, uint32_t code, Error expect) {
  HostNtcThermistorPort port;
  ntc_thermistor::NtcThermistor<HostNtcThermistorPort> sensor(port, config);
  if (!sensor.EnsureInitialized().ok()) {
    ++Failures();
    return;
  }
  port.SetCode(code);
  const auto r = sensor.Read();
  (void)std::printf("  %s (code %u): %s\n", what, code, ntc_thermistor::ToString(r.error));
  Failures() += (r.error == expect) ? 0 : 1;
}

} // namespace

int main() {
  (void)std::printf("NTC thermistor driver %s on the host\n", ntc_thermistor::GetDriverVersion());
  const auto& parts = ntc_thermistor_test::Parts();
  Divider divider;

  // 1. The example preset: a 39-row table; the simulation is the Steinhart-Hart
  // fit of the same part.
  const Config table_part{.curve = ntc_thermistor::presets::kTdkNtcg163jf103ft1s};
  Sweep("Table part (preset, low-side divider, 12 bit)", table_part, parts[4], divider);

  // 2. The generic preset: the beta equation, with the window the application
  // declares.
  Config beta_part{.curve = ntc_thermistor::presets::kGeneric10kB3950};
  beta_part.min_temperature_mc = -40'000;
  beta_part.max_temperature_mc = 125'000;
  Sweep("Beta part (generic 10 kOhm / B 3950)", beta_part, parts[0], divider);

  // 3. A part declared here: 100 kOhm, B 4250.
  Divider big = divider;
  big.series_ohm = 100'000;
  Sweep("User part (100 kOhm / B 4250, declared in main.cpp)", UserPart(), parts[1], big);

  // 4. The same table part with the thermistor on the supply side.
  Config high = table_part;
  high.topology = Topology::NtcHighSide;
  Divider high_divider = divider;
  high_divider.topology = Topology::NtcHighSide;
  Sweep("Table part, high-side divider", high, parts[4], high_divider);

  // 5. Wiring faults and a bad port.
  (void)std::printf("Faults\n");
  Fault("thermistor cut", table_part, 4095, Error::OpenCircuit);
  Fault("thermistor shorted", table_part, 0, Error::ShortCircuit);
  Fault("port returns 13 bits for a 12-bit ADC", table_part, 8191, Error::CodeOutOfScale);
  Fault("colder than the table", table_part, 4000, Error::BelowRange);

  (void)std::printf("%s\n", (Failures() == 0) ? "all readings within their bounds" : "FAILED");
  return (Failures() == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
