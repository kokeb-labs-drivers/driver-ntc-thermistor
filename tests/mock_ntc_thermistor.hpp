/**
 * @file mock_ntc_thermistor.hpp
 * @brief The model: synthetic thermistors, the divider in front of an ADC, and
 * the mock ports.
 *
 * Written from the equations (the beta equation, Steinhart-Hart, the resistor
 * divider) and shares no code or constant with `inc/`: a mistake in the driver
 * is not copied into its model. A part is a `Part`, a few numbers; the suite
 * runs every case over the same set of parts, so no test depends on one part.
 * Arithmetic is `long double`, exact for the purpose.
 *
 * | Part | Law | Mode of the driver | What it checks |
 * | --- | --- | --- | --- |
 * | P1 | beta 3950, 10 kOhm | beta | the generic class |
 * | P2 | beta 4250, 100 kOhm | beta | high R25 |
 * | P3 | beta 3100, 2.2 kOhm | beta | low R25, hot resistances of a few ohms |
 * | P4 | Steinhart-Hart, 47 kOhm | table, 2.5 degC rows | a curve that is not
 * beta | | P5 | Steinhart-Hart, 10 kOhm | table, uneven rows | coarse and
 * uneven spacing | | P6 | beta 3380, 10 kOhm at 20 degC | beta | a reference
 * other than 25 degC | | P7 | the TDK table, typed again | table | the example
 * preset's data |
 */
#ifndef NTC_THERMISTOR_MOCK_NTC_THERMISTOR_HPP
#define NTC_THERMISTOR_MOCK_NTC_THERMISTOR_HPP

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "ntc_thermistor_adc_interface.hpp"
#include "ntc_thermistor_types.hpp"

