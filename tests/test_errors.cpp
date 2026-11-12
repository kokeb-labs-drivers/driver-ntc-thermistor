/**
 * @file test_errors.cpp
 * @brief The `Error` enum follows the standard, and its values are returned by
 * real calls.
 *
 * Base values (0 to 9) MAY go unreturned: this driver reads an analog node, so
 * `Timeout`, `DeviceIdMismatch`, `Crc`, `Busy`, `DeviceFault` and `Unsupported`
 * are never returned, and its contract says so. Every driver-specific value (16
 * and up) MUST be returned by some call.
 */
#include <cstring>

#include <array>

#include "mock_ntc_thermistor.hpp"
#include "ntc_thermistor.hpp"
#include "ntc_thermistor_presets.hpp"
#include "test_harness.hpp"

using ntc_thermistor::Error;
using ntc_thermistor_test::DelayPort;
using ntc_thermistor_test::MinimalPort;

static_assert(static_cast<int>(Error::Ok) == 0 && static_cast<int>(Error::NotInitialized) == 1 &&
                  static_cast<int>(Error::InvalidArgument) == 2 &&
                  static_cast<int>(Error::Bus) == 3 && static_cast<int>(Error::Unsupported) == 9,
              "the base Error set has fixed values (standards/driver.md)");
static_assert(static_cast<int>(Error::OpenCircuit) == 16 &&
                  static_cast<int>(Error::CodeOutOfScale) == 21,
              "the driver's own values start at 16");

namespace {

constexpr bool kRequireEveryBaseValue = false;
constexpr int kFirstDriverSpecific = 16;
/// Which error values some call has returned.
std::array<bool, 256>& Seen() {
  static std::array<bool, 256> seen{};
  return seen;
}

void Record(Error e) {
  Seen()[static_cast<uint8_t>(e)] = true;
}

ntc_thermistor::Config Beta() {
  ntc_thermistor::Config c{};
  c.curve = ntc_thermistor::presets::kGeneric10kB3950;
  c.min_temperature_mc = -40'000;
  c.max_temperature_mc = 125'000;
  return c;
}

/// One reading of a fixed code through a real driver, its error recorded.
Error ReadCode(uint32_t code, ntc_thermistor::Config c = Beta()) {
  DelayPort port;
  port.state.Fixed(code);
  ntc_thermistor::NtcThermistor<DelayPort> sensor(port, c);
  Record(sensor.Read().error); // NotInitialized
  Record(sensor.EnsureInitialized().error);
  const auto r = sensor.Read();
  Record(r.error);
  return r.error;
}

void RunScenarios() {
  CHECK_EQ(ReadCode(2048), Error::Ok);
  CHECK_EQ(ReadCode(4095), Error::OpenCircuit);
  CHECK_EQ(ReadCode(0), Error::ShortCircuit);
  CHECK_EQ(ReadCode(4000), Error::BelowRange);
  CHECK_EQ(ReadCode(100), Error::AboveRange);
  CHECK_EQ(ReadCode(4096), Error::CodeOutOfScale);
  {
    DelayPort port;
    port.state.codes = {2000, 2200};
    ntc_thermistor::Config c = Beta();
    c.sample_count = 2;
    c.max_spread_codes = 10;
    ntc_thermistor::NtcThermistor<DelayPort> sensor(port, c);
    REQUIRE(sensor.EnsureInitialized().ok());
    Record(sensor.Read().error); // Unstable
  }
  {
    MinimalPort port;
    ntc_thermistor::NtcThermistor<MinimalPort> bad(port, ntc_thermistor::Config{});
    Record(bad.EnsureInitialized().error); // InvalidArgument
    port.state.init_ok = false;
    ntc_thermistor::NtcThermistor<MinimalPort> down(port, Beta());
    Record(down.EnsureInitialized().error); // Bus
  }
}

} // namespace

TEST_CASE("Every driver-specific Error value is returned by some call") {
  RunScenarios();
  for (int v = 0; v < 256; ++v) {
    const auto e = static_cast<Error>(v);
    if (std::strcmp(ntc_thermistor::ToString(e), "Unknown") == 0) {
      continue;
    }
    const bool required = v >= kFirstDriverSpecific || kRequireEveryBaseValue;
    if (!Seen().at(static_cast<std::size_t>(v))) {
      std::printf("    %s: Error::%s is never returned\n", required ? "FAIL" : "note",
                  ntc_thermistor::ToString(e));
    }
    CHECK(Seen().at(static_cast<std::size_t>(v)) || !required);
    CHECK(v < 10 || v >= kFirstDriverSpecific);
  }
  CHECK(Seen()[static_cast<uint8_t>(Error::Ok)]);
  CHECK(Seen()[static_cast<uint8_t>(Error::NotInitialized)]);
  CHECK(Seen()[static_cast<uint8_t>(Error::InvalidArgument)]);
  CHECK(Seen()[static_cast<uint8_t>(Error::Bus)]);
}

TEST_CASE("ToString names every enumerator and says Unknown for anything else") {
  const Error all[] = {Error::Ok,
                       Error::NotInitialized,
                       Error::InvalidArgument,
                       Error::Bus,
                       Error::Timeout,
                       Error::DeviceIdMismatch,
                       Error::Crc,
                       Error::Busy,
                       Error::DeviceFault,
                       Error::Unsupported,
                       Error::OpenCircuit,
                       Error::ShortCircuit,
                       Error::BelowRange,
                       Error::AboveRange,
                       Error::Unstable,
                       Error::CodeOutOfScale};
  for (const Error e : all) {
    const char* name = ntc_thermistor::ToString(e);
    CHECK(std::strcmp(name, "Unknown") != 0);
    CHECK(std::strlen(name) > 1U);
  }
  CHECK(std::strcmp(ntc_thermistor::ToString(static_cast<Error>(10)), "Unknown") == 0);
  CHECK(std::strcmp(ntc_thermistor::ToString(static_cast<Error>(22)), "Unknown") == 0);
  CHECK(std::strcmp(ntc_thermistor::ToString(static_cast<Error>(255)), "Unknown") == 0);
}

TEST_CASE("Result carries a value or an error, and a failed result carries a "
          "zero value") {
  const auto ok = ntc_thermistor::Result<uint32_t>::Success(7);
  CHECK(ok.ok() && static_cast<bool>(ok) && ok.value == 7U);
  const auto bad = ntc_thermistor::Result<uint32_t>::Failure(Error::Bus);
  CHECK(!bad.ok() && !static_cast<bool>(bad) && bad.value == 0U && bad.error == Error::Bus);
  const auto v = ntc_thermistor::Result<void>::Success();
  CHECK(v.ok() && static_cast<bool>(v));
  const auto vf = ntc_thermistor::Result<void>::Failure(Error::InvalidArgument);
  CHECK(!vf.ok() && !static_cast<bool>(vf));
}

TEST_CASE("The version getters agree with the version header") {
  CHECK(std::strcmp(ntc_thermistor::GetDriverVersion(), NTC_THERMISTOR_VERSION_STRING) == 0);
  CHECK_EQ(ntc_thermistor::GetDriverVersionMajor(),
           static_cast<uint32_t>(NTC_THERMISTOR_VERSION_MAJOR));
}
