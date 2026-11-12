/**
 * @file test_generated_table.cpp
 * @brief The output of `tools/ntc_table.py` for a Steinhart-Hart part is a
 * valid table, and the driver's conversion on it follows the equation it was
 * made from, within the interpolation bound.
 */
#include <cmath>
#include <cstdint>

#include "ntc_thermistor.hpp"
#include "test_harness.hpp"

#include "generated_sh_table.inc"

namespace {

using ntc_thermistor::Config;
using ntc_thermistor::Curve;
using ntc_thermistor::Error;

constexpr double kA = 0.8767633e-3;
constexpr double kB = 2.527899e-4;
constexpr double kC = 1.906681e-7;

/// Steinhart-Hart temperature in degrees Celsius for a resistance in ohms.
double ShCelsius(double ohms) {
  const double ln = std::log(ohms);
  return (1.0 / (kA + (kB * ln) + (kC * ln * ln * ln))) - 273.15;
}

Config MakeConfig() {
  Config c;
  c.curve = Curve{.table = std::span<const ntc_thermistor::TablePoint>(kGeneratedSh)};
  c.adc_bits = 16;
  c.series_ohm = 10'000;
  return c;
}

TEST_CASE("The generated table is valid and covers -40 to 150 degC") {
  const Config c = MakeConfig();
  CHECK(ntc_thermistor::Validate(c) == Error::Ok);
  CHECK_EQ(c.curve.table.size(), std::size_t{39});
  CHECK_EQ(c.curve.table[0].temperature_mc, -40'000);
  CHECK_EQ(c.curve.table[38].temperature_mc, 150'000);
}

TEST_CASE("Convert on the generated table follows the Steinhart-Hart equation") {
  const Config c = MakeConfig();
  double worst = 0.0;
  for (uint32_t code = 2000U; code < 65'000U; code += 97U) {
    const auto r = ntc_thermistor::Convert(c, code);
    if (!r) {
      continue; // an end of the table: a range error is the contract
    }
    const double ohms = 10'000.0 * (static_cast<double>(code) / 65'536.0) /
                        (1.0 - (static_cast<double>(code) / 65'536.0));
    const double want = ShCelsius(ohms);
    const double got = static_cast<double>(r.value.temperature_mc) / 1000.0;
    worst = (std::fabs(got - want) > worst) ? std::fabs(got - want) : worst;
  }
  CHECK(worst < 0.06); // the table's interpolation error at 5 K rows, plus the
                       // integer logarithm
}

} // namespace