namespace ntc_thermistor_test {

using Real = long double;
using ntc_thermistor::Topology;

/// 0 degC in kelvin.
inline constexpr Real kZeroC = 273.15L;

// --- The synthetic parts
// ------------------------------------------------------------------------

/// How the part's true curve is generated.
enum class Law : uint8_t { Beta, SteinhartHart, Rows };

/// A synthetic thermistor: the numbers a datasheet gives, and how the driver is
/// told about it.
// A test record whose fields are ordered by meaning, not by size.
struct Part { // NOLINT(clang-analyzer-optin.performance.Padding)
  const char* name{""};
  Real r0{10000.0L}; ///< Resistance at `t0_c`.
  Real t0_c{25.0L};  ///< Reference temperature.
  Law law{Law::Beta};
  Real beta{3950.0L}; ///< Beta law.
  Real sh_a{0.0L};    ///< Steinhart-Hart: 1/T = a + b ln R + c (ln R)^3.
  Real sh_b{0.0L};
  Real sh_c{0.0L};
  bool table_mode{false}; ///< The driver gets a table (else R0 and B).
  Real t_lo_c{-40.0L};    ///< Lowest temperature of the table and of the window.
  Real t_hi_c{125.0L};    ///< Highest.
  Real step_c{5.0L};      ///< Row spacing of a generated table.
  bool uneven{false};     ///< Rows every 5 degC below 0 degC and every 10 degC above (P5).
  Real b_t2_c{85.0L};     ///< The second temperature of the beta pair.
  uint32_t series_ohm{10000};
  /// Largest interpolation error between rows, in kelvin, measured over 19
  /// interior points per segment (tools: the measuring loop in test_model.cpp)
  /// and pinned with 20 % headroom; for a part whose table is generated from a
  /// beta curve the bound is 0.0013 * step^2 + 0.003 instead.
  Real interp_k{0.0L};
  std::vector<ntc_thermistor::TablePoint> rows; ///< The table the driver is given (table mode).
};

/// R at `t_c` by the beta equation: R = R0 exp(B (1/T - 1/T0)).
inline Real BetaR(const Part& p, Real t_c) {
  return p.r0 * std::exp(p.beta * (1.0L / (t_c + kZeroC) - 1.0L / (p.t0_c + kZeroC)));
}

/// T (degC) at resistance `r` by the beta equation.
inline Real BetaT(const Part& p, Real r) {
  return (1.0L / ((1.0L / (p.t0_c + kZeroC)) + (std::log(r / p.r0) / p.beta))) - kZeroC;
}

/// T (degC) at resistance `r` by Steinhart-Hart.
inline Real ShT(const Part& p, Real r) {
  const Real x = std::log(r);
  return (1.0L / (p.sh_a + (p.sh_b * x) + (p.sh_c * x * x * x))) - kZeroC;
}

/// R at `t_c` by Steinhart-Hart: the root of the cubic, by bisection (the cubic
/// rises with ln R).
inline Real ShR(const Part& p, Real t_c) {
  const Real want = 1.0L / (t_c + kZeroC);
  Real lo = std::log(1.0L);
  Real hi = std::log(1.0e9L);
  for (int i = 0; i < 200; ++i) {
    const Real mid = (lo + hi) / 2.0L;
    const Real f = p.sh_a + (p.sh_b * mid) + (p.sh_c * mid * mid * mid);
    if (f > want) {
      hi = mid;
    } else {
      lo = mid;
    }
  }
  return std::exp((lo + hi) / 2.0L);
}

/// A monotone cubic (Fritsch-Carlson) through points ascending in x: the
/// model's reference for a table that has no formula behind it.
class Pchip {
public:
  Pchip() = default;
  /// Build from ascending `x` and the values `y`.
  Pchip(std::vector<Real> x, std::vector<Real> y) : x_(std::move(x)), y_(std::move(y)) {
    const std::size_t n = x_.size();
    std::vector<Real> delta(n - 1U);
    for (std::size_t i = 0; i + 1U < n; ++i) {
      delta[i] = (y_[i + 1U] - y_[i]) / (x_[i + 1U] - x_[i]);
    }
    d_.assign(n, 0.0L);
    d_[0] = delta[0];
    d_[n - 1U] = delta[n - 2U];
    for (std::size_t i = 1; i + 1U < n; ++i) {
      if (delta[i - 1U] * delta[i] <= 0.0L) {
        d_[i] = 0.0L;
      } else {
        const Real h0 = x_[i] - x_[i - 1U];
        const Real h1 = x_[i + 1U] - x_[i];
        const Real w1 = (2.0L * h1) + h0;
        const Real w2 = h1 + (2.0L * h0);
        d_[i] = (w1 + w2) / ((w1 / delta[i - 1U]) + (w2 / delta[i]));
      }
    }
  }
  /// The interpolant at `x` (inside the span).
  [[nodiscard]] Real At(Real x) const {
    std::size_t i = 0;
    while ((i + 2U < x_.size()) && (x > x_[i + 1U])) {
      ++i;
    }
    const Real h = x_[i + 1U] - x_[i];
    const Real t = (x - x_[i]) / h;
    const Real t2 = t * t;
    const Real t3 = t2 * t;
    return (((2.0L * t3) - (3.0L * t2) + 1.0L) * y_[i]) + ((t3 - (2.0L * t2) + t) * h * d_[i]) +
           (((-2.0L * t3) + (3.0L * t2)) * y_[i + 1U]) + ((t3 - t2) * h * d_[i + 1U]);
  }

private:
  std::vector<Real> x_;
  std::vector<Real> y_;
  std::vector<Real> d_;
};

/// The temperatures (degC) of the rows of a generated table.
inline std::vector<Real> RowTemperatures(const Part& p) {
  std::vector<Real> t;
  Real x = p.t_lo_c;
  while (x <= p.t_hi_c + 1.0e-9L) {
    t.push_back(x);
    Real step = p.step_c;
    if (p.uneven) {
      step = (x < -1.0e-9L) ? 5.0L : 10.0L;
    }
    x += step;
  }
  return t;
}

/// R at `t_c` for the part's true curve (not for `Law::Rows`, which has only
/// its rows).
inline Real TrueR(const Part& p, Real t_c) {
  return p.law == Law::Beta ? BetaR(p, t_c) : ShR(p, t_c);
}

/// T at `r` for the part's true curve (not for `Law::Rows`).
inline Real TrueT(const Part& p, Real r) {
  return p.law == Law::Beta ? BetaT(p, r) : ShT(p, r);
}

/// The rows of a generated table: the true curve sampled near each nominal
/// temperature, R rounded to a whole ohm as a datasheet prints it and T the
/// curve's value at that R, so that every row lies on the curve and the
/// interpolation error between rows is not mixed with the rounding of the rows.
inline std::vector<ntc_thermistor::TablePoint> MakeRows(const Part& p) {
  std::vector<ntc_thermistor::TablePoint> rows;
  for (const Real t : RowTemperatures(p)) {
    const Real r = std::round(TrueR(p, t));
    rows.push_back(
        {static_cast<uint32_t>(r), static_cast<int32_t>(std::lround(TrueT(p, r) * 1000.0L))});
  }
  return rows;
}

/// The TDK table typed again, as two columns: the model's own second copy of
/// the sheet's values.
inline const std::vector<ntc_thermistor::TablePoint>& TdkRows() {
  static const std::vector<ntc_thermistor::TablePoint> kRows = [] {
    const int celsius[39] = {-40, -35, -30, -25, -20, -15, -10, -5,  0,   5,   10,  15,  20,
                             25,  30,  35,  40,  45,  50,  55,  60,  65,  70,  75,  80,  85,
                             90,  95,  100, 105, 110, 115, 120, 125, 130, 135, 140, 145, 150};
    const uint32_t ohms[39] = {188500, 144300, 111300, 86600, 67800, 53460, 42450, 33930,
                               27280,  22070,  17960,  14700, 12090, 10000, 8310,  6940,
                               5826,   4911,   4158,   3536,  3019,  2588,  2227,  1924,
                               1668,   1451,   1267,   1110,  975,   860,   760,   674,
                               599,    534,    478,    428,   385,   346,   313};
    std::vector<ntc_thermistor::TablePoint> r;
    r.reserve(39);
    for (std::size_t i = 0; i < 39; ++i) {
      r.push_back({ohms[i], celsius[i] * 1000});
    }
    return r;
  }();
  return kRows;
}

/// The seven parts. Built once; the rows live as long as the program.
inline const std::vector<Part>& Parts() {
  static const std::vector<Part> kParts = [] {
    std::vector<Part> v;
    Part p1;
    p1.name = "P1 10k B3950";
    p1.b_t2_c = 50.0L;
    v.push_back(p1);
    Part p2;
    p2.name = "P2 100k B4250";
    p2.r0 = 100000.0L;
    p2.beta = 4250.0L;
    p2.series_ohm = 100000;
    v.push_back(p2);
    Part p3;
    p3.name = "P3 2.2k B3100";
    p3.r0 = 2200.0L;
    p3.beta = 3100.0L;
    p3.series_ohm = 2200;
    v.push_back(p3);
    Part p4;
    p4.name = "P4 47k SH table 2.5";
    p4.r0 = 47000.0L;
    p4.law = Law::SteinhartHart;
    p4.sh_a = 0.0005777838523187202L;
    p4.sh_b = 0.00026324576243589166L;
    p4.sh_c = -4.476950218356294e-08L;
    p4.table_mode = true;
    p4.step_c = 2.5L;
    p4.series_ohm = 47000;
    p4.interp_k = 0.0096L;
    v.push_back(p4);
    Part p5;
    p5.name = "P5 10k SH table uneven";
    p5.law = Law::SteinhartHart;
    p5.sh_a = 0.8767633e-3L;
    p5.sh_b = 2.527899e-4L;
    p5.sh_c = 1.906681e-7L;
    p5.table_mode = true;
    p5.uneven = true;
    p5.t_hi_c = 130.0L;
    p5.interp_k = 0.0852L;
    v.push_back(p5);
    Part p6;
    p6.name = "P6 10k at 20C B3380";
    p6.t0_c = 20.0L;
    p6.beta = 3380.0L;
    p6.t_lo_c = -20.0L;
    p6.t_hi_c = 110.0L;
    p6.b_t2_c = 80.0L;
    v.push_back(p6);
    Part p7;
    p7.name = "P7 TDK table";
    p7.law = Law::Rows;
    p7.table_mode = true;
    p7.t_hi_c = 150.0L;
    p7.interp_k = 0.0279L;
    v.push_back(p7);
    for (Part& p : v) {
      if (p.law == Law::Rows) {
        p.rows = TdkRows();
      } else if (p.table_mode) {
        p.rows = MakeRows(p);
      } else {
        p.rows.clear();
      }
    }
    return v;
  }();
  return kParts;
}

/// The model's reference curve of `p`: R (ohm) to T (degC), exact for a law, a
/// monotone cubic through the rows in ln R for a part that is only rows.
class Reference {
public:
  explicit Reference(const Part& p) : part_(p) {
    if (p.law == Law::Rows) {
      std::vector<Real> x;
      std::vector<Real> y;
      for (std::size_t i = p.rows.size(); i > 0U; --i) {
        x.push_back(std::log(static_cast<Real>(p.rows[i - 1U].resistance_ohm)));
        y.push_back(static_cast<Real>(p.rows[i - 1U].temperature_mc) / 1000.0L);
      }
      cubic_ = Pchip(x, y);
    }
  }
  /// T (degC) at `r`.
  [[nodiscard]] Real T(Real r) const {
    return part_.law == Law::Rows ? cubic_.At(std::log(r)) : TrueT(part_, r);
  }
  /// R (ohm) at `t_c` for a part with a formula.
  [[nodiscard]] Real R(Real t_c) const {
    return TrueR(part_, t_c);
  }

private:
  const Part& part_;
  Pchip cubic_;
};

// --- The divider and the ADC
// --------------------------------------------------------------------

/// The divider and converter in front of the node.
struct Divider {
  Topology topology{Topology::NtcLowSide};
  uint32_t series_ohm{10000};
  uint32_t bits{12};
  uint32_t supply_mv{0}; ///< 0: ratiometric (the reference is the supply).
  uint32_t vref_mv{0};
};

/// The node's share of the supply for a thermistor of `r` ohms.
inline Real NodeShare(const Divider& d, Real r) {
  const Real rs = static_cast<Real>(d.series_ohm);
  return d.topology == Topology::NtcLowSide ? r / (rs + r) : rs / (rs + r);
}

/// The full scale count, 2^bits.
inline Real Scale(const Divider& d) {
  return std::ldexp(1.0L, static_cast<int>(d.bits));
}

/// The real-valued code the converter would show for a thermistor of `r` ohms
/// (before flooring).
inline Real RealCode(const Divider& d, Real r) {
  const Real share = NodeShare(d, r);
  const Real volts = d.supply_mv == 0U
                         ? share
                         : (share * static_cast<Real>(d.supply_mv) / static_cast<Real>(d.vref_mv));
  return Scale(d) * volts;
}

/// The values `lo, lo + step, ...` below `hi`, computed from an index (no
/// floating-point loop counter).
inline std::vector<Real> Sweep(Real lo, Real hi, Real step) {
  std::vector<Real> v;
  const auto n = static_cast<std::size_t>(std::floor((hi - lo) / step)) + 1U;
  v.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    const Real x = lo + (step * static_cast<Real>(i));
    if (x < hi) {
      v.push_back(x);
    }
  }
  return v;
}

