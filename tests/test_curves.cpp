/**
 * @file test_curves.cpp
 * @brief Steinhart-Hart mode, the tolerance of B on a table, the bound's extra
 * terms, the fault classification with a reference above the supply, and the
 * caps on the waits.
 *
 * The references are written here from the equations with `std::log`, not taken
 * from the driver.
 */
#include <cmath>
#include <cstdint>

#include "ntc_thermistor.hpp"
#include "ntc_thermistor_presets.hpp"
#include "test_harness.hpp"

namespace {

using ntc_thermistor::Config;
using ntc_thermistor::Curve;
using ntc_thermistor::Error;
using ntc_thermistor::Topology;
using ntc_thermistor::detail::operator""_u16;
using ntc_thermistor::detail::operator""_u8;

static_assert(0xFF_u8 == 255U && 0xFFFF_u16 == 65'535U, "the typed literals keep their value");

// A 10 kOhm part as published Steinhart-Hart coefficients:
// 0.8767633e-3, 2.527899e-4, 1.906681e-7 in units of 1e-15 per kelvin.
constexpr int64_t kA = 876'763'300'000;
constexpr int64_t kB = 252'789'900'000;
constexpr int64_t kC = 190'668'100;

double ShCelsius(double ohms) {
  const double ln = std::log(ohms);
  return (1.0 / ((static_cast<double>(kA) * 1e-15) + (static_cast<double>(kB) * 1e-15 * ln) +
                 (static_cast<double>(kC) * 1e-15 * ln * ln * ln))) -
         273.15;
}

Config ShConfig(Topology topology) {
  Config c;
  c.curve.sh_a_e15 = kA;
  c.curve.sh_b_e15 = kB;
  c.curve.sh_c_e15 = kC;
  c.min_temperature_mc = -40'000;
  c.max_temperature_mc = 150'000;
  c.adc_bits = 16;
  c.topology = topology;
  return c;
}

double NodeOhms(const Config& c, uint32_t code) {
  const auto ratio = static_cast<double>(code) / 65'536.0;
  const auto series = static_cast<double>(c.series_ohm);
  return (c.topology == Topology::NtcLowSide) ? (series * ratio / (1.0 - ratio))
                                              : (series * (1.0 - ratio) / ratio);
}

TEST_CASE("Steinhart-Hart mode follows the equation to 5 mK over -40 to 150 "
          "degC, both topologies") {
  for (const Topology topology : {Topology::NtcLowSide, Topology::NtcHighSide}) {
    const Config c = ShConfig(topology);
    CHECK(ntc_thermistor::Validate(c) == Error::Ok);
    int converted = 0;
    for (uint32_t code = 1000U; code < 64'500U; code += 61U) {
      const auto r = ntc_thermistor::Convert(c, code);
      const double want = ShCelsius(NodeOhms(c, code));
      if (!r) {
        CHECK(((want < -39.9) || (want > 149.9)) || (r.error == Error::BelowRange) ||
              (r.error == Error::AboveRange));
        continue;
      }
      ++converted;
      CHECK(std::fabs((static_cast<double>(r.value.temperature_mc) / 1000.0) - want) < 0.005);
    }
    CHECK(converted > 500);
  }
}

TEST_CASE("Steinhart-Hart mode at the reference point and in the window") {
  Config c = ShConfig(Topology::NtcLowSide);
  const auto mid = ntc_thermistor::Convert(c, 32'768U); // R = series = 10 kOhm
  REQUIRE(mid.ok());
  CHECK(std::abs(mid.value.temperature_mc - 25'000) < 20);
  CHECK_EQ(mid.value.resistance_ohm, 10'000U);
  c.max_temperature_mc = 20'000; // 25 degC is above the window now
  CHECK(ntc_thermistor::Convert(c, 32'768U).error == Error::AboveRange);
  c.max_temperature_mc = 150'000;
  c.min_temperature_mc = 30'000;
  CHECK(ntc_thermistor::Convert(c, 32'768U).error == Error::BelowRange);
}

TEST_CASE("Steinhart-Hart mode: an inverse that is not positive is AboveRange, "
          "not a number") {
  Config c = ShConfig(Topology::NtcLowSide);
  c.curve.sh_a_e15 = -ntc_thermistor::kMaxShAE15;
  CHECK(ntc_thermistor::Validate(c) == Error::Ok);
  CHECK(ntc_thermistor::Convert(c, 32'768U).error == Error::AboveRange);
}

TEST_CASE("Steinhart-Hart mode: the coefficients, the window and the other "
          "fields are checked") {
  Config ok = ShConfig(Topology::NtcLowSide);
  CHECK(ntc_thermistor::Validate(ok) == Error::Ok);
  Config c = ok;
  c.min_temperature_mc = ntc_thermistor::kNoMinTemperatureMc;
  CHECK(ntc_thermistor::Validate(c) == Error::InvalidArgument); // a window is mandatory
  c = ok;
  c.curve.sh_b_e15 = ntc_thermistor::kMaxShBE15 + 1;
  CHECK(ntc_thermistor::Validate(c) == Error::InvalidArgument);
  c = ok;
  c.curve.sh_a_e15 = ntc_thermistor::kMaxShAE15 + 1;
  CHECK(ntc_thermistor::Validate(c) == Error::InvalidArgument);
  c = ok;
  c.curve.sh_c_e15 = ntc_thermistor::kMaxShCE15 + 1;
  CHECK(ntc_thermistor::Validate(c) == Error::InvalidArgument);
  c = ok;
  c.curve.sh_c_e15 = -ntc_thermistor::kMaxShCE15; // 1/T would fall with ln R at the cold end
  CHECK(ntc_thermistor::Validate(c) == Error::InvalidArgument);
  c = ok;
  c.curve.sh_c_e15 = -1'000; // a small negative cubic term is fine
  CHECK(ntc_thermistor::Validate(c) == Error::Ok);
  c = ok;
  c.curve.r0_ohm = 10'000; // one description only
  CHECK(ntc_thermistor::Validate(c) == Error::InvalidArgument);
  c = ok;
  c.curve.b_kelvin = 3950;
  CHECK(ntc_thermistor::Validate(c) == Error::InvalidArgument);
  c = ok;
  c.curve.b_tol_ppm = 1000; // there is no B to be wrong
  CHECK(ntc_thermistor::Validate(c) == Error::InvalidArgument);
  c = ok;
  c.curve.curve_error_mc = ntc_thermistor::kMaxCurveErrorMc + 1U;
  CHECK(ntc_thermistor::Validate(c) == Error::InvalidArgument);
  // The coefficients belong to SH mode only.
  c = Config{};
  c.curve.r0_ohm = 10'000;
  c.curve.b_kelvin = 3950;
  c.min_temperature_mc = -40'000;
  c.max_temperature_mc = 125'000;
  CHECK(ntc_thermistor::Validate(c) == Error::Ok);
  c.curve.sh_a_e15 = 1; // with B zero this is beta mode plus a stray coefficient
  CHECK(ntc_thermistor::Validate(c) == Error::InvalidArgument);
  c = Config{};
  c.curve.table = ntc_thermistor::presets::kTdkNtcg163jf103ft1sRows;
  CHECK(ntc_thermistor::Validate(c) == Error::Ok);
  c.curve.sh_c_e15 = 1;
  CHECK(ntc_thermistor::Validate(c) == Error::InvalidArgument);
}

TEST_CASE("Steinhart-Hart mode: the bound has the resistance tolerance, the "
          "curve error and one code") {
  Config c = ShConfig(Topology::NtcLowSide);
  const auto base = ntc_thermistor::Convert(c, 32'768U);
  REQUIRE(base.ok());
  c.curve.curve_error_mc = 70;
  const auto with_error = ntc_thermistor::Convert(c, 32'768U);
  CHECK_EQ(with_error.value.uncertainty_mc, base.value.uncertainty_mc + 70U);
  c.curve.curve_error_mc = 0;
  c.curve.r_tol_ppm = 10'000; // 1 % of R at 25 degC is about 0.27 K here
  const auto with_tol = ntc_thermistor::Convert(c, 32'768U);
  CHECK(with_tol.value.uncertainty_mc > base.value.uncertainty_mc + 240U);
  CHECK(with_tol.value.uncertainty_mc < base.value.uncertainty_mc + 300U);
}

TEST_CASE("A table's bound includes the tolerance of B: T |1/T - 1/T0| tol, at "
          "both ends") {
  Config c;
  c.curve = ntc_thermistor::presets::kTdkNtcg163jf103ft1s; // b_tol_ppm 10000
  c.adc_bits = 16;
  Config none = c;
  none.curve.b_tol_ppm = 0;
  // A code per temperature: the node ratio of a 10 kOhm series resistor against
  // the row resistance.
  struct Case {
    double celsius;
    double ohms;
  };
  const Case cases[3] = {{125.0, 534.0}, {-35.0, 144'300.0}, {25.0, 10'000.0}};
  for (const Case& k : cases) {
    const double ratio = k.ohms / (10'000.0 + k.ohms);
    const auto code = static_cast<uint32_t>(std::lround(ratio * 65'536.0));
    const auto got = ntc_thermistor::Convert(c, code);
    const auto base = ntc_thermistor::Convert(none, code);
    REQUIRE(got.ok());
    REQUIRE(base.ok());
    const double t = (static_cast<double>(got.value.temperature_mc) / 1000.0) + 273.15;
    const double want = 0.01 * t * std::fabs(t - 298.15) / 298.15 * 1000.0;
    const double extra = static_cast<double>(got.value.uncertainty_mc) -
                         static_cast<double>(base.value.uncertainty_mc);
    CHECK(std::fabs(extra - want) < 3.0);
  }
  // At the reference temperature the term is zero.
  const auto at_ref = ntc_thermistor::Convert(c, 32'768U);
  const auto at_ref_none = ntc_thermistor::Convert(none, 32'768U);
  CHECK(at_ref.value.uncertainty_mc - at_ref_none.value.uncertainty_mc < 10U);
}

TEST_CASE("A reference above the supply: the open rail is the supply's, not "
          "the converter's") {
  Config c = ShConfig(Topology::NtcLowSide);
  c.adc_bits = 12;
  c.adc_vref_mv = 3300;
  c.divider_supply_mv = 3000; // the node cannot read above 4096 * 3000 / 3300 = 3723
  CHECK(ntc_thermistor::Validate(c) == Error::Ok);
  CHECK(ntc_thermistor::Convert(c, 3720U).error == Error::OpenCircuit);
  CHECK(ntc_thermistor::Convert(c, 3723U).error == Error::OpenCircuit);
  CHECK(ntc_thermistor::Convert(c, 4000U).error == Error::OpenCircuit);
  CHECK(ntc_thermistor::Convert(c, 3719U).error != Error::OpenCircuit);
  CHECK(ntc_thermistor::Convert(c, 2U).error == Error::ShortCircuit);
  c.topology = Topology::NtcHighSide; // the faults swap rails
  CHECK(ntc_thermistor::Convert(c, 3720U).error == Error::ShortCircuit);
  CHECK(ntc_thermistor::Convert(c, 2U).error == Error::OpenCircuit);
  // A reference below the supply is unchanged: the converter's top code is the
  // rail.
  c.adc_vref_mv = 3000;
  c.divider_supply_mv = 3300;
  c.topology = Topology::NtcLowSide;
  CHECK(ntc_thermistor::Convert(c, 4093U).error == Error::OpenCircuit);
  CHECK(ntc_thermistor::Convert(c, 4092U).error != Error::OpenCircuit);
}

TEST_CASE("Table ends are hard edges: the cold row is the last good resistance") {
  Config c;
  c.curve = ntc_thermistor::presets::kTdkNtcg163jf103ft1s;
  c.adc_bits = 16;
  const double cold = 188'500.0;
  const auto last_ok = static_cast<uint32_t>(std::floor(cold / (10'000.0 + cold) * 65'536.0));
  CHECK(ntc_thermistor::Convert(c, last_ok - 1U).ok());
  CHECK(ntc_thermistor::Convert(c, last_ok + 2U).error == Error::BelowRange);
  // At 12 bits one code is 0.2 K wide there: a node that is 188.5 kOhm reads
  // either side of the row.
  c.adc_bits = 12;
  CHECK(ntc_thermistor::Convert(c, 3'900U).error == Error::BelowRange);
}

TEST_CASE("A wait is capped: a 1 s wait is valid and a longer one is not") {
  Config c = ShConfig(Topology::NtcLowSide);
  c.sample_delay_us = ntc_thermistor::kMaxWaitUs;
  c.settle_us = ntc_thermistor::kMaxWaitUs;
  CHECK(ntc_thermistor::Validate(c) == Error::Ok);
  c.sample_delay_us = ntc_thermistor::kMaxWaitUs + 1U;
  CHECK(ntc_thermistor::Validate(c) == Error::InvalidArgument);
  c.sample_delay_us = 0;
  c.settle_us = ntc_thermistor::kMaxWaitUs + 1U;
  CHECK(ntc_thermistor::Validate(c) == Error::InvalidArgument);
}

} // namespace
