/**
 * @file fuzz_ntc_thermistor.cpp
 * @brief Fuzz target: the driver over an ADC whose codes the fuzzer controls.
 *
 * What the driver reads is a code from the port's ADC, so the code is the
 * untrusted input. The fuzzer's bytes choose a configuration (the divider, the
 * sampling, a curve that is a beta description, the example preset, the generic
 * preset, a table built from fuzzed pairs, Steinhart-Hart coefficients, or
 * none), then a sequence of driver calls; for every conversion they also choose
 * what the ADC does: pass the code the divider model gives for the simulated
 * resistance, fail, return a rail code, a value above scale, or a code that
 * wobbles between samples.
 *
 * After every call the target checks the driver's promises (docs/contract.md),
 * and aborts, naming the line, when one breaks:
 *
 * - a failed call carries an `Error` this driver returns and a zeroed value; an
 * error type fits the code (a node code at the open rail is never `Ok`, never
 * the other fault);
 * - before `EnsureInitialized()` succeeds no port call is made;
 * `InvalidArgument` means none was made; an empty curve, or a beta curve
 * without a window, is always `InvalidArgument`;
 * - an `Ok` temperature lies inside the configured window and, in table mode,
 * inside the table's span plus the offset; an `Ok` resistance lies inside the
 * table's resistance range; in beta mode the temperature is above absolute zero
 * before the offset;
 * - `Ok` never comes from a failed sample, a sample at or above 2^bits, or a
 * spread above `max_spread_codes`; `uncertainty_mc` is at least 1 and the mean
 * code is the rounded mean;
 * - `SetBias` is switched on and off the same number of times in a `Read`, the
 * last call is off, also on every error path;
 * - over 64 probe codes taken from the input, sorted, `Convert` is monotone and
 * its errors come as contiguous runs in order;
 * - `ReadRawCode()` makes at most one conversion in the same bias sequence, and
 * `GetLastCode()` is the last code the port returned;
 * - every ADC call has `channel == Config::adc_channel`, and a `Read` makes at
 * most `sample_count` reads; a bad `SetConfig` leaves the old configuration.
 *
 * Built two ways (tests/fuzz/CMakeLists.txt): with libFuzzer (Clang,
 * `-DNTC_THERMISTOR_BUILD_FUZZERS=ON`), and as a plain executable that replays
 * the corpus in tests/fuzz/corpus/ as a CTest on any compiler.
 */
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>

#include "mock_ntc_thermistor.hpp"
#include "ntc_thermistor.hpp"
#include "ntc_thermistor_presets.hpp"

namespace {

using ntc_thermistor::Config;
using ntc_thermistor::Error;
using ntc_thermistor::Topology;

/// The fuzzer's bytes, read front to back; zeros once they run out.
class Input {
public:
  Input(const uint8_t* data, std::size_t size) noexcept : data_(data), size_(size) {}

  [[nodiscard]] bool Empty() const noexcept {
    return pos_ >= size_;
  }
  uint8_t Byte() noexcept {
    return pos_ < size_ ? data_[pos_++] : uint8_t{0};
  }
  uint32_t Word() noexcept {
    const uint32_t high = Byte();
    return (high << 8U) | Byte();
  }
  uint32_t Triple() noexcept {
    const uint32_t high = Byte();
    return (high << 16U) | Word();
  }

private:
  const uint8_t* data_;
  std::size_t size_;
  std::size_t pos_ = 0;
};

/// A broken promise: name the line and abort, so libFuzzer saves the input as a
/// crash.
void Check(bool promise, int line = __builtin_LINE()) noexcept {
  if (!promise) {
    (void)std::fprintf(stderr, "fuzz_ntc_thermistor: promise broken at line %d\n", line);
    std::abort();
  }
}

/// What the ADC does with one conversion, chosen by the fuzzer; any other byte
/// passes the model's code.
constexpr uint8_t kFail = 0xFF;       ///< The conversion fails.
constexpr uint8_t kLowRail = 0xFE;    ///< The code is 0.
constexpr uint8_t kHighRail = 0xFD;   ///< The code is full scale - 1.
constexpr uint8_t kAboveScale = 0xFC; ///< The code is 2^bits plus the next byte.
constexpr uint8_t kWobble = 0xFB;     ///< The model's code plus or minus a few.

/// The port: a divider and converter model behind hooks the fuzzer controls,
/// with a record of use.
class FuzzPort : public ntc_thermistor::AdcInterface<FuzzPort> {
public:
  explicit FuzzPort(Input& in) noexcept : in_(in) {}