/// The values `lo, lo * ratio, ...` below `hi`.
inline std::vector<Real> GeoSweep(Real lo, Real hi, Real ratio) {
  std::vector<Real> v;
  const auto n = static_cast<std::size_t>(std::floor(std::log(hi / lo) / std::log(ratio))) + 1U;
  v.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    const Real x = lo * std::pow(ratio, static_cast<Real>(i));
    if (x < hi) {
      v.push_back(x);
    }
  }
  return v;
}

/// The converter's code for `r`: floor, clamped to 0 .. 2^bits - 1.
inline uint32_t CodeFor(const Divider& d, Real r) {
  const Real c = std::floor(RealCode(d, r));
  const Real top = Scale(d) - 1.0L;
  Real clamped = (c < 0.0L) ? 0.0L : c;
  clamped = (clamped > top) ? top : clamped;
  return static_cast<uint32_t>(clamped);
}

/// The resistance a real-valued `code` stands for (the divider inverted).
inline Real ResistanceOf(const Divider& d, Real code) {
  Real x = code / Scale(d);
  if (d.supply_mv != 0U) {
    x = x * static_cast<Real>(d.vref_mv) / static_cast<Real>(d.supply_mv);
  }
  const Real rs = static_cast<Real>(d.series_ohm);
  return d.topology == Topology::NtcLowSide ? rs * x / (1.0L - x) : rs * (1.0L - x) / x;
}

