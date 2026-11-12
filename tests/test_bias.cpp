/**
 * @file test_bias.cpp
 * @brief The optional bias switch: the order of port calls, and the divider off
 * on every path.
 */
#include <string>

#include "mock_ntc_thermistor.hpp"
#include "ntc_thermistor.hpp"
#include "ntc_thermistor_presets.hpp"
#include "test_harness.hpp"

namespace {

using ntc_thermistor::Config;
using ntc_thermistor::Error;
using ntc_thermistor_test::BiasPort;
using ntc_thermistor_test::Op;

Config BiasConfig() {
  Config c{};
  c.curve = ntc_thermistor::presets::kGeneric10kB3950;
  c.min_temperature_mc = -40'000;
  c.max_temperature_mc = 125'000;
  c.sample_count = 3;
  c.sample_delay_us = 50;
  c.settle_us = 200;
  return c;
}

/// The operations of the log after the start-up, as a string of letters: b on,
/// B off, r read, d delay.
const char* const kUnused = "";

std::string Sequence(const BiasPort& port, std::size_t from) {
  std::string s;
  for (std::size_t i = from; i < port.state.log.size(); ++i) {
    const Op op = port.state.log[i].op;
    if (op == Op::BiasOn) {
      s += 'b';
    } else if (op == Op::BiasOff) {
      s += 'B';
    } else if (op == Op::Read) {
      s += 'r';
    } else if (op == Op::Delay) {
      s += 'd';
    } else {
      s += 'i';
    }
  }
  return s + kUnused;
}

TEST_CASE("Start-up switches the divider off; a read is on, settle, samples "
          "with delays, off") {
  BiasPort port;
  port.state.Fixed(2048);
  ntc_thermistor::NtcThermistor<BiasPort> sensor(port, BiasConfig());
  REQUIRE(sensor.EnsureInitialized().ok());
  CHECK(Sequence(port, 0) == "iB");
  const std::size_t before = port.state.log.size();
  REQUIRE(sensor.Read().ok());
  CHECK(Sequence(port, before) == "bdrdrdrB");
  CHECK_EQ(port.state.WaitedUs(), 300U);
}

TEST_CASE("The divider is switched off after a failed read, a bad code, "
          "Unstable and a range error") {
  struct Case {
    const char* what;
    void (*setup)(BiasPort&);
    Error expect;
  };
  const Case cases[] = {
      {"read fails", [](BiasPort& p) { p.state.fail_read_at = 1; }, Error::Bus},
      {"above scale", [](BiasPort& p) { p.state.codes = {2048, 9999, 2048}; },
       Error::CodeOutOfScale},
      {"unstable", [](BiasPort& p) { p.state.codes = {2000, 2100, 2000}; }, Error::Unstable},
      {"open circuit", [](BiasPort& p) { p.state.Fixed(4095); }, Error::OpenCircuit},
      {"cold", [](BiasPort& p) { p.state.Fixed(4000); }, Error::BelowRange},
  };
  for (const Case& c : cases) {
    BiasPort port;
    port.state.Fixed(2048);
    c.setup(port);
    Config cfg = BiasConfig();
    cfg.max_spread_codes = 20;
    ntc_thermistor::NtcThermistor<BiasPort> sensor(port, cfg);
    REQUIRE(sensor.EnsureInitialized().ok());
    const auto r = sensor.Read();
    CHECK_EQ(r.error, c.expect);
    CHECK_EQ(port.state.Count(Op::BiasOn), 1);
    CHECK_EQ(port.state.Count(Op::BiasOff),
             2); // the one at start-up and the one after the read
    CHECK(port.state.log.back().op == Op::BiasOff);
  }
}

TEST_CASE("A failed bias switch is Bus and leaves the divider off") {
  BiasPort port;
  port.state.Fixed(2048);
  port.state.fail_bias_on_at = 0;
  ntc_thermistor::NtcThermistor<BiasPort> sensor(port, BiasConfig());
  REQUIRE(sensor.EnsureInitialized().ok());
  const std::size_t before = port.state.log.size();
  CHECK_EQ(sensor.Read().error, Error::Bus);
  CHECK(Sequence(port, before) == "bB"); // no sample was taken
  // switching off fails after a good read: the reading is not returned
  BiasPort stuck;
  stuck.state.Fixed(2048);
  ntc_thermistor::NtcThermistor<BiasPort> s2(stuck, BiasConfig());
  stuck.state.bias_off_ok = false;
  CHECK_EQ(s2.EnsureInitialized().error,
           Error::Bus); // start-up cannot switch it off
  CHECK(!s2.IsInitialized());
  stuck.state.bias_off_ok = true;
  REQUIRE(s2.EnsureInitialized().ok());
  stuck.state.bias_off_ok = false;
  CHECK_EQ(s2.Read().error, Error::Bus);
  CHECK_EQ(s2.ReadResistance().error, Error::Bus);
  // a failed read and a failed switch-off: the read's error wins
  stuck.state.fail_read_at = stuck.state.reads;
  CHECK_EQ(s2.Read().error, Error::Bus);
}

TEST_CASE("ReadResistance uses the same bias sequence") {
  BiasPort port;
  port.state.Fixed(2048);
  ntc_thermistor::NtcThermistor<BiasPort> sensor(port, BiasConfig());
  REQUIRE(sensor.EnsureInitialized().ok());
  const std::size_t before = port.state.log.size();
  const auto r = sensor.ReadResistance();
  REQUIRE(r.ok());
  CHECK_EQ(r.value, 10'000U);
  CHECK(Sequence(port, before) == "bdrdrdrB");
}

TEST_CASE("settle_us 0 waits nothing between the switch and the first sample") {
  BiasPort port;
  port.state.Fixed(2048);
  Config c = BiasConfig();
  c.settle_us = 0;
  c.sample_count = 2;
  ntc_thermistor::NtcThermistor<BiasPort> sensor(port, c);
  REQUIRE(sensor.EnsureInitialized().ok());
  const std::size_t before = port.state.log.size();
  REQUIRE(sensor.Read().ok());
  CHECK(Sequence(port, before) == "brdrB");
}

TEST_CASE("ReadRawCode is one conversion in the bias sequence, with no averaging") {
  BiasPort port;
  port.state.codes = {1500, 1600, 1700};
  ntc_thermistor::NtcThermistor<BiasPort> sensor(port, BiasConfig());
  CHECK_EQ(sensor.ReadRawCode().error, Error::NotInitialized);
  CHECK_EQ(port.state.log.size(), 0U);
  CHECK_EQ(sensor.GetLastCode(), 0U);
  REQUIRE(sensor.EnsureInitialized().ok());
  const std::size_t before = port.state.log.size();
  const auto r = sensor.ReadRawCode();
  REQUIRE(r.ok());
  CHECK_EQ(r.value, 1500U);
  CHECK_EQ(sensor.GetLastCode(), 1500U);
  CHECK(Sequence(port, before) == "bdrB"); // settle, one sample, no delay between samples
}

TEST_CASE("ReadRawCode reports a failed read and an out-of-scale code, and "
          "GetLastCode keeps it") {
  BiasPort port;
  ntc_thermistor::NtcThermistor<BiasPort> sensor(port, BiasConfig());
  REQUIRE(sensor.EnsureInitialized().ok());
  port.state.codes = {9999};
  CHECK_EQ(sensor.ReadRawCode().error, Error::CodeOutOfScale);
  CHECK_EQ(sensor.GetLastCode(), 9999U);
  CHECK(port.state.log.back().op == Op::BiasOff);
  port.state.codes = {1234};
  port.state.next = 0;
  port.state.fail_read_at = port.state.reads;
  CHECK_EQ(sensor.ReadRawCode().error, Error::Bus);
  CHECK_EQ(sensor.GetLastCode(), 9999U); // a failed read leaves the last code
  CHECK(port.state.log.back().op == Op::BiasOff);
}

TEST_CASE("GetLastCode is the last sample of the last read") {
  BiasPort port;
  port.state.codes = {1000, 1002, 1004};
  Config c = BiasConfig();
  c.max_spread_codes = 0;
  ntc_thermistor::NtcThermistor<BiasPort> sensor(port, c);
  REQUIRE(sensor.EnsureInitialized().ok());
  REQUIRE(sensor.Read().ok());
  CHECK_EQ(sensor.GetLastCode(), 1004U);
}

} // namespace
