/**
 * @file test_ntc_thermistor.cpp
 * @brief Driver behaviour: configuration checks, lifecycle, boundaries,
 * averaging, the pure conversion and its helpers.
 */
#include <array>
#include <bit>
#include <cstdint>

#include "mock_ntc_thermistor.hpp"
#include "ntc_thermistor.hpp"
#include "ntc_thermistor_presets.hpp"
#include "test_harness.hpp"

namespace {

using ntc_thermistor::Config;
using ntc_thermistor::Curve;
using ntc_thermistor::Error;
using ntc_thermistor::Topology;
using ntc_thermistor::Validate;
using ntc_thermistor_test::DelayPort;
using ntc_thermistor_test::MinimalPort;
using ntc_thermistor_test::Op;

namespace presets = ntc_thermistor::presets;

/// A valid beta configuration: 10 kOhm / B 3950, window -40 to +125 degC, 12
/// bits, 10 kOhm.
constexpr Config BetaConfig() {
  Config c{};
  c.curve = presets::kGeneric10kB3950;
  c.min_temperature_mc = -40'000;
  c.max_temperature_mc = 125'000;
  return c;
}

/// A valid table configuration: the example preset.
constexpr Config TableConfig() {
  Config c{};
  c.curve = presets::kTdkNtcg163jf103ft1s;
  return c;
}

// --8<-- [start:validate-static]
static_assert(Validate(BetaConfig()) == Error::Ok);
static_assert(Validate(TableConfig()) == Error::Ok);
static_assert(Validate(Config{}) == Error::InvalidArgument,
              "no default part: an empty curve is refused");
// --8<-- [end:validate-static]

constexpr Config With(Config c, auto mutate) {
  mutate(c);
  return c;
}

// --- Config validation: every field one step inside and one outside
// -----------------------------

static_assert(Validate(With(BetaConfig(), [](Config& c) { c.adc_bits = 7; })) ==
              Error::InvalidArgument);
static_assert(Validate(With(BetaConfig(), [](Config& c) { c.adc_bits = 8; })) == Error::Ok);
static_assert(Validate(With(BetaConfig(), [](Config& c) { c.adc_bits = 24; })) == Error::Ok);
static_assert(Validate(With(BetaConfig(), [](Config& c) { c.adc_bits = 25; })) ==
              Error::InvalidArgument);
static_assert(Validate(With(BetaConfig(), [](Config& c) { c.series_ohm = 0; })) ==
              Error::InvalidArgument);
static_assert(Validate(With(BetaConfig(), [](Config& c) { c.series_ohm = 1; })) == Error::Ok);
static_assert(Validate(With(BetaConfig(), [](Config& c) { c.series_ohm = 16'777'216; })) ==
              Error::Ok);
static_assert(Validate(With(BetaConfig(), [](Config& c) { c.series_ohm = 16'777'217; })) ==
              Error::InvalidArgument);
static_assert(Validate(With(BetaConfig(), [](Config& c) { c.curve.r0_ohm = 0; })) ==
              Error::InvalidArgument);
static_assert(Validate(With(BetaConfig(), [](Config& c) { c.curve.r0_ohm = 1; })) == Error::Ok);
static_assert(Validate(With(BetaConfig(), [](Config& c) { c.curve.r0_ohm = 10'000'000; })) ==
              Error::Ok);
static_assert(Validate(With(BetaConfig(), [](Config& c) { c.curve.r0_ohm = 10'000'001; })) ==
              Error::InvalidArgument);
static_assert(Validate(With(BetaConfig(), [](Config& c) { c.curve.b_kelvin = 999; })) ==
              Error::InvalidArgument);
static_assert(Validate(With(BetaConfig(), [](Config& c) { c.curve.b_kelvin = 1000; })) ==
              Error::Ok);
static_assert(Validate(With(BetaConfig(), [](Config& c) { c.curve.b_kelvin = 8000; })) ==
              Error::Ok);
static_assert(Validate(With(BetaConfig(), [](Config& c) { c.curve.b_kelvin = 8001; })) ==
              Error::InvalidArgument);
static_assert(Validate(With(BetaConfig(), [](Config& c) {
                c.curve.b_t2_mc = c.curve.ref_temperature_mc;
              })) == Error::InvalidArgument);
static_assert(Validate(With(BetaConfig(), [](Config& c) { c.curve.b_t2_mc = -273'150; })) ==
              Error::InvalidArgument);
static_assert(Validate(With(BetaConfig(), [](Config& c) {
                c.curve.ref_temperature_mc = -273'150;
              })) == Error::InvalidArgument);
static_assert(Validate(With(BetaConfig(),
                            [](Config& c) {
                              c.min_temperature_mc = ntc_thermistor::kNoMinTemperatureMc;
                              c.max_temperature_mc = ntc_thermistor::kNoMaxTemperatureMc;
                            })) == Error::InvalidArgument,
              "beta mode with the default window");
static_assert(Validate(With(BetaConfig(), [](Config& c) {
                c.min_temperature_mc = ntc_thermistor::kNoMinTemperatureMc;
              })) == Error::InvalidArgument);
static_assert(Validate(With(BetaConfig(), [](Config& c) {
                c.max_temperature_mc = ntc_thermistor::kNoMaxTemperatureMc;
              })) == Error::InvalidArgument);
static_assert(Validate(With(BetaConfig(), [](Config& c) { c.min_temperature_mc = -273'150; })) ==
                  Error::InvalidArgument,
              "the window stays above absolute zero");
static_assert(Validate(With(BetaConfig(), [](Config& c) { c.min_temperature_mc = -273'149; })) ==
              Error::Ok);
static_assert(Validate(With(BetaConfig(), [](Config& c) {
                c.max_temperature_mc = c.min_temperature_mc;
              })) == Error::InvalidArgument);
static_assert(Validate(With(TableConfig(), [](Config& c) {
                c.max_temperature_mc = c.min_temperature_mc;
              })) == Error::InvalidArgument);
// supply and reference: both or neither
static_assert(Validate(With(BetaConfig(), [](Config& c) { c.adc_vref_mv = 3300; })) ==
              Error::InvalidArgument);
static_assert(Validate(With(BetaConfig(), [](Config& c) { c.divider_supply_mv = 3300; })) ==
              Error::InvalidArgument);
static_assert(Validate(With(BetaConfig(), [](Config& c) {
                c.adc_vref_mv = 3300;
                c.divider_supply_mv = 3300;
              })) == Error::Ok);
static_assert(Validate(With(BetaConfig(), [](Config& c) {
                c.adc_vref_mv = 20'000;
                c.divider_supply_mv = 20'000;
              })) == Error::Ok);
static_assert(Validate(With(BetaConfig(), [](Config& c) {
                c.adc_vref_mv = 20'001;
                c.divider_supply_mv = 3300;
              })) == Error::InvalidArgument);
static_assert(Validate(With(BetaConfig(), [](Config& c) {
                c.adc_vref_mv = 3300;
                c.divider_supply_mv = 20'001;
              })) == Error::InvalidArgument);
static_assert(Validate(With(BetaConfig(),
                            [](Config& c) {
                              c.adc_vref_mv = 3300;
                              c.divider_supply_mv = 3300;
                              c.open_margin_codes = 0;
                            })) == Error::InvalidArgument,
              "margins are at least 1 when not ratiometric");
static_assert(Validate(With(BetaConfig(), [](Config& c) {
                c.adc_vref_mv = 3300;
                c.divider_supply_mv = 3300;
                c.short_margin_codes = 0;
              })) == Error::InvalidArgument);
static_assert(Validate(With(BetaConfig(),
                            [](Config& c) {
                              c.open_margin_codes = 0;
                              c.short_margin_codes = 0;
                            })) == Error::Ok,
              "a ratiometric divider may have margins of 0");
// sampling
static_assert(Validate(With(BetaConfig(), [](Config& c) { c.sample_count = 0; })) ==
              Error::InvalidArgument);
static_assert(Validate(With(BetaConfig(), [](Config& c) { c.sample_count = 1; })) == Error::Ok);
static_assert(Validate(With(BetaConfig(), [](Config& c) { c.sample_count = 16; })) == Error::Ok);
static_assert(Validate(With(BetaConfig(), [](Config& c) { c.sample_count = 17; })) ==
              Error::InvalidArgument);
// offset
static_assert(Validate(With(BetaConfig(), [](Config& c) { c.offset_mc = 20'000; })) == Error::Ok);
static_assert(Validate(With(BetaConfig(), [](Config& c) { c.offset_mc = -20'000; })) == Error::Ok);
static_assert(Validate(With(BetaConfig(), [](Config& c) { c.offset_mc = 20'001; })) ==
              Error::InvalidArgument);
static_assert(Validate(With(BetaConfig(), [](Config& c) { c.offset_mc = -20'001; })) ==
              Error::InvalidArgument);
// tolerances
static_assert(Validate(With(BetaConfig(), [](Config& c) { c.curve.r_tol_ppm = 500'000; })) ==
              Error::Ok);
static_assert(Validate(With(BetaConfig(), [](Config& c) { c.curve.r_tol_ppm = 500'001; })) ==
              Error::InvalidArgument);
static_assert(Validate(With(BetaConfig(), [](Config& c) { c.curve.b_tol_ppm = 500'001; })) ==
              Error::InvalidArgument);
static_assert(Validate(With(BetaConfig(), [](Config& c) { c.series_tol_ppm = 500'001; })) ==
              Error::InvalidArgument);
// topology out of enumeration
static_assert(Validate(With(BetaConfig(), [](Config& c) {
                c.topology = std::bit_cast<Topology>(uint8_t{2});
              })) == Error::InvalidArgument);
// margins that leave no usable code
static_assert(Validate(With(BetaConfig(), [](Config& c) {
                c.open_margin_codes = 2048;
                c.short_margin_codes = 2047;
              })) == Error::InvalidArgument);
static_assert(Validate(With(BetaConfig(), [](Config& c) {
                c.open_margin_codes = 2047;
                c.short_margin_codes = 2047;
              })) == Error::Ok);
// the table's b_kelvin and r0 stay in range when given
static_assert(Validate(With(TableConfig(), [](Config& c) { c.curve.b_kelvin = 999; })) ==
              Error::InvalidArgument);
static_assert(Validate(With(TableConfig(), [](Config& c) { c.curve.b_kelvin = 0; })) == Error::Ok);
static_assert(Validate(With(TableConfig(), [](Config& c) { c.curve.r0_ohm = 10'000'001; })) ==
              Error::InvalidArgument);

// Tables: sizes, order, duplicates.
constexpr ntc_thermistor::TablePoint kThree[3] = {{3000, -10'000}, {2000, 10'000}, {1000, 30'000}};
constexpr ntc_thermistor::TablePoint kOne[1] = {{3000, -10'000}};
constexpr ntc_thermistor::TablePoint kRisingR[3] = {
    {1000, -10'000}, {2000, 10'000}, {3000, 30'000}};
constexpr ntc_thermistor::TablePoint kDuplicateR[3] = {
    {3000, -10'000}, {3000, 10'000}, {1000, 30'000}};
constexpr ntc_thermistor::TablePoint kFallingT[3] = {
    {3000, 10'000}, {2000, -10'000}, {1000, -30'000}};
constexpr ntc_thermistor::TablePoint kZeroR[3] = {{3000, -10'000}, {2000, 10'000}, {0, 30'000}};
constexpr ntc_thermistor::TablePoint kColdT[3] = {{3000, -273'150}, {2000, 10'000}, {1000, 30'000}};
constexpr ntc_thermistor::TablePoint kCloseR[2] = {{1'000'000'000, -10'000},
                                                   {1'000'000'000 - 1, 10'000}};
constexpr ntc_thermistor::TablePoint kZeroFirstR[2] = {{0, -10'000}, {0, 10'000}};
constexpr Config TableOf(std::span<const ntc_thermistor::TablePoint> rows) {
  Config c{};
  c.curve.table = rows;
  return c;
}
static_assert(Validate(TableOf(kThree)) == Error::Ok);
static_assert(Validate(TableOf(kOne)) == Error::InvalidArgument);
static_assert(Validate(TableOf(kRisingR)) == Error::InvalidArgument);
static_assert(Validate(TableOf(kDuplicateR)) == Error::InvalidArgument);
static_assert(Validate(TableOf(kFallingT)) == Error::InvalidArgument);
static_assert(Validate(TableOf(kZeroR)) == Error::InvalidArgument);
static_assert(Validate(TableOf(kColdT)) == Error::InvalidArgument);
static_assert(Validate(TableOf(kCloseR)) == Error::InvalidArgument,
              "rows whose logarithms agree are refused");
static_assert(Validate(TableOf(kZeroFirstR)) == Error::InvalidArgument);

/// A table of `n` rows: R falling 0.005 % per row, T rising 1 degC per row.
template <std::size_t N>
struct Rows {
  ntc_thermistor::TablePoint rows[N]{};
  constexpr Rows() {
    uint32_t r = 4'000'000'000U;
    for (std::size_t i = 0; i < N; ++i) {
      rows[i] = {r, static_cast<int32_t>(i) * 1000};
      r -= r / 20'000U;
    }
  }
};
constexpr Rows<2> kTwoRows{};
constexpr Rows<256> kMaxRows{};
constexpr Rows<257> kTooMany{};
static_assert(Validate(TableOf(kTwoRows.rows)) == Error::Ok);
static_assert(Validate(TableOf(kMaxRows.rows)) == Error::Ok, "256 rows are the most");
static_assert(Validate(TableOf(kTooMany.rows)) == Error::InvalidArgument);

TEST_CASE("Validate is the same at run time as in the static_asserts above") {
  CHECK(Validate(BetaConfig()) == Error::Ok);
  CHECK(Validate(Config{}) == Error::InvalidArgument);
  Config c = BetaConfig();
  c.sample_count = 0;
  CHECK(Validate(c) == Error::InvalidArgument);
}

// --- Presets are data
// ---------------------------------------------------------------------------

/// A third part, declared here with no preset: 4.7 kOhm, B 3977 (B25/85),
/// window -30 to +100 degC.
constexpr Config ThirdPart() {
  Config c{};
  c.curve.r0_ohm = 4700;
  c.curve.b_kelvin = 3977;
  c.series_ohm = 4700;
  c.min_temperature_mc = -30'000;
  c.max_temperature_mc = 100'000;
  return c;
}
static_assert(Validate(ThirdPart()) == Error::Ok, "a part is data: no preset, no driver change");

TEST_CASE("A part with no preset works with the driver unchanged") {
  MinimalPort port;
  port.state.Fixed(2048);
  ntc_thermistor::NtcThermistor<MinimalPort> sensor(port, ThirdPart());
  REQUIRE(sensor.EnsureInitialized().ok());
  const auto r = sensor.Read();
  REQUIRE(r.ok());
  CHECK_EQ(r.value.temperature_mc, 25'000);
  CHECK_EQ(r.value.resistance_ohm, 4700U);
}

TEST_CASE("Both presets validate in a Config, and read 25 degC at their R25") {
  for (const Config& cfg : {TableConfig(), BetaConfig()}) {
    const auto r =
        ntc_thermistor::Convert(With(cfg, [](Config& c) { c.series_ohm = 10'000; }), 2048);
    REQUIRE(r.ok());
    CHECK_EQ(r.value.temperature_mc, 25'000);
  }
  CHECK_EQ(presets::kTdkNtcg163jf103ft1s.table.size(), 39U);
  CHECK(presets::kGeneric10kB3950.table.empty());
  CHECK_EQ(presets::kGeneric10kB3950.b_kelvin, 3950U);
  CHECK_EQ(presets::kGeneric10kB3950.b_t2_mc, 50'000);
}

// --- Lifecycle
// ----------------------------------------------------------------------------------

TEST_CASE("The constructor does no I/O; every call returns NotInitialized "
          "before the start") {
  MinimalPort port;
  ntc_thermistor::NtcThermistor<MinimalPort> sensor(port, BetaConfig());
  CHECK(port.state.log.empty());
  CHECK_EQ(sensor.Read().error, Error::NotInitialized);
  CHECK_EQ(sensor.ReadResistance().error, Error::NotInitialized);
  CHECK(port.state.log.empty()); // not a single port call, and no read starts the driver
  CHECK(!sensor.IsInitialized());
}

TEST_CASE("EnsureInitialized brings the port up once, then returns Ok with no I/O") {
  MinimalPort port;
  ntc_thermistor::NtcThermistor<MinimalPort> sensor(port, BetaConfig());
  REQUIRE(sensor.EnsureInitialized().ok());
  CHECK(sensor.IsInitialized());
  CHECK_EQ(port.state.Count(Op::Init), 1);
  for (int i = 0; i < 3; ++i) {
    CHECK(sensor.EnsureInitialized().ok());
  }
  CHECK_EQ(port.state.log.size(), 1U);
}

TEST_CASE("An invalid configuration is InvalidArgument with no port call; a "
          "port failure is Bus") {
  MinimalPort port;
  ntc_thermistor::NtcThermistor<MinimalPort> empty(port, Config{});
  CHECK_EQ(empty.EnsureInitialized().error, Error::InvalidArgument);
  CHECK(port.state.log.empty());
  CHECK(!empty.IsInitialized());
  ntc_thermistor::NtcThermistor<MinimalPort> sensor(port, BetaConfig());
  port.state.init_ok = false;
  CHECK_EQ(sensor.EnsureInitialized().error, Error::Bus);
  CHECK(!sensor.IsInitialized());
  CHECK_EQ(sensor.Read().error, Error::NotInitialized);
  port.state.init_ok = true;
  CHECK(sensor.EnsureInitialized().ok()); // the next call starts again
}

TEST_CASE("A configuration that needs a hook the port lacks is "
          "InvalidArgument, before any I/O") {
  MinimalPort port;
  Config delayed = BetaConfig();
  delayed.sample_count = 4;
  delayed.sample_delay_us = 100;
  ntc_thermistor::NtcThermistor<MinimalPort> a(port, delayed);
  CHECK_EQ(a.EnsureInitialized().error, Error::InvalidArgument);
  Config settle = BetaConfig();
  settle.settle_us = 100;
  ntc_thermistor::NtcThermistor<MinimalPort> b(port, settle);
  CHECK_EQ(b.EnsureInitialized().error, Error::InvalidArgument);
  CHECK(port.state.log.empty());
  // a delay between samples of one sample is not a wait
  Config one = BetaConfig();
  one.sample_delay_us = 100;
  ntc_thermistor::NtcThermistor<MinimalPort> c(port, one);
  CHECK(c.EnsureInitialized().ok());
  // with DelayUs the same configurations start
  DelayPort delay_port;
  ntc_thermistor::NtcThermistor<DelayPort> d(delay_port, delayed);
  CHECK(d.EnsureInitialized().ok());
  // settling needs SetBias even when DelayUs is there
  ntc_thermistor::NtcThermistor<DelayPort> e(delay_port, settle);
  CHECK_EQ(e.EnsureInitialized().error, Error::InvalidArgument);
  // a port with SetBias but no DelayUs cannot wait
  ntc_thermistor_test::BiasNoDelayPort bias_port;
  ntc_thermistor::NtcThermistor<ntc_thermistor_test::BiasNoDelayPort> f(bias_port, settle);
  CHECK_EQ(f.EnsureInitialized().error, Error::InvalidArgument);
  ntc_thermistor::NtcThermistor<ntc_thermistor_test::BiasNoDelayPort> g(bias_port, BetaConfig());
  CHECK(g.EnsureInitialized().ok());
}

TEST_CASE("SetConfig validates first and keeps the old configuration on failure") {
  MinimalPort port;
  ntc_thermistor::NtcThermistor<MinimalPort> sensor(port, BetaConfig());
  REQUIRE(sensor.EnsureInitialized().ok());
  Config bad = BetaConfig();
  bad.adc_bits = 3;
  CHECK_EQ(sensor.SetConfig(bad).error, Error::InvalidArgument);
  CHECK_EQ(sensor.GetConfig().adc_bits, 12);
  Config needs_delay = BetaConfig();
  needs_delay.sample_count = 2;
  needs_delay.sample_delay_us = 5;
  CHECK_EQ(sensor.SetConfig(needs_delay).error, Error::InvalidArgument);
  CHECK_EQ(sensor.GetConfig().sample_count, 1);
  Config good = BetaConfig();
  good.offset_mc = 500;
  CHECK(sensor.SetConfig(good).ok());
  CHECK_EQ(sensor.GetConfig().offset_mc, 500);
  const std::size_t calls = port.state.log.size();
  port.state.Fixed(2048);
  CHECK_EQ(sensor.Read().value.temperature_mc, 25'500);
  CHECK(port.state.log.size() > calls);
}

// --- Boundaries: codes, rows, window
// -------------------------------------------------------------

TEST_CASE("Codes at the rails: the exact fault for both topologies and several "
          "resolutions") {
  for (const uint8_t bits : std::array<uint8_t, 4>{8, 12, 16, 24}) {
    const uint32_t scale = uint32_t{1} << bits;
    const uint32_t top = scale - 1U;
    for (const Topology topology : {Topology::NtcLowSide, Topology::NtcHighSide}) {
      Config c = BetaConfig();
      c.adc_bits = bits;
      c.topology = topology;
      const bool low = topology == Topology::NtcLowSide;
      const Error high_rail = low ? Error::OpenCircuit : Error::ShortCircuit;
      const Error low_rail = low ? Error::ShortCircuit : Error::OpenCircuit;
      // the default margin is 2 codes at each rail
      CHECK_EQ(ntc_thermistor::Convert(c, 0).error, low_rail);
      CHECK_EQ(ntc_thermistor::Convert(c, 1).error, low_rail);
      CHECK_EQ(ntc_thermistor::Convert(c, 2).error, low_rail);
      CHECK(ntc_thermistor::Convert(c, 3).error != low_rail);
      CHECK_EQ(ntc_thermistor::Convert(c, top).error, high_rail);
      CHECK_EQ(ntc_thermistor::Convert(c, top - 1U).error, high_rail);
      CHECK_EQ(ntc_thermistor::Convert(c, top - 2U).error, high_rail);
      CHECK(ntc_thermistor::Convert(c, top - 3U).error != high_rail);
      CHECK_EQ(ntc_thermistor::Convert(c, scale).error, Error::CodeOutOfScale);
      CHECK_EQ(ntc_thermistor::Convert(c, scale + 1U).error, Error::CodeOutOfScale);
      CHECK_EQ(ntc_thermistor::Convert(c, UINT32_MAX).error, Error::CodeOutOfScale);
    }
  }
}

TEST_CASE("The margins belong to the faults: they move the edges, a margin of "
          "0 leaves only the rail") {
  for (const Topology topology : {Topology::NtcLowSide, Topology::NtcHighSide}) {
    Config c = BetaConfig();
    c.topology = topology;
    c.open_margin_codes = 0;
    c.short_margin_codes = 0;
    const bool low = topology == Topology::NtcLowSide;
    CHECK_EQ(ntc_thermistor::Convert(c, 0).error, low ? Error::ShortCircuit : Error::OpenCircuit);
    CHECK_EQ(ntc_thermistor::Convert(c, 4095).error,
             low ? Error::OpenCircuit : Error::ShortCircuit);
    CHECK(ntc_thermistor::Convert(c, 1).error != Error::ShortCircuit);
    CHECK(ntc_thermistor::Convert(c, 4094).error != Error::OpenCircuit);
    c.open_margin_codes = 10;
    c.short_margin_codes = 20;
    const uint32_t open_edge = low ? 4095U - 10U : 10U;
    const uint32_t short_edge = low ? 20U : 4095U - 20U;
    CHECK_EQ(ntc_thermistor::Convert(c, open_edge).error, Error::OpenCircuit);
    CHECK_EQ(ntc_thermistor::Convert(c, short_edge).error, Error::ShortCircuit);
    CHECK(ntc_thermistor::Convert(c, low ? open_edge - 1U : open_edge + 1U).error !=
          Error::OpenCircuit);
    CHECK(ntc_thermistor::Convert(c, low ? short_edge + 1U : short_edge - 1U).error !=
          Error::ShortCircuit);
  }
}

TEST_CASE("A reference above the supply: a node the supply cannot reach is the "
          "high-rail fault") {
  Config c = BetaConfig();
  c.adc_vref_mv = 3300;
  c.divider_supply_mv = 2500; // codes above 2500/3300 of full scale cannot occur
  CHECK(ntc_thermistor::Convert(c, 3000).ok() ||
        ntc_thermistor::Convert(c, 3000).error != Error::OpenCircuit);
  CHECK_EQ(ntc_thermistor::Convert(c, 3200).error, Error::OpenCircuit);
  c.topology = Topology::NtcHighSide;
  CHECK_EQ(ntc_thermistor::Convert(c, 3200).error, Error::ShortCircuit);
}

TEST_CASE("The table's ends: the end rows exactly, one ohm outside is out of range") {
  const auto& rows = presets::kTdkNtcg163jf103ft1sRows;
  Config c = TableConfig();
  const auto first = rows.front();
  const auto last = rows.back();
  // choose the series resistor equal to the row: R = Rs at half scale
  const auto at = [&c](uint32_t ohms, uint32_t code) {
    Config x = c;
    x.series_ohm = ohms;
    return ntc_thermistor::Convert(x, code);
  };
  CHECK_EQ(at(first.resistance_ohm, 2048).value.temperature_mc, first.temperature_mc);
  CHECK_EQ(at(last.resistance_ohm, 2048).value.temperature_mc, last.temperature_mc);
  CHECK_EQ(at(first.resistance_ohm + 1U, 2048).error, Error::BelowRange);
  CHECK_EQ(at(last.resistance_ohm - 1U, 2048).error, Error::AboveRange);
  CHECK(at(first.resistance_ohm - 1U, 2048).ok());
  CHECK(at(last.resistance_ohm + 1U, 2048).ok());
}

TEST_CASE("The window is inclusive at both ends, and the offset is added after "
          "it and saturates") {
  Config c = TableConfig();
  c.series_ohm = 10'000; // code 2048 is 25 degC
  c.min_temperature_mc = 25'000;
  c.max_temperature_mc = 25'000;
  CHECK_EQ(ntc_thermistor::Convert(c, 2048).error,
           Error::InvalidArgument); // min == max is not a window
  c.max_temperature_mc = 25'001;
  CHECK(ntc_thermistor::Convert(c, 2048).ok());
  c.min_temperature_mc = 24'000;
  c.max_temperature_mc = 25'000;
  CHECK(ntc_thermistor::Convert(c, 2048).ok());
  c.min_temperature_mc = 25'001;
  c.max_temperature_mc = 30'000;
  CHECK_EQ(ntc_thermistor::Convert(c, 2048).error, Error::BelowRange);
  c.min_temperature_mc = 20'000;
  c.max_temperature_mc = 24'999;
  CHECK_EQ(ntc_thermistor::Convert(c, 2048).error, Error::AboveRange);
  // the offset does not move the window
  c.min_temperature_mc = 20'000;
  c.max_temperature_mc = 25'000;
  c.offset_mc = 20'000;
  CHECK_EQ(ntc_thermistor::Convert(c, 2048).value.temperature_mc, 45'000);
  c.offset_mc = -20'000;
  CHECK_EQ(ntc_thermistor::Convert(c, 2048).value.temperature_mc, 5'000);
  // saturation: a reading near the top of the int32 range with a positive
  // offset
  CHECK_EQ(ntc_thermistor::detail::SaturatingAdd(INT32_MAX - 5, 20'000), INT32_MAX);
  CHECK_EQ(ntc_thermistor::detail::SaturatingAdd(INT32_MIN + 5, -20'000), INT32_MIN);
  CHECK_EQ(ntc_thermistor::detail::SaturatingAdd(-1, 1), 0);
}

TEST_CASE("Beta mode: a non-positive beta sum and a resistance beyond 32 bits "
          "are range errors") {
  Config c = BetaConfig();
  c.min_temperature_mc = -273'000;
  c.max_temperature_mc = 2'000'000;
  c.curve.b_kelvin = 8000;
  c.adc_bits = 24;
  c.series_ohm = 1;
  // R of a fraction of an ohm: ln(R/R0) is so negative that 1/T goes negative
  CHECK_EQ(ntc_thermistor::Convert(c, 3).error, Error::AboveRange);
  // a high-side divider with a huge series resistor and a code of 3 gives R >
  // 2^32
  Config h = BetaConfig();
  h.adc_bits = 24;
  h.topology = Topology::NtcHighSide;
  h.series_ohm = 16'777'216;
  h.open_margin_codes = 2;
  CHECK_EQ(ntc_thermistor::Convert(h, 3).error, Error::BelowRange);
  CHECK_EQ(ntc_thermistor::ConvertResistance(h, 3).error, Error::BelowRange);
  // a resistance of 0 is hotter than any curve (a window that admits it would
  // still refuse it)
  Config z = TableConfig();
  z.adc_bits = 24;
  z.topology = Topology::NtcLowSide;
  z.series_ohm = 1;
  CHECK_EQ(ntc_thermistor::Convert(z, 3).error, Error::AboveRange);
  CHECK_EQ(ntc_thermistor::detail::CurveTemperature(z.curve, 0).error, Error::AboveRange);
}

TEST_CASE("Convert validates the configuration first") {
  CHECK_EQ(ntc_thermistor::Convert(Config{}, 2048).error, Error::InvalidArgument);
  CHECK_EQ(ntc_thermistor::ConvertResistance(Config{}, 2048).error, Error::InvalidArgument);
}

TEST_CASE("The interpolation tolerates rows whose logarithms agree: it returns "
          "the colder row") {
  const ntc_thermistor::TablePoint a{1'000'000'000, -10'000};
  const ntc_thermistor::TablePoint b{1'000'000'000U - 1U, 10'000};
  CHECK_EQ(ntc_thermistor::detail::Interpolate(a, b, (uint64_t{1'000'000'000} * 256U) - 100U),
           -10'000);
}

// --- Reading, resistance
// ------------------------------------------------------------------------

TEST_CASE("Read returns the temperature, the resistance, the mean code and an "
          "uncertainty") {
  MinimalPort port;
  port.state.Fixed(2048);
  ntc_thermistor::NtcThermistor<MinimalPort> sensor(port, TableConfig());
  REQUIRE(sensor.EnsureInitialized().ok());
  const auto r = sensor.Read();
  REQUIRE(r.ok());
  CHECK_EQ(r.value.temperature_mc, 25'000);
  CHECK_EQ(r.value.resistance_ohm, 10'000U);
  CHECK_EQ(r.value.code, 2048U);
  CHECK(r.value.uncertainty_mc >= 1U);
  const auto ohms = sensor.ReadResistance();
  REQUIRE(ohms.ok());
  CHECK_EQ(ohms.value, 10'000U);
  CHECK(sensor.GetConfig().curve.table.size() == 39U);
}

TEST_CASE("ReadResistance has no curve and no window: out-of-curve resistances "
          "read fine") {
  MinimalPort port;
  port.state.Fixed(4000); // R = 10k * 4000/96 = 416 kOhm: colder than the TDK table
  ntc_thermistor::NtcThermistor<MinimalPort> sensor(port, TableConfig());
  REQUIRE(sensor.EnsureInitialized().ok());
  CHECK_EQ(sensor.Read().error, Error::BelowRange);
  const auto ohms = sensor.ReadResistance();
  REQUIRE(ohms.ok());
  CHECK_EQ(ohms.value, 416'667U);
  port.state.Fixed(4095);
  CHECK_EQ(sensor.ReadResistance().error, Error::OpenCircuit);
  port.state.Fixed(4096);
  CHECK_EQ(sensor.ReadResistance().error, Error::CodeOutOfScale);
  port.state.Fixed(0);
  CHECK_EQ(sensor.Read().error, Error::ShortCircuit);
}

// --- Averaging, spread
// --------------------------------------------------------------------------

TEST_CASE("The mean rounds to nearest, ties up, at every sample count") {
  for (uint8_t n = 1; n <= 16; ++n) {
    for (uint32_t extra = 0; extra <= n; ++extra) {
      // n samples: n - extra of 2000 and extra of 2001: the sum is 2000 n +
      // extra
      DelayPort port;
      port.state.codes.clear();
      for (uint32_t i = 0; i < n; ++i) {
        port.state.codes.push_back(i < extra ? 2001U : 2000U);
      }
      Config c = BetaConfig();
      c.sample_count = n;
      ntc_thermistor::NtcThermistor<DelayPort> sensor(port, c);
      REQUIRE(sensor.EnsureInitialized().ok());
      const auto r = sensor.Read();
      REQUIRE(r.ok());
      const uint32_t want = 2000U + ((2U * extra >= n) ? 1U : 0U);
      CHECK_EQ(r.value.code, want);
      CHECK_EQ(port.state.Count(Op::Read), static_cast<int>(n));
    }
  }
}

TEST_CASE("Samples are separated by sample_delay_us, n - 1 times, and every "
          "read uses the channel") {
  DelayPort port;
  port.state.Fixed(2048);
  Config c = BetaConfig();
  c.sample_count = 5;
  c.sample_delay_us = 150;
  c.adc_channel = 7;
  ntc_thermistor::NtcThermistor<DelayPort> sensor(port, c);
  REQUIRE(sensor.EnsureInitialized().ok());
  REQUIRE(sensor.Read().ok());
  CHECK_EQ(port.state.Count(Op::Read), 5);
  CHECK_EQ(port.state.Count(Op::Delay), 4);
  CHECK_EQ(port.state.WaitedUs(), 600U);
  for (const auto& e : port.state.log) {
    if (e.op == Op::Read) {
      CHECK_EQ(e.arg, 7U);
    }
  }
  // sample_delay_us 0 makes no wait at all
  DelayPort quiet;
  quiet.state.Fixed(2048);
  c.sample_delay_us = 0;
  ntc_thermistor::NtcThermistor<DelayPort> fast(quiet, c);
  REQUIRE(fast.EnsureInitialized().ok());
  REQUIRE(fast.Read().ok());
  CHECK_EQ(quiet.state.Count(Op::Delay), 0);
}

TEST_CASE("Unstable exactly above the spread; 0 turns the check off") {
  for (const uint32_t spread : {0U, 4U, 5U}) {
    DelayPort port;
    port.state.codes = {2000, 2005, 2002, 2003};
    Config c = BetaConfig();
    c.sample_count = 4;
    c.max_spread_codes = spread;
    ntc_thermistor::NtcThermistor<DelayPort> sensor(port, c);
    REQUIRE(sensor.EnsureInitialized().ok());
    const auto r = sensor.Read();
    // the samples span 5 codes
    if (spread == 4U) {
      CHECK_EQ(r.error, Error::Unstable);
      CHECK_EQ(r.value.code, 0U);
    } else {
      CHECK(r.ok());
    }
  }
}

TEST_CASE("The first failed sample is Bus and no result; a sample at or above "
          "scale is CodeOutOfScale") {
  DelayPort port;
  port.state.Fixed(2048);
  port.state.fail_read_at = 2;
  Config c = BetaConfig();
  c.sample_count = 4;
  ntc_thermistor::NtcThermistor<DelayPort> sensor(port, c);
  REQUIRE(sensor.EnsureInitialized().ok());
  const auto r = sensor.Read();
  CHECK_EQ(r.error, Error::Bus);
  CHECK_EQ(r.value.temperature_mc, 0);
  CHECK_EQ(port.state.Count(Op::Read), 3); // no read after the failure
  DelayPort scale;
  scale.state.codes = {2048, 4096, 2048, 2048};
  ntc_thermistor::NtcThermistor<DelayPort> s2(scale, c);
  REQUIRE(s2.EnsureInitialized().ok());
  CHECK_EQ(s2.Read().error, Error::CodeOutOfScale);
  CHECK_EQ(scale.state.Count(Op::Read), 2);
}

TEST_CASE("Noisy samples average out") {
  DelayPort port;
  port.state.Fixed(2048);
  port.state.noise = 3;
  Config c = BetaConfig();
  c.sample_count = 16;
  ntc_thermistor::NtcThermistor<DelayPort> sensor(port, c);
  REQUIRE(sensor.EnsureInitialized().ok());
  const auto r = sensor.Read();
  REQUIRE(r.ok());
  CHECK(r.value.code >= 2045U && r.value.code <= 2051U);
}

// --- Uncertainty
// --------------------------------------------------------------------------------

TEST_CASE("With no tolerance declared the uncertainty is one converter code") {
  Config c = BetaConfig();
  const auto r = ntc_thermistor::Convert(c, 2048);
  REQUIRE(r.ok());
  // 12 bits, node at half: about 0.029 K per code at 25 degC for B 3950
  // (T^2/B/4/4096 * 4)
  CHECK(r.value.uncertainty_mc >= 20U && r.value.uncertainty_mc <= 40U);
  c.adc_bits = 24;
  CHECK(ntc_thermistor::Convert(c, 1U << 23U).value.uncertainty_mc >= 1U);
  CHECK(ntc_thermistor::Convert(c, 1U << 23U).value.uncertainty_mc <= 2U);
}

TEST_CASE("Declared tolerances raise the bound and it contains a perturbed part") {
  ntc_thermistor_test::Rng rng(77);
  Config c = BetaConfig();
  c.curve.r_tol_ppm = 10'000;
  c.curve.b_tol_ppm = 10'000;
  c.series_tol_ppm = 10'000;
  ntc_thermistor_test::Part nominal;
  nominal.b_t2_c = 50.0L;
  const ntc_thermistor_test::Reference ref(nominal);
  ntc_thermistor_test::Divider d;
  d.bits = 12;
  uint32_t widest = 0;
  for (int i = 0; i < 1000; ++i) {
    // a real part: R25 and B off by up to 1 %, the series resistor off by up to
    // 1 %
    ntc_thermistor_test::Part real = nominal;
    real.r0 = nominal.r0 * (1.0L + rng.Between(-0.01L, 0.01L));
    real.beta = nominal.beta * (1.0L + rng.Between(-0.01L, 0.01L));
    ntc_thermistor_test::Divider dr = d;
    dr.series_ohm =
        static_cast<uint32_t>(std::lround(10'000.0L * (1.0L + rng.Between(-0.01L, 0.01L))));
    const long double t_true = rng.Between(-30.0L, 115.0L);
    const uint32_t code =
        ntc_thermistor_test::CodeFor(dr, ntc_thermistor_test::BetaR(real, t_true));
    const auto got = ntc_thermistor::Convert(c, code);
    if (!got.ok()) {
      continue;
    }
    // what the driver reports for the nominal part against what the perturbed
    // part really did
    const long double nominal_t = ntc_thermistor_test::BetaT(
        nominal, ntc_thermistor_test::ResistanceOf(d, static_cast<long double>(code)));
    const long double shift =
        std::fabs((static_cast<long double>(got.value.temperature_mc) / 1000.0L) - t_true);
    CHECK(shift <= (static_cast<long double>(got.value.uncertainty_mc) / 1000.0L) + 0.001L);
    CHECK(std::fabs(nominal_t - (static_cast<long double>(got.value.temperature_mc) / 1000.0L)) <
          0.2L);
    widest = got.value.uncertainty_mc > widest ? got.value.uncertainty_mc : widest;
  }
  CHECK(widest > 300U); // 1 % of R is 0.26 K at 25 degC, three of them add
  (void)ref;
}

TEST_CASE("The bound is not slack: it stays within 2x of the real worst case "
          "at the table's middle") {
  Config c = TableConfig();
  const auto r = ntc_thermistor::Convert(c, 2048);
  REQUIRE(r.ok());
  // 1 % of R at 25 degC moves the TDK table by 0.26 K; B is not used in table
  // mode
  CHECK(r.value.uncertainty_mc >= 250U && r.value.uncertainty_mc <= 600U);
  Config none = c;
  none.curve.r_tol_ppm = 0;
  CHECK(ntc_thermistor::Convert(none, 2048).value.uncertainty_mc < 60U);
}

TEST_CASE("The uncertainty saturates and never wraps at the extremes") {
  Config c = BetaConfig();
  c.curve.r_tol_ppm = 500'000;
  c.curve.b_tol_ppm = 500'000;
  c.series_tol_ppm = 500'000;
  c.min_temperature_mc = -273'000;
  c.max_temperature_mc = 1'000'000;
  c.curve.b_kelvin = 8000;
  for (uint32_t code = 3; code < 4093U; code += 97U) {
    const auto r = ntc_thermistor::Convert(c, code);
    if (r.ok()) {
      CHECK(r.value.uncertainty_mc >= 1U);
    }
  }
  CHECK_EQ(ntc_thermistor::detail::SatMul(UINT64_MAX, 2), UINT64_MAX);
  CHECK_EQ(ntc_thermistor::detail::SatMul(0, UINT64_MAX), 0U);
  CHECK_EQ(ntc_thermistor::detail::SatAdd(UINT64_MAX, 1), UINT64_MAX);
  CHECK_EQ(ntc_thermistor::detail::SatAdd(1, 2), 3U);
  CHECK_EQ(ntc_thermistor::detail::DivRoundU(5, 0), 0U);
  CHECK_EQ(ntc_thermistor::detail::DivRound(5, 0), 0);
  CHECK_EQ(ntc_thermistor::detail::DivRound(-5, 2), -3);
  CHECK_EQ(ntc_thermistor::detail::DivRound(5, 2), 3);
  CHECK_EQ(ntc_thermistor::detail::DivRoundU(UINT64_MAX, 2), (UINT64_MAX / 2U) + 1U);
}

// --- Helpers
// ------------------------------------------------------------------------------------

TEST_CASE("Unit conversions and the self-heating power") {
  CHECK_EQ(ntc_thermistor::ToMilliKelvin(25'000), 298'150);
  CHECK_EQ(ntc_thermistor::ToMilliKelvin(INT32_MAX), INT32_MAX);
  CHECK_EQ(ntc_thermistor::ToMilliFahrenheit(25'000), 77'000);
  CHECK_EQ(ntc_thermistor::ToMilliFahrenheit(-40'000), -40'000);
  CHECK_EQ(ntc_thermistor::ToMilliFahrenheit(100'000), 212'000);
  CHECK_EQ(ntc_thermistor::ToMilliFahrenheit(INT32_MAX), INT32_MAX);
  CHECK_EQ(ntc_thermistor::ToMilliFahrenheit(INT32_MIN), INT32_MIN);
  Config c = BetaConfig();
  CHECK_EQ(ntc_thermistor::SelfHeatingMicrowatts(c, 10'000),
           0U); // ratiometric: no supply known
  c.adc_vref_mv = 3300;
  c.divider_supply_mv = 3300;
  // 3.3 V over 10 k + 10 k: 3.3^2 * 10k / 20k^2 = 272.25 uW
  CHECK_EQ(ntc_thermistor::SelfHeatingMicrowatts(c, 10'000), 272U);
  c.series_ohm = 16'777'216;
  c.adc_vref_mv = 20'000;
  c.divider_supply_mv = 20'000;
  CHECK(ntc_thermistor::SelfHeatingMicrowatts(c, UINT32_MAX) <
        1000U); // large resistances: no overflow
  c.series_ohm = 1;
  CHECK_EQ(ntc_thermistor::SelfHeatingMicrowatts(c, 1),
           100'000'000U); // 20 V across 2 ohm: the part sees 20 V^2 / 4
}

TEST_CASE("GetConfig returns the configuration in use") {
  MinimalPort port;
  Config c = TableConfig();
  c.adc_channel = 3;
  ntc_thermistor::NtcThermistor<MinimalPort> sensor(port, c);
  CHECK_EQ(sensor.GetConfig().adc_channel, 3);
}

TEST_CASE("The integer helpers: a zero divisor saturates, and a wide value "
          "clamps to 32 bits") {
  CHECK_EQ(ntc_thermistor::detail::FineQuotient(5, 0), ntc_thermistor::detail::kMaxU64);
  CHECK_EQ(ntc_thermistor::detail::ClampToInt32(int64_t{INT32_MAX} + 5), INT32_MAX);
  CHECK_EQ(ntc_thermistor::detail::ClampToInt32(int64_t{INT32_MIN} - 5), INT32_MIN);
  CHECK_EQ(ntc_thermistor::detail::ClampToInt32(-7), -7);
}

} // namespace