/// A driver `Config` for `part` behind `d`, with the part's table or beta
/// description and window.
inline ntc_thermistor::Config MakeConfig(const Part& p, const Divider& d) {
  ntc_thermistor::Config c{};
  if (p.table_mode) {
    c.curve.table = std::span<const ntc_thermistor::TablePoint>(p.rows);
  } else {
    c.min_temperature_mc = static_cast<int32_t>(std::lround(p.t_lo_c * 1000.0L));
    c.max_temperature_mc = static_cast<int32_t>(std::lround(p.t_hi_c * 1000.0L));
  }
  c.curve.r0_ohm = static_cast<uint32_t>(std::lround(p.r0));
  c.curve.ref_temperature_mc = static_cast<int32_t>(std::lround(p.t0_c * 1000.0L));
  c.curve.b_kelvin = p.law == Law::Beta ? static_cast<uint32_t>(std::lround(p.beta)) : 0U;
  c.curve.b_t2_mc = static_cast<int32_t>(std::lround(p.b_t2_c * 1000.0L));
  c.adc_bits = static_cast<uint8_t>(d.bits);
  c.topology = d.topology;
  c.series_ohm = d.series_ohm;
  c.adc_vref_mv = d.vref_mv;
  c.divider_supply_mv = d.supply_mv;
  if (d.supply_mv != 0U) {
    c.open_margin_codes = 2;
    c.short_margin_codes = 2;
  }
  return c;
}