  bool EnsureInitialized() noexcept {
    ++inits_;
    return (in_.Byte() & 0x07U) != 0U;
  }

  bool ReadRaw(uint8_t channel, uint32_t& code) noexcept {
    Check(channel == channel_);
    ++reads_;
    const uint8_t action = in_.Byte();
    if (action == kFail) {
      return false;
    }
    const uint32_t scale = uint32_t{1} << bits_;
    if (action == kLowRail) {
      code = 0U;
    } else if (action == kHighRail) {
      code = scale - 1U;
    } else if (action == kAboveScale) {
      code = scale + in_.Byte();
    } else {
      code = modelCode();
      if (action == kWobble) {
        const uint32_t jitter = in_.Byte() % 7U;
        code = (code + jitter >= 3U) ? (code + jitter - 3U) : 0U;
        code = (code >= scale) ? (scale - 1U) : code;
      }
    }
    last_codes_[static_cast<std::size_t>(sample_) % last_codes_.size()] = code;
    ++sample_;
    return true;
  }

  void DelayUs(uint32_t us) noexcept {
    (void)us;
    ++delays_;
  }

  bool SetBias(bool on) noexcept {
    on ? ++bias_on_ : ++bias_off_;
    last_bias_on_ = on;
    return (in_.Byte() & 0x1FU) != 0U; // sometimes the pin write fails
  }

  /// Tell the port what the driver is configured with, so its model and checks
  /// follow.
  void Follow(const Config& c) noexcept {
    channel_ = c.adc_channel;
    bits_ = std::min<uint32_t>(std::max<uint32_t>(c.adc_bits, 8U), 24U);
    divider_.topology = c.topology;
    divider_.series_ohm = std::max<uint32_t>(c.series_ohm, 1U);
    divider_.bits = bits_;
    divider_.supply_mv = c.divider_supply_mv;
    divider_.vref_mv = c.adc_vref_mv;
    if ((divider_.supply_mv == 0U) != (divider_.vref_mv == 0U)) {
      divider_.supply_mv = 0U;
      divider_.vref_mv = 0U;
    }
  }

  void SetResistance(uint32_t ohms) noexcept {
    resistance_ = ohms;
  }

  [[nodiscard]] int Reads() const noexcept {
    return reads_;
  }
  [[nodiscard]] int Total() const noexcept {
    return inits_ + reads_ + bias_on_ + bias_off_ + delays_;
  }
  [[nodiscard]] int BiasOn() const noexcept {
    return bias_on_;
  }
  [[nodiscard]] int BiasOff() const noexcept {
    return bias_off_;
  }
  [[nodiscard]] bool LastBiasOn() const noexcept {
    return last_bias_on_;
  }
  [[nodiscard]] int Samples() const noexcept {
    return sample_;
  }
  [[nodiscard]] uint32_t Sample(int i) const noexcept {
    return last_codes_[static_cast<std::size_t>(i) % last_codes_.size()];
  }

private:
  [[nodiscard]] uint32_t modelCode() const noexcept {
    return ntc_thermistor_test::CodeFor(divider_, static_cast<long double>(resistance_));
  }

