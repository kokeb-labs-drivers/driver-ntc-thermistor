/**
 * @file test_model.cpp
 * @brief The driver against the model, for every synthetic part: exactness at
 * the rows, agreement with the true curve, the interpolation bound, and the
 * properties that hold for any part.
 *
 * tests/mock_ntc_thermistor.hpp is written from the equations without the
 * driver's headers, so a constant or a formula the driver gets wrong is not
 * copied into the model. These tests make the two agree, or the build fails
 * (handbook KB-TST-030).
 */
#include <cmath>
#include <cstdint>
#include <numbers>

#include "mock_ntc_thermistor.hpp"
#include "ntc_thermistor.hpp"
#include "ntc_thermistor_presets.hpp"
#include "test_harness.hpp"

namespace {

using ntc_thermistor::Config;
using ntc_thermistor::Error;
using ntc_thermistor::TablePoint;
using ntc_thermistor::Topology;
using ntc_thermistor_test::CodeFor;
using ntc_thermistor_test::Divider;
using ntc_thermistor_test::GeoSweep;
using ntc_thermistor_test::Law;
using ntc_thermistor_test::LsbKelvin;
using ntc_thermistor_test::MakeConfig;
using ntc_thermistor_test::Part;
using ntc_thermistor_test::Parts;
using ntc_thermistor_test::Real;
using ntc_thermistor_test::Reference;
using ntc_thermistor_test::ResistanceOf;
using ntc_thermistor_test::RoundingKelvin;
using ntc_thermistor_test::Scale;
using ntc_thermistor_test::Sweep;

constexpr uint32_t kBitsList[3] = {10, 12, 16};
constexpr Topology kTopologies[2] = {Topology::NtcLowSide, Topology::NtcHighSide};

// --- Compile-time checks (Ln, the preset)
// -------------------------------------------------------

/// Whether the rows of `rows` are strictly monotone: R falling, T rising.
template <typename Rows>
constexpr bool Monotone(const Rows& rows) {
  for (std::size_t i = 1; i < rows.size(); ++i) {
    if ((rows[i].resistance_ohm >= rows[i - 1U].resistance_ohm) ||
        (rows[i].temperature_mc <= rows[i - 1U].temperature_mc)) {
      return false;
    }
  }
  return true;
}

// --8<-- [start:cross-check]
static_assert(ntc_thermistor::detail::Ln16(1U) == 0U, "ln 1 = 0");
static_assert(ntc_thermistor::detail::Ln16(2U) == 45426U, "ln 2 in Q16: 0.693147 * 65536");
static_assert(ntc_thermistor::detail::Ln16(uint64_t{1U} << 20U) == 908'522U, "ln 2^20 in Q16");
static_assert(ntc_thermistor::detail::Ln16(uint64_t{1U} << 40U) == 1'817'044U, "ln 2^40 in Q16");
static_assert(Monotone(ntc_thermistor::presets::kTdkNtcg163jf103ft1sRows), "the preset is a curve");
static_assert(ntc_thermistor::Validate(ntc_thermistor::Config{
                  .curve = ntc_thermistor::presets::kTdkNtcg163jf103ft1s}) == Error::Ok,
              "the example preset is a valid curve with the default divider");
// --8<-- [end:cross-check]

TEST_CASE("Ln16 is within one unit (2^-16 neper) of std::log over 1 to 2^64 - "
          "1, monotone") {
  uint32_t previous = 0;
  uint64_t x = 1;
  while (true) {
    const uint32_t got = ntc_thermistor::detail::Ln16(x);
    const Real want = std::log(static_cast<Real>(x)) * 65536.0L;
    CHECK(std::fabs(static_cast<Real>(got) - want) <= 1.0L);
    CHECK(got >= previous);
    previous = got;
    const uint64_t next = x + 1U + (x / 97U);
    if (next < x) {
      break; // wrapped: x is within 1/97 of 2^64
    }
    x = next;
  }
  const uint64_t top = ~uint64_t{0};
  CHECK(std::fabs(static_cast<Real>(ntc_thermistor::detail::Ln16(top)) -
                  (std::log(static_cast<Real>(top)) * 65536.0L)) <= 1.0L);
  for (uint32_t n = 0; n < 64U; ++n) {
    const Real want = static_cast<Real>(n) * std::numbers::ln2_v<Real> * 65536.0L;
    CHECK(std::fabs(static_cast<Real>(ntc_thermistor::detail::Ln16(uint64_t{1} << n)) - want) <=
          1.0L);
  }
  CHECK_EQ(ntc_thermistor::detail::Ln16(0), 0U);
}

TEST_CASE("The preset rows equal the model's own copy of the TDK table, row "
          "for row") {
  const auto& model = ntc_thermistor_test::TdkRows();
  const auto& preset = ntc_thermistor::presets::kTdkNtcg163jf103ft1sRows;
  REQUIRE(model.size() == preset.size());
  for (std::size_t i = 0; i < preset.size(); ++i) {
    CHECK_EQ(preset[i].resistance_ohm, model[i].resistance_ohm);
    CHECK_EQ(preset[i].temperature_mc, model[i].temperature_mc);
  }
}

TEST_CASE("The TDK rows imply the B values the vendor lists, within 3 K (a "
          "sanity check of the data)") {
  const auto& rows = ntc_thermistor_test::TdkRows();
  const auto find = [&rows](int celsius) {
    for (const TablePoint& r : rows) {
      if (r.temperature_mc == celsius * 1000) {
        return static_cast<Real>(r.resistance_ohm);
      }
    }
    return Real{0};
  };
  const Real r25 = find(25);
  const Real k25 = 298.15L;
  const int pairs[4] = {50, 75, 85, 100};
  const Real listed[4] = {3382.0L, 3421.6L, 3435.4L, 3453.2L};
  for (int i = 0; i < 4; ++i) {
    const Real t2 = static_cast<Real>(pairs[i]) + 273.15L;
    const Real b = std::log(r25 / find(pairs[i])) / ((1.0L / k25) - (1.0L / t2));
    CHECK(std::fabs(b - listed[i]) < 3.0L);
  }
}

TEST_CASE("The TDK preset carries the figures of the datasheet's Electrical Characteristics "
          "(Rev.3, p. 1) and its rows agree with the sheet's B values") {
  const ntc_thermistor::Curve& c = ntc_thermistor::presets::kTdkNtcg163jf103ft1s;
  CHECK_EQ(c.r0_ohm, 10'000U); // resistance at 25 degC: 10 kOhm
  CHECK_EQ(c.ref_temperature_mc, 25'000);
  CHECK_EQ(c.r_tol_ppm, 10'000U); // +-1 %
  CHECK_EQ(c.b_kelvin, 3'435U);   // B25/85: 3435 K
  CHECK_EQ(c.b_t2_mc, 85'000);
  CHECK_EQ(c.b_tol_ppm, 10'000U); // +-1 %
  // Operating temperature -40 to +150 degC: the table spans exactly that range.
  CHECK_EQ(c.table.front().temperature_mc, -40'000);
  CHECK_EQ(c.table.back().temperature_mc, 150'000);
  // The sheet gives B25/85 = 3435 K +-1 % and B25/50 = 3380 K typ.: the rows imply both. (The sheet
  // prints no table; its 1 degC step data is a separate file that could not be fetched, so the rows
  // stay unchecked.)
  const auto& rows = ntc_thermistor_test::TdkRows();
  const auto find = [&rows](int celsius) {
    for (const TablePoint& r : rows) {
      if (r.temperature_mc == celsius * 1000) {
        return static_cast<Real>(r.resistance_ohm);
      }
    }
    return Real{0};
  };
  const Real k25 = 298.15L;
  const auto implied_b = [&](int celsius) {
    return std::log(find(25) / find(celsius)) /
           ((1.0L / k25) - (1.0L / (static_cast<Real>(celsius) + 273.15L)));
  };
  CHECK(std::fabs(implied_b(85) - 3435.0L) < 3435.0L * 0.01L);
  CHECK(std::fabs(implied_b(85) - 3435.0L) < 1.0L);
  CHECK(std::fabs(implied_b(50) - 3380.0L) < 5.0L);
}

TEST_CASE("Every part's table is strictly monotone and valid") {
  for (const Part& p : Parts()) {
    if (!p.table_mode) {
      continue;
    }
    for (std::size_t i = 1; i < p.rows.size(); ++i) {
      CHECK(p.rows[i].resistance_ohm < p.rows[i - 1U].resistance_ohm);
      CHECK(p.rows[i].temperature_mc > p.rows[i - 1U].temperature_mc);
    }
    Divider d;
    d.series_ohm = p.series_ohm;
    CHECK(ntc_thermistor::Validate(MakeConfig(p, d)) == Error::Ok);
  }
}

// --- Exactness at the rows
// ----------------------------------------------------------------------

TEST_CASE("A code at a row's resistance returns the row's temperature, within "
          "one code") {
  for (const Part& p : Parts()) {
    if (!p.table_mode) {
      continue;
    }
    const Reference ref(p);
    for (const uint32_t bits : kBitsList) {
      for (const Topology topology : kTopologies) {
        Divider d;
        d.series_ohm = p.series_ohm;
        d.bits = bits;
        d.topology = topology;
        const Config cfg = MakeConfig(p, d);
        for (const TablePoint& row : p.rows) {
          const uint32_t code = CodeFor(d, static_cast<Real>(row.resistance_ohm));
          const auto r = ntc_thermistor::Convert(cfg, code);
          if (!r.ok()) {
            // a code that rounds just past an end row is outside the table:
            // only the end rows may do so
            const bool end_row = (&row == &p.rows.front()) || (&row == &p.rows.back());
            CHECK(end_row);
            CHECK((r.error == Error::AboveRange) || (r.error == Error::BelowRange));
            continue;
          }
          const Real want = static_cast<Real>(row.temperature_mc) / 1000.0L;
          const Real bound = LsbKelvin(ref, d, code) +
                             RoundingKelvin(ref, static_cast<Real>(row.resistance_ohm)) + 0.003L;
          CHECK(std::fabs((static_cast<Real>(r.value.temperature_mc) / 1000.0L) - want) <= bound);
        }
      }
    }
  }
}

TEST_CASE("With a series resistor equal to a row's resistance the result is "
          "the row, to the "
          "milli-degree") {
  for (const Part& p : Parts()) {
    if (!p.table_mode) {
      continue;
    }
    for (const Topology topology : kTopologies) {
      for (const TablePoint& row : p.rows) {
        Divider d;
        d.series_ohm = row.resistance_ohm;
        d.bits = 12;
        d.topology = topology;
        const auto r = ntc_thermistor::Convert(MakeConfig(p, d), 2048);
        REQUIRE(r.ok());
        CHECK_EQ(r.value.temperature_mc, row.temperature_mc);
        CHECK_EQ(r.value.resistance_ohm, row.resistance_ohm);
      }
    }
  }
}

// --- Agreement with the true curve
// --------------------------------------------------------------

/// The error bound of one conversion: the converter's step, the whole-ohm
/// rounding of the rows and of the result, the part's interpolation bound and
/// the logarithm's own quantisation.
Real Bound(const Part& p, const Reference& ref, const Divider& d, uint32_t code, Real r) {
  Real interp = 0.0L;
  if (p.table_mode) {
    interp = p.law == Law::Rows || p.law == Law::SteinhartHart
                 ? p.interp_k
                 : (0.0013L * p.step_c * p.step_c) + 0.003L;
  }
  return LsbKelvin(ref, d, code) + (2.0L * RoundingKelvin(ref, r)) + interp + 0.01L;
}

TEST_CASE("Convert follows the true curve of every part over its whole range, "
          "both topologies") {
  for (const Part& p : Parts()) {
    if (p.law == Law::Rows) {
      continue; // P7 has no formula: it is checked at its rows and against its
                // cubic below
    }
    const Reference ref(p);
    for (const uint32_t bits : kBitsList) {
      for (const Topology topology : kTopologies) {
        Divider d;
        d.series_ohm = p.series_ohm;
        d.bits = bits;
        d.topology = topology;
        const Config cfg = MakeConfig(p, d);
        int checked = 0;
        for (const Real t : Sweep(p.t_lo_c + 1.0L, p.t_hi_c - 1.0L, 0.37L)) {
          const Real r = p.law == Law::Rows ? Real{0} : ref.R(t);
          if (p.law == Law::Rows) {
            continue; // P7 has no formula: it is checked at its rows and
                      // against its cubic below
          }
          const uint32_t code = CodeFor(d, r);
          const Real lsb = LsbKelvin(ref, d, code);
          if (std::fabs(t - p.t_lo_c) < (2.0L * lsb) + 0.1L ||
              std::fabs(t - p.t_hi_c) < (2.0L * lsb) + 0.1L) {
            continue;
          }
          const auto got = ntc_thermistor::Convert(cfg, code);
          REQUIRE(got.ok());
          const Real exact = ref.T(ResistanceOf(d, static_cast<Real>(code)));
          CHECK(std::fabs((static_cast<Real>(got.value.temperature_mc) / 1000.0L) - exact) <=
                Bound(p, ref, d, code, ResistanceOf(d, static_cast<Real>(code))));
          ++checked;
        }
        CHECK(checked > 50);
      }
    }
  }
}

TEST_CASE("The TDK table part (P7) follows its monotone cubic between rows") {
  for (const Part& p : Parts()) {
    if (p.law != Law::Rows) {
      continue;
    }
    const Reference ref(p);
    Divider d;
    d.series_ohm = p.series_ohm;
    d.bits = 16;
    const Config cfg = MakeConfig(p, d);
    for (const Real r : GeoSweep(330.0L, 188000.0L, 1.0173L)) {
      const uint32_t code = CodeFor(d, r);
      const auto got = ntc_thermistor::Convert(cfg, code);
      REQUIRE(got.ok());
      const Real exact = ref.T(ResistanceOf(d, static_cast<Real>(code)));
      CHECK(std::fabs((static_cast<Real>(got.value.temperature_mc) / 1000.0L) - exact) <=
            Bound(p, ref, d, code, r));
    }
  }
}

// --- Interpolation bound
// ------------------------------------------------------------------------

/// The worst error of `cfg`'s table against `ref` over 19 interior points of
/// every segment, using the driver's own curve function.
Real WorstInterpolation(const Part& p, const Reference& ref) {
  ntc_thermistor::Curve c{};
  c.table = std::span<const TablePoint>(p.rows);
  Real worst = 0.0L;
  for (std::size_t i = 0; i + 1U < p.rows.size(); ++i) {
    const Real t0 = static_cast<Real>(p.rows[i].temperature_mc) / 1000.0L;
    const Real t1 = static_cast<Real>(p.rows[i + 1U].temperature_mc) / 1000.0L;
    for (int k = 1; k <= 19; ++k) {
      Real r = 0.0L;
      if (p.law == Law::Rows) {
        Real lo = static_cast<Real>(p.rows[i + 1U].resistance_ohm);
        Real hi = static_cast<Real>(p.rows[i].resistance_ohm);
        const Real want = t0 + ((t1 - t0) * static_cast<Real>(k) / 20.0L);
        for (int j = 0; j < 100; ++j) {
          const Real mid = (lo + hi) / 2.0L;
          (ref.T(mid) > want ? lo : hi) = mid;
        }
        r = (lo + hi) / 2.0L;
      } else {
        r = ref.R(t0 + ((t1 - t0) * static_cast<Real>(k) / 20.0L));
      }
      const auto fine = static_cast<uint64_t>(std::llround(r * 256.0L));
      const auto got = ntc_thermistor::detail::CurveTemperature(c, fine);
      CHECK(got.ok());
      const Real err = std::fabs((static_cast<Real>(got.value) / 1000.0L) -
                                 ref.T(static_cast<Real>(fine) / 256.0L));
      worst = err > worst ? err : worst;
    }
  }
  return worst;
}

TEST_CASE("Interpolation error is within the pinned bound for the table parts "
          "(P4, P5, P7)") {
  for (const Part& p : Parts()) {
    if (!p.table_mode) {
      continue;
    }
    const Reference ref(p);
    const Real worst = WorstInterpolation(p, ref);
    CHECK(worst <= p.interp_k);
    CHECK(worst > p.interp_k / 4.0L); // the bound is not slack: it pins a measured value
  }
}

TEST_CASE("A table generated from a beta curve interpolates within 0.0013 "
          "step^2 + 0.003 K") {
  for (const Part& base : Parts()) {
    if (base.table_mode) {
      continue;
    }
    for (const Real step : {1.0L, 2.5L, 5.0L, 10.0L}) {
      Part p = base;
      p.table_mode = true;
      p.step_c = step;
      p.rows = ntc_thermistor_test::MakeRows(p);
      const Reference ref(p);
      CHECK(WorstInterpolation(p, ref) <= (0.0013L * step * step) + 0.003L);
    }
  }
}

TEST_CASE("Beta mode and a table generated from the same beta curve agree "
          "within the interpolation "
          "bound") {
  for (const Part& base : Parts()) {
    if (base.table_mode) {
      continue;
    }
    Part tab = base;
    tab.table_mode = true;
    tab.step_c = 2.5L;
    tab.rows = ntc_thermistor_test::MakeRows(tab);
    Divider d;
    d.series_ohm = base.series_ohm;
    d.bits = 16;
    const Config beta = MakeConfig(base, d);
    const Config table = MakeConfig(tab, d);
    const Reference ref(base);
    for (const Real t : Sweep(base.t_lo_c + 3.0L, base.t_hi_c - 3.0L, 1.1L)) {
      const uint32_t code = CodeFor(d, ref.R(t));
      const auto a = ntc_thermistor::Convert(beta, code);
      const auto b = ntc_thermistor::Convert(table, code);
      REQUIRE(a.ok() && b.ok());
      const Real r = ResistanceOf(d, static_cast<Real>(code));
      CHECK(std::fabs(static_cast<Real>(a.value.temperature_mc - b.value.temperature_mc)) /
                1000.0L <=
            (0.0013L * 2.5L * 2.5L) + 0.003L + (2.0L * RoundingKelvin(ref, r)) + 0.005L);
    }
  }
}

// --- Properties
// ---------------------------------------------------------------------------------

/// Where `error` sits along rising code: the ranks of the zones a low-side
/// divider shows (high side reversed).
int ZoneRank(Error e, Topology topology) {
  int rank = 2;
  if (e == Error::ShortCircuit) {
    rank = 0;
  } else if (e == Error::AboveRange) {
    rank = 1;
  } else if (e == Error::BelowRange) {
    rank = 3;
  } else if (e == Error::OpenCircuit) {
    rank = 4;
  } else {
    rank = 2; // a reading
  }
  return topology == Topology::NtcLowSide ? rank : 4 - rank;
}

/// Sweep every code in [0, 2^bits) and check the zones come in order and the
/// temperature never turns.
void CheckSweep(const Part& p, uint32_t bits, Topology topology) {
  Divider d;
  d.series_ohm = p.series_ohm;
  d.bits = bits;
  d.topology = topology;
  const Config cfg = MakeConfig(p, d);
  const uint32_t scale = 1U << bits;
  int previous_rank = -1;
  int64_t previous_t = 0;
  bool have_t = false;
  int ok_codes = 0;
  for (uint32_t code = 0; code < scale; ++code) {
    const auto r = ntc_thermistor::Convert(cfg, code);
    const int rank = ZoneRank(r.error, topology);
    if (!r.ok()) {
      CHECK(r.error == Error::ShortCircuit || r.error == Error::OpenCircuit ||
            r.error == Error::AboveRange || r.error == Error::BelowRange);
    }
    CHECK(rank >= previous_rank); // the zones are contiguous runs at the ends, in order
    previous_rank = rank;
    if (r.ok()) {
      ++ok_codes;
      const int64_t t = r.value.temperature_mc;
      if (have_t) {
        // along rising code a low-side divider cools (R rises), a high-side one
        // heats
        CHECK(topology == Topology::NtcLowSide ? t <= previous_t : t >= previous_t);
      }
      previous_t = t;
      have_t = true;
    }
  }
  CHECK(ok_codes > 8);
}

TEST_CASE("Every code of a 12-bit and a 16-bit converter: zones in order, "
          "temperature monotone") {
  for (const Part& p : Parts()) {
    for (const Topology topology : kTopologies) {
      CheckSweep(p, 12, topology);
      CheckSweep(p, 16, topology);
    }
  }
}

TEST_CASE("Round trip: temperature to resistance to code to Convert, for 5000 "
          "random temperatures "
          "per part") {
  ntc_thermistor_test::Rng rng(0xC0FFEE);
  for (const Part& p : Parts()) {
    const Reference ref(p);
    for (const Topology topology : kTopologies) {
      Divider d;
      d.series_ohm = p.series_ohm;
      d.bits = 12;
      d.topology = topology;
      const Config cfg = MakeConfig(p, d);
      int checked = 0;
      for (int i = 0; i < 5000; ++i) {
        const Real t = rng.Between(p.t_lo_c + 3.0L, p.t_hi_c - 3.0L);
        Real r = 0.0L;
        if (p.law == Law::Rows) {
          Real lo = static_cast<Real>(p.rows.back().resistance_ohm);
          Real hi = static_cast<Real>(p.rows.front().resistance_ohm);
          for (int j = 0; j < 100; ++j) {
            const Real mid = (lo + hi) / 2.0L;
            (ref.T(mid) > t ? lo : hi) = mid;
          }
          r = (lo + hi) / 2.0L;
        } else {
          r = ref.R(t);
        }
        const uint32_t code = CodeFor(d, r);
        const Real lsb = LsbKelvin(ref, d, code);
        if ((t - p.t_lo_c < 2.0L * lsb) || (p.t_hi_c - t < 2.0L * lsb)) {
          continue;
        }
        const auto got = ntc_thermistor::Convert(cfg, code);
        REQUIRE(got.ok());
        CHECK(std::fabs((static_cast<Real>(got.value.temperature_mc) / 1000.0L) - t) <=
              Bound(p, ref, d, code, r) + lsb);
        ++checked;
      }
      CHECK(checked > 3000);
    }
  }
}

TEST_CASE("Symmetry: low-side code c and high-side code 2^bits - c give the "
          "same resistance within "
          "an ohm") {
  for (const Part& p : Parts()) {
    for (const uint32_t bits : {8U, 12U, 16U}) {
      Divider low;
      low.series_ohm = p.series_ohm;
      low.bits = bits;
      Divider high = low;
      high.topology = Topology::NtcHighSide;
      const Config lo_cfg = MakeConfig(p, low);
      const Config hi_cfg = MakeConfig(p, high);
      const uint32_t scale = 1U << bits;
      const uint32_t step = (scale / 251U) + 1U;
      for (uint32_t c = 4; c < scale - 4U; c += step) {
        const auto a = ntc_thermistor::ConvertResistance(lo_cfg, c);
        const auto b = ntc_thermistor::ConvertResistance(hi_cfg, scale - c);
        REQUIRE(a.ok() && b.ok());
        const int64_t diff = static_cast<int64_t>(a.value) - static_cast<int64_t>(b.value);
        CHECK(diff >= -1 && diff <= 1);
      }
    }
  }
}

TEST_CASE("Reference invariance: scaling the supply and the reference together "
          "changes nothing") {
  for (const Part& p : Parts()) {
    Divider d;
    d.series_ohm = p.series_ohm;
    const Config ratio = MakeConfig(p, d);
    Config a = ratio;
    a.adc_vref_mv = 3300;
    a.divider_supply_mv = 3300;
    Config b = ratio;
    b.adc_vref_mv = 5000;
    b.divider_supply_mv = 5000;
    for (uint32_t code = 100; code < 4000U; code += 37U) {
      const auto x = ntc_thermistor::Convert(ratio, code);
      const auto y = ntc_thermistor::Convert(a, code);
      const auto z = ntc_thermistor::Convert(b, code);
      CHECK_EQ(x.error, y.error);
      CHECK_EQ(x.error, z.error);
      CHECK_EQ(x.value.temperature_mc, y.value.temperature_mc);
      CHECK_EQ(x.value.temperature_mc, z.value.temperature_mc);
      CHECK_EQ(x.value.resistance_ohm, z.value.resistance_ohm);
    }
  }
}

TEST_CASE("Separate supply and reference: 3.3 V on a 2.5 V reference, 5 V on a "
          "3.3 V reference") {
  struct Case {
    uint32_t supply;
    uint32_t vref;
  };
  for (const Case c : {Case{3300, 2500}, Case{5000, 3300}}) {
    for (const Part& p : Parts()) {
      if (p.law == Law::Rows) {
        continue; // no formula
      }
      const Reference ref(p);
      Divider d;
      d.series_ohm = p.series_ohm;
      d.supply_mv = c.supply;
      d.vref_mv = c.vref;
      d.bits = 12;
      const Config cfg = MakeConfig(p, d);
      int checked = 0;
      for (const Real t : Sweep(p.t_lo_c + 8.0L, p.t_hi_c - 8.0L, 3.3L)) {
        const Real r = p.law == Law::Rows ? Real{0} : ref.R(t);
        if (p.law == Law::Rows) {
          continue;
        }
        const uint32_t code = CodeFor(d, r);
        const auto got = ntc_thermistor::Convert(cfg, code);
        if (!got.ok()) {
          continue; // the converter's range does not reach this part of the
                    // curve: a wiring choice
        }
        CHECK(std::fabs((static_cast<Real>(got.value.temperature_mc) / 1000.0L) - t) <=
              Bound(p, ref, d, code, r) + LsbKelvin(ref, d, code));
        ++checked;
      }
      CHECK(checked > 10);
      // a wrong supply value moves the result by the amount the divider
      // predicts
      Config wrong = cfg;
      wrong.divider_supply_mv = c.supply + (c.supply / 10U);
      const uint32_t code =
          CodeFor(d, p.law == Law::Rows ? static_cast<Real>(p.series_ohm) : ref.R(25.0L));
      const auto good = ntc_thermistor::ConvertResistance(cfg, code);
      const auto bad = ntc_thermistor::ConvertResistance(wrong, code);
      REQUIRE(good.ok() && bad.ok());
      CHECK(good.value != bad.value);
    }
  }
}

TEST_CASE("No wrap at the bounds: the 64-bit resistance equals a long-double check") {
  const Part& p = Parts()[0];
  for (const uint32_t bits : {8U, 24U}) {
    for (const uint32_t rs : {1U, 1000U, 16'777'216U}) {
      for (const Topology topology : kTopologies) {
        Divider d;
        d.series_ohm = rs;
        d.bits = bits;
        d.topology = topology;
        d.supply_mv = 20000;
        d.vref_mv = 19999;
        Config cfg = MakeConfig(p, d);
        cfg.open_margin_codes = 1;
        cfg.short_margin_codes = 1;
        const uint32_t scale = 1U << bits;
        const uint32_t codes[7] = {2,          3,          scale / 4U, scale / 2U,
                                   scale - 4U, scale - 3U, scale - 2U};
        for (const uint32_t code : codes) {
          const auto got = ntc_thermistor::ConvertResistance(cfg, code);
          const Real exact = ResistanceOf(d, static_cast<Real>(code));
          if (got.ok()) {
            CHECK(std::fabs(static_cast<Real>(got.value) - exact) <= 1.0L);
          } else {
            // beyond 32 bits, a fault, or a node the supply cannot reach: never
            // a wrapped number
            CHECK(got.error == Error::BelowRange || got.error == Error::OpenCircuit ||
                  got.error == Error::ShortCircuit);
            CHECK((got.error != Error::BelowRange) || (exact <= 0.0L) || (exact >= 4.0e9L) ||
                  !std::isfinite(static_cast<double>(exact)));
          }
        }
      }
    }
  }
}

TEST_CASE("Pure: equal inputs give equal outputs, and two drivers do not interact") {
  const Part& p = Parts()[3];
  Divider d;
  d.series_ohm = p.series_ohm;
  const Config cfg = MakeConfig(p, d);
  const auto a = ntc_thermistor::Convert(cfg, 1700);
  const auto b = ntc_thermistor::Convert(cfg, 1700);
  CHECK_EQ(a.value.temperature_mc, b.value.temperature_mc);
  CHECK_EQ(a.value.uncertainty_mc, b.value.uncertainty_mc);
  ntc_thermistor_test::MinimalPort pa;
  ntc_thermistor_test::MinimalPort pb;
  pa.state.Fixed(1700);
  pb.state.Fixed(900);
  ntc_thermistor::NtcThermistor<ntc_thermistor_test::MinimalPort> da(pa, cfg);
  ntc_thermistor::NtcThermistor<ntc_thermistor_test::MinimalPort> db(pb, cfg);
  REQUIRE(da.EnsureInitialized().ok() && db.EnsureInitialized().ok());
  const auto ra = da.Read();
  const auto rb = db.Read();
  const auto ra2 = da.Read();
  CHECK_EQ(ra.value.temperature_mc, a.value.temperature_mc);
  CHECK_EQ(ra2.value.temperature_mc, ra.value.temperature_mc);
  CHECK(rb.value.temperature_mc != ra.value.temperature_mc);
}

} // namespace