/// The temperature step one converter code makes at `code`, in kelvin: the
/// larger of the two neighbouring differences of the model's reference (codes 1
/// to 2^bits - 2).
inline Real LsbKelvin(const Reference& ref, const Divider& d, uint32_t code) {
  const Real c = static_cast<Real>(code);
  const Real here = ref.T(ResistanceOf(d, c));
  const Real up = std::fabs(ref.T(ResistanceOf(d, c + 1.0L)) - here);
  const Real down = std::fabs(here - ref.T(ResistanceOf(d, c - 1.0L)));
  return up > down ? up : down;
}

/// How much a whole-ohm rounding of R moves T at `r`, in kelvin (half an ohm
/// each way).
inline Real RoundingKelvin(const Reference& ref, Real r) {
  return std::fabs(ref.T(r + 0.5L) - ref.T(r - 0.5L)) / 2.0L;
}

/// A fixed-seed generator for the property tests: the same inputs on every run
/// and platform.
class Rng {
public:
  explicit Rng(uint64_t seed) : state_(seed) {}
  /// The next 32 random bits.
  uint32_t Next() {
    state_ ^= state_ << 13U;
    state_ ^= state_ >> 7U;
    state_ ^= state_ << 17U;
    return static_cast<uint32_t>(state_ >> 11U);
  }
  /// A real in [lo, hi).
  Real Between(Real lo, Real hi) {
    return lo + ((hi - lo) * static_cast<Real>(Next()) / 4294967296.0L);
  }

private:
  uint64_t state_;
};