  Input& in_;
  ntc_thermistor_test::Divider divider_{};
  uint32_t bits_ = 12;
  uint8_t channel_ = 0;
  uint32_t resistance_ = 10000;
  int inits_ = 0;
  int reads_ = 0;
  int delays_ = 0;
  int bias_on_ = 0;
  int bias_off_ = 0;
  bool last_bias_on_ = false;
  int sample_ = 0;
  std::array<uint32_t, 16> last_codes_{};
};

using Driver = ntc_thermistor::NtcThermistor<FuzzPort>;

/// The errors this driver returns; `Timeout`, `DeviceIdMismatch`, `Crc`,
/// `Busy`, `DeviceFault` and `Unsupported` never.
bool KnownError(Error e) noexcept {
  switch (e) {
  case Error::Ok:
  case Error::NotInitialized:
  case Error::InvalidArgument:
  case Error::Bus:
  case Error::OpenCircuit:
  case Error::ShortCircuit:
  case Error::BelowRange:
  case Error::AboveRange:
  case Error::Unstable:
  case Error::CodeOutOfScale:
    return true;
  case Error::Timeout:
  case Error::DeviceIdMismatch:
  case Error::Crc:
  case Error::Busy:
  case Error::DeviceFault:
  case Error::Unsupported:
    return false;
  default:
    break;
  }
  return false;
}

/// The curve data behind a configuration: a table built from the fuzzer's bytes
/// lives here.
struct Material {
  std::array<ntc_thermistor::TablePoint, 8> rows{};
  std::size_t row_count = 0;
};

/// A table from the fuzzer's bytes: valid and monotone when the flag says so,
/// anything otherwise.
void FuzzTable(Input& in, Material& m) noexcept {
  m.row_count = 2U + (in.Byte() % 7U);
  const bool sorted = (in.Byte() & 1U) != 0U;
  uint32_t r = 2000U + (in.Word() * 40U);
  int32_t t = -50'000 + (static_cast<int32_t>(in.Byte()) * 100);
  for (std::size_t i = 0; i < m.row_count; ++i) {
    if (sorted) {
      m.rows[i] = {r, t};
      r -= std::min<uint32_t>(r - 1U, (r / 8U) + in.Byte());
      t += 1000 + (static_cast<int32_t>(in.Byte()) * 20);
    } else {
      m.rows[i] = {in.Triple(), static_cast<int32_t>(in.Word()) - 30'000};
    }
  }
}

/// The curve of a configuration, from the fuzzer's bytes.
ntc_thermistor::Curve FuzzCurve(Input& in, Material& m) noexcept {
  ntc_thermistor::Curve c{};
  switch (in.Byte() % 6U) {
  case 0:
    c.r0_ohm = (in.Word() * 16U) + (in.Byte() % 2U);
    c.b_kelvin = 1000U + (in.Word() % 7500U);
    c.ref_temperature_mc = (static_cast<int32_t>(in.Byte()) - 100) * 1000;
    c.b_t2_mc = (static_cast<int32_t>(in.Byte()) - 50) * 1000;
    break;
  case 1:
    c = ntc_thermistor::presets::kTdkNtcg163jf103ft1s;
    break;
  case 2:
    c = ntc_thermistor::presets::kGeneric10kB3950;
    break;
  case 3:
    FuzzTable(in, m);
    c.table = std::span<const ntc_thermistor::TablePoint>(m.rows.data(), m.row_count);
    c.r0_ohm = 10'000;
    break;
  case 5: // Steinhart-Hart: coefficients that are valid, borderline and refused
    c.sh_a_e15 = static_cast<int64_t>(in.Word()) * 20'000'000;
    c.sh_b_e15 = static_cast<int64_t>(in.Word()) * 4'000'000;
    c.sh_c_e15 = (static_cast<int64_t>(in.Word()) - 32'768) * 10'000;
    break;
  default:
    break; // an empty curve
  }
  c.r_tol_ppm = in.Word() * 8U;
  c.b_tol_ppm = static_cast<uint32_t>(in.Byte()) * 2000U;
  return c;
}

/// The divider and sampling fields of a configuration, from the fuzzer's bytes.
void FuzzDivider(Input& in, Config& c) noexcept {
  const uint8_t flags = in.Byte();
  if ((flags & 0x01U) == 0U) {
    return; // the defaults: 12 bits, 10 kOhm, low side, ratiometric
  }
  c.adc_bits = static_cast<uint8_t>(8U + (in.Byte() % 18U));
  c.topology = static_cast<Topology>(in.Byte() % 3U);
  c.series_ohm = in.Triple() % 16'777'218U;
  c.series_tol_ppm = static_cast<uint32_t>(in.Byte()) * 2000U;
  if ((flags & 0x02U) != 0U) {
    c.adc_vref_mv = 1U + (in.Word() % 20'100U);
    c.divider_supply_mv = 1U + (in.Word() % 20'100U);
  }
  c.open_margin_codes = in.Byte() % 6U;
  c.short_margin_codes = in.Byte() % 6U;
  c.sample_count = static_cast<uint8_t>(in.Byte() % 18U);
  c.sample_delay_us = static_cast<uint32_t>(in.Byte()) * 10U;
  c.settle_us = static_cast<uint32_t>(in.Byte()) * 10U;
  c.max_spread_codes = ((in.Byte() % 4U) == 0U) ? 0U : (in.Byte() % 32U);
  c.offset_mc = static_cast<int32_t>(in.Word() % 40'100U) - 20'050;
  c.adc_channel = static_cast<uint8_t>(in.Byte() % 3U);
}

/// A configuration from the fuzzer's bytes; sometimes invalid on purpose.
Config FuzzConfig(Input& in, Material& m) noexcept {
  Config c{};
  c.curve = FuzzCurve(in, m);
  FuzzDivider(in, c);
  const uint8_t window = in.Byte();
  if ((window & 0x01U) != 0U) {
    c.min_temperature_mc = (static_cast<int32_t>(in.Byte()) - 80) * 1000;
    c.max_temperature_mc = c.min_temperature_mc + (static_cast<int32_t>(in.Byte()) * 1000);
  } else if ((window & 0x02U) != 0U) {
    c.min_temperature_mc = -40'000;
    c.max_temperature_mc = 125'000;
  } else {
    // no window declared
  }
  return c;
}

/// How an error sorts along rising code on a low-side divider; reversed for a
/// high-side one.
int ZoneRank(Error e, Topology topology) noexcept {
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
    rank = 2;
  }
  return topology == Topology::NtcLowSide ? rank : 4 - rank;
}

/// Whether a configuration describes a curve with no part or a beta curve with
/// no window.
bool NoPartOrWindow(const Config& c) noexcept {
  const bool empty = c.curve.table.empty() && (c.curve.sh_b_e15 == 0) &&
                     ((c.curve.r0_ohm == 0U) || (c.curve.b_kelvin == 0U));
  const bool no_window =
      c.curve.table.empty() && ((c.min_temperature_mc == ntc_thermistor::kNoMinTemperatureMc) ||
                                (c.max_temperature_mc == ntc_thermistor::kNoMaxTemperatureMc));
  return empty || no_window;
}

/// The promises about a value that is `Ok`.
void CheckReading(const Config& c, const ntc_thermistor::Reading& r) noexcept {
  const int64_t raw = static_cast<int64_t>(r.temperature_mc) - c.offset_mc;
  const bool saturated = (r.temperature_mc == INT32_MAX) || (r.temperature_mc == INT32_MIN);
  Check(saturated || ((raw >= c.min_temperature_mc) && (raw <= c.max_temperature_mc)));
  Check(r.uncertainty_mc >= 1U);
  Check(r.code < (uint32_t{1} << c.adc_bits));
  if (c.curve.table.empty()) {
    Check(saturated || (raw > ntc_thermistor::kAbsoluteZeroMc)); // no wrapped temperature: T > 0 K
  } else {
    const auto& first = c.curve.table.front();
    const auto& last = c.curve.table.back();
    Check(saturated || ((raw >= first.temperature_mc) && (raw <= last.temperature_mc)));
    Check((r.resistance_ohm >= last.resistance_ohm) && (r.resistance_ohm <= first.resistance_ohm));
  }
}

/// What a read did at the port, measured around one driver call.
struct Probe {
  int total;
  int reads;
  int bias_on;
  int bias_off;
  int samples;
};

Probe Before(const FuzzPort& port) noexcept {
  return {port.Total(), port.Reads(), port.BiasOn(), port.BiasOff(), port.Samples()};
}

/// The mean code of the samples a read took, and whether they were all in scale
/// and within the spread.
struct Samples {
  uint32_t mean = 0;
  bool in_scale = true;
  bool spread_ok = true;
};

Samples Summarise(const FuzzPort& port, const Probe& b, const Config& c) noexcept {
  Samples s;
  const int n = port.Samples() - b.samples;
  uint32_t lo = UINT32_MAX;
  uint32_t hi = 0;
  uint64_t sum = 0;
  for (int i = 0; i < n; ++i) {
    const uint32_t code = port.Sample(b.samples + i);
    s.in_scale = s.in_scale && (code < (uint32_t{1} << c.adc_bits));
    lo = std::min(lo, code);
    hi = std::max(hi, code);
    sum += code;
  }
  if (n > 0) {
    s.mean = static_cast<uint32_t>((sum + (static_cast<uint64_t>(c.sample_count) / 2U)) /
                                   c.sample_count);
    s.spread_ok = (c.max_spread_codes == 0U) || ((hi - lo) <= c.max_spread_codes);
  }
  return s;
}

/// The promises common to `Read()` and `ReadResistance()`, `value_zero` for a
/// failed value.
void CheckRead(const FuzzPort& port, const Probe& b, const Config& c, bool was_up,
               Error error) noexcept {
  Check(KnownError(error));
  if (!was_up) {
    Check((error == Error::NotInitialized) && (port.Total() == b.total));
    return;
  }
  Check(((port.BiasOn() - b.bias_on) == 1) && ((port.BiasOff() - b.bias_off) == 1) &&
        !port.LastBiasOn());
  Check((port.Reads() - b.reads) <= static_cast<int>(c.sample_count));
  Check(error != Error::InvalidArgument);
}

void DoRead(Driver& d, FuzzPort& port, bool was_up) noexcept {
  const Probe b = Before(port);
  const Config c = d.GetConfig();
  const auto r = d.Read();
  CheckRead(port, b, c, was_up, r.error);
  if (!was_up) {
    Check(r.value.temperature_mc == 0);
    return;
  }
  const Samples s = Summarise(port, b, c);
  const int taken = port.Samples() - b.samples;
  const int expected = c.sample_count;
  const bool complete = (taken == expected) && ((port.Reads() - b.reads) == taken);
  if (r.ok()) {
    Check(complete && s.in_scale && s.spread_ok);
    Check(r.value.code == s.mean);
    CheckReading(c, r.value);
    return;
  }
  Check((r.value.temperature_mc == 0) && (r.value.resistance_ohm == 0U) && (r.value.code == 0U) &&
        (r.value.uncertainty_mc == 0U));
  if (r.error == Error::Unstable) {
    Check(complete && s.in_scale && !s.spread_ok);
  } else if (r.error == Error::CodeOutOfScale) {
    Check(!s.in_scale);
  } else if (complete && s.in_scale && s.spread_ok && (r.error != Error::Bus)) {
    // a fault or a range error is exactly what the pure conversion says of the
    // mean code
    Check(ntc_thermistor::Convert(c, s.mean).error == r.error);
  } else {
    Check(r.error == Error::Bus || !complete);
  }
}

void DoReadResistance(Driver& d, FuzzPort& port, bool was_up) noexcept {
  const Probe b = Before(port);
  const Config c = d.GetConfig();
  const auto r = d.ReadResistance();
  CheckRead(port, b, c, was_up, r.error);
  if (!was_up) {
    Check(r.value == 0U);
    return;
  }
  const Samples s = Summarise(port, b, c);
  if (r.ok()) {
    Check(s.in_scale && s.spread_ok);
    Check(ntc_thermistor::ConvertResistance(c, s.mean).value == r.value);
  } else {
    Check(r.value == 0U);
  }
}

void DoReadRaw(Driver& d, FuzzPort& port, bool was_up) noexcept {
  const Probe b = Before(port);
  const Config c = d.GetConfig();
  const auto r = d.ReadRawCode();
  CheckRead(port, b, c, was_up, r.error);
  if (!was_up) {
    Check(r.value == 0U);
    return;
  }
  Check((port.Reads() - b.reads) <= 1);
  Check((r.error == Error::Ok) || (r.error == Error::Bus) || (r.error == Error::CodeOutOfScale));
  if (port.Samples() > b.samples) {
    const uint32_t last = port.Sample(port.Samples() - 1);
    Check(d.GetLastCode() == last);
    Check(!r.ok() || ((r.value == last) && (r.value < (uint32_t{1} << c.adc_bits))));
  }
  Check(r.ok() || (r.value == 0U));
}

void DoStart(Driver& d, FuzzPort& port, bool was_up) noexcept {
  const int total = port.Total();
  const bool valid = ntc_thermistor::Validate(d.GetConfig()) == Error::Ok;
  const auto r = d.EnsureInitialized();
  Check(KnownError(r.error));
  Check(r.ok() == d.IsInitialized());
  if (was_up) {
    Check(r.ok() && (port.Total() == total));
    return;
  }
  Check((r.error == Error::Ok) || (r.error == Error::InvalidArgument) || (r.error == Error::Bus));
  Check((r.error == Error::InvalidArgument) == !valid);
  if (r.error == Error::InvalidArgument) {
    Check(port.Total() == total);
  }
  if (NoPartOrWindow(d.GetConfig())) {
    Check(r.error == Error::InvalidArgument);
  }
}

/// A replacement configuration: the promises of `SetConfig()`.
void DoSetConfig(Driver& d, FuzzPort& port, Input& in, std::array<Material, 2>& mats,
                 std::size_t& active) noexcept {
  const std::size_t attempt = 1U - active;
  const Config next = FuzzConfig(in, mats[attempt]);
  const Config old = d.GetConfig();
  const int total = port.Total();
  const auto r = d.SetConfig(next);
  Check(KnownError(r.error) && (port.Total() == total));
  Check(r.ok() == (ntc_thermistor::Validate(next) == Error::Ok));
  if (NoPartOrWindow(next)) {
    Check(r.error == Error::InvalidArgument);
  }
  const Config now = d.GetConfig();
  if (r.ok()) {
    active = attempt;
    port.Follow(now);
    Check((now.series_ohm == next.series_ohm) && (now.adc_bits == next.adc_bits));
  } else {
    Check((now.series_ohm == old.series_ohm) && (now.adc_bits == old.adc_bits) &&
          (now.offset_mc == old.offset_mc) && (now.curve.table.data() == old.curve.table.data()) &&
          (now.curve.table.size() == old.curve.table.size()) &&
          (now.adc_channel == old.adc_channel));
  }
}

/// 64 probe codes, sorted: `Convert` is monotone and its errors are contiguous
/// runs in order.
void DoConvertProbe(const Driver& d, Input& in) noexcept {
  const Config c = d.GetConfig();
  std::array<uint32_t, 64> codes{};
  for (uint32_t& code : codes) {
    code = (in.Triple() << 1U) ^ in.Byte();
  }
  const bool valid = ntc_thermistor::Validate(c) == Error::Ok;
  if (valid) {
    const uint32_t scale = uint32_t{1} << c.adc_bits;
    for (uint32_t& code : codes) {
      code %= (scale + 4U); // a few above scale as well
    }
  }
  std::sort(codes.begin(), codes.end());
  int previous_rank = -1;
  bool have = false;
  int32_t previous_t = 0;
  for (const uint32_t code : codes) {
    const auto r = ntc_thermistor::Convert(c, code);
    Check(KnownError(r.error));
    if (!valid) {
      Check(r.error == Error::InvalidArgument);
      continue;
    }
    const uint32_t scale = uint32_t{1} << c.adc_bits;
    const bool low_side = c.topology == Topology::NtcLowSide;
    const uint32_t high_margin = low_side ? c.open_margin_codes : c.short_margin_codes;
    const uint32_t low_margin = low_side ? c.short_margin_codes : c.open_margin_codes;
    if (code >= scale) {
      Check(r.error == Error::CodeOutOfScale);
      continue;
    }
    // The node cannot read above 2^bits * supply / reference when the reference is the larger.
    uint64_t rail = scale;
    if ((c.adc_vref_mv != 0U) && (c.divider_supply_mv != 0U) &&
        (c.adc_vref_mv > c.divider_supply_mv)) {
      rail = (static_cast<uint64_t>(scale) * c.divider_supply_mv) / c.adc_vref_mv;
    }
    const auto top = static_cast<uint32_t>((rail > 0U) ? (rail - 1U) : 0U);
    if ((high_margin >= top) || (code >= (top - high_margin))) {
      Check(r.error == (low_side ? Error::OpenCircuit
                                 : Error::ShortCircuit)); // never Ok, never the other fault
    } else if (code <= low_margin) {
      Check(r.error == (low_side ? Error::ShortCircuit : Error::OpenCircuit));
    } else {
      Check(r.error != Error::CodeOutOfScale);
    }
    const int rank = ZoneRank(r.error, c.topology);
    Check(rank >= previous_rank);
    previous_rank = rank;
    if (r.ok()) {
      CheckReading(c, r.value);
      Check(!have || (low_side ? (r.value.temperature_mc <= previous_t)
                               : (r.value.temperature_mc >= previous_t)));
      previous_t = r.value.temperature_mc;
      have = true;
    }
  }
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, std::size_t size) {
  Input in(data, size);
  FuzzPort port(in);
  std::array<Material, 2> mats{};
  std::size_t active = 0;
  Driver driver(port, FuzzConfig(in, mats[active]));
  port.Follow(driver.GetConfig());
  for (int calls = 0; (calls < 48) && !in.Empty(); ++calls) {
    const bool was_up = driver.IsInitialized();
    switch (in.Byte() % 7U) {
    case 0:
      DoStart(driver, port, was_up);
      break;
    case 1:
      DoRead(driver, port, was_up);
      break;
    case 2:
      DoReadResistance(driver, port, was_up);
      break;
    case 3:
      DoSetConfig(driver, port, in, mats, active);
      break;
    case 4:
      DoConvertProbe(driver, in);
      break;
    case 5:
      DoReadRaw(driver, port, was_up);
      break;
    default:
      port.SetResistance(1U + (in.Triple() % 3'000'000U));
      break;
    }
  }
  return 0;
}