// --- The mock ports
// -----------------------------------------------------------------------------

/// What the port was asked, in order.
enum class Op : uint8_t { Init, Read, Delay, BiasOn, BiasOff };

/// One port call.
struct Event {
  Op op{Op::Init};
  uint32_t arg{0}; ///< The channel of a read, the microseconds of a delay.
};

/// The ADC behind the divider: what it returns, when it fails, and a record of
/// every call.
struct AdcState {
  std::vector<uint32_t> codes{2048}; ///< Returned in turn; the last one repeats.
  std::size_t next{0};
  int fail_read_at{-1};    ///< Fail the read with this 0-based index.
  int fail_bias_on_at{-1}; ///< Fail the n-th `SetBias(true)`.
  bool bias_off_ok{true};  ///< `SetBias(false)` result.
  bool init_ok{true};      ///< `EnsureInitialized()` result.
  uint32_t noise{0};       ///< Add up to +-noise codes, from a seeded generator.
  uint32_t seed{12345};
  int reads{0};
  int bias_on_calls{0};
  std::vector<Event> log;

  /// Count the events of a kind.
  [[nodiscard]] int Count(Op op) const {
    int n = 0;
    for (const Event& e : log) {
      n += (e.op == op) ? 1 : 0;
    }
    return n;
  }
  /// Microseconds waited in total.
  [[nodiscard]] uint64_t WaitedUs() const {
    uint64_t sum = 0;
    for (const Event& e : log) {
      sum += (e.op == Op::Delay) ? e.arg : 0U;
    }
    return sum;
  }
  /// Set the code every read returns.
  void Fixed(uint32_t code) {
    codes.assign(1, code);
    next = 0;
  }

  /// The port's conversion.
  bool Read(uint8_t channel, uint32_t& code) {
    log.push_back({Op::Read, channel});
    const int index = reads++;
    if (index == fail_read_at) {
      return false;
    }
    uint32_t value = codes[next];
    next = (next + 1U < codes.size()) ? next + 1U : next;
    if (noise > 0U) {
      seed = (seed * 1664525U) + 1013904223U;
      const uint32_t span = (2U * noise) + 1U;
      const uint32_t draw = (seed >> 8U) % span;
      value = (value + draw >= noise) ? (value + draw - noise) : 0U;
    }
    code = value;
    return true;
  }
};

/// The required hooks over an `AdcState`. Derive `class P : public
/// PortBase<P>`.
template <typename Self>
class PortBase : public ntc_thermistor::AdcInterface<Self> {
public:
  AdcState state{};

  bool EnsureInitialized() noexcept {
    state.log.push_back({Op::Init, 0});
    return state.init_ok;
  }
  bool ReadRaw(uint8_t channel, uint32_t& code) noexcept {
    return state.Read(channel, code);
  }
};

/// The required hooks only.
class MinimalPort : public PortBase<MinimalPort> {};

/// The required hooks and `DelayUs()`.
class DelayPort : public PortBase<DelayPort> {
public:
  void DelayUs(uint32_t us) noexcept {
    state.log.push_back({Op::Delay, us});
  }
};

/// Every optional hook.
class BiasPort : public PortBase<BiasPort> {
public:
  void DelayUs(uint32_t us) noexcept {
    state.log.push_back({Op::Delay, us});
  }
  bool SetBias(bool on) noexcept {
    state.log.push_back({on ? Op::BiasOn : Op::BiasOff, 0});
    if (on) {
      return state.bias_on_calls++ != state.fail_bias_on_at;
    }
    return state.bias_off_ok;
  }
};

/// `SetBias()` without `DelayUs()`.
class BiasNoDelayPort : public PortBase<BiasNoDelayPort> {
public:
  bool SetBias(bool on) noexcept {
    state.log.push_back({on ? Op::BiasOn : Op::BiasOff, 0});
    return true;
  }
};

} // namespace ntc_thermistor_test

#endif // NTC_THERMISTOR_MOCK_NTC_THERMISTOR_HPP
