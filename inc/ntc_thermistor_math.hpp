/**
 * @file ntc_thermistor_math.hpp
 * @brief Integer arithmetic of the NTC thermistor driver: a binary logarithm,
 * rounded division and saturating multiplication, with no libm and no floating
 * point.
 * @copyright Copyright (c) 2026 The driver-ntc-thermistor authors.
 * SPDX-License-Identifier: MIT
 *
 * @details
 * Everything is `constexpr`. No function divides by zero or shifts by a count
 * it has not bounded. `SatMul`, `SatAdd` and `DivRoundU` never overflow;
 * `DivRound`, `AbsDiff` and `Ln16` have the preconditions stated on each
 * (handbook KB-CPP20-012, -020, -058).
 */
#ifndef NTC_THERMISTOR_MATH_HPP
#define NTC_THERMISTOR_MATH_HPP

#include <bit>
#include <cstdint>
#include <limits>

namespace ntc_thermistor::detail {

/// Fractional bits of the logarithms the driver compares (Q16: one unit is
/// 1/65536 of a neper).
inline constexpr uint32_t kLnFractionBits = 16U;
/// Square-and-compare steps of `Ln16()`: one more bit than the result keeps,
/// used to round.
inline constexpr uint32_t kLnSteps = kLnFractionBits + 1U;
/// ln 2 as a Q32 fraction: 0.6931471805599453 * 2^32, rounded.
inline constexpr uint64_t kLn2Q32 = uint64_t{2977044472U};
/// 2^31 and 2^32 as 64-bit values (the mantissa window of `Ln16()` and the
/// rounding term).
inline constexpr uint64_t kTwoPow31 = uint64_t{1U} << 31U;
/// 2^32.
inline constexpr uint64_t kTwoPow32 = uint64_t{1U} << 32U;
/// Resistances are carried in 1/256 ohm ("fine ohms") so that rounding to a
/// whole ohm does not cost a hot, low-resistance thermistor more than the ADC
/// does.
inline constexpr uint64_t kFineScale = uint64_t{256U};
/// The largest `uint64_t`.
inline constexpr uint64_t kMaxU64 = ~uint64_t{0U};

/**
 * @brief The natural logarithm of `x` in Q16, rounded to nearest: an integer
 * binary logarithm
 * (`std::bit_width`) plus 17 square-and-compare steps for the fraction, times
 * ln 2 in Q32.
 *
 * The error is at most one unit (2^-16 of a neper, measured 0.77 unit over 1 to
 * 2^64 - 1); for a thermistor that is about 0.001 K at the hot end of the
 * range. Monotone: a larger `x` never gives a smaller result.
 *
 * @param x A positive number; 0 returns 0 (callers never pass it).
 * @return ln(x) * 65536; at most 44.4 * 65536.
 */
[[nodiscard]] constexpr uint32_t Ln16(uint64_t x) noexcept {
  if (x == 0U) {
    return 0U;
  }
  const uint32_t whole = static_cast<uint32_t>(std::bit_width(x)) - 1U; // floor(log2 x), 0..63
  // The mantissa x / 2^whole in [1, 2), as a Q31 number: [2^31, 2^32).
  uint64_t m = (whole >= 31U) ? (x >> ((whole - 31U) & 63U)) : (x << ((31U - whole) & 63U));
  uint32_t fraction = 0U;
  for (uint32_t step = 0U; step < kLnSteps; ++step) {
    m = (m * m) >> 31U; // the square, back in Q31: [2^31, 2^33)
    fraction <<= 1U;
    if (m >= kTwoPow32) {
      fraction |= 1U;
      m >>= 1U;
    }
  }
  const uint32_t log2_q17 = (whole << kLnSteps) | fraction;
  const uint64_t log2_q16 = (static_cast<uint64_t>(log2_q17) + 1U) >> 1U;
  return static_cast<uint32_t>(((log2_q16 * kLn2Q32) + kTwoPow31) >> 32U);
}

/**
 * @brief `a / b` rounded to nearest, halves away from zero.
 * @param a The dividend; `|a| + b / 2` must fit `int64_t` (the callers keep it
 * below 2^62).
 * @param b The divisor; at least 1 (a smaller one returns 0, which no caller
 * reaches).
 * @return The rounded quotient.
 */
[[nodiscard]] constexpr int64_t DivRound(int64_t a, int64_t b) noexcept {
  if (b <= 0) {
    return 0;
  }
  const int64_t half = b / 2;
  return (a >= 0) ? ((a + half) / b) : -(((-a) + half) / b);
}

/**
 * @brief `a / b` rounded to nearest (halves up).
 * @param a The dividend.
 * @param b The divisor; at least 1 (0 returns 0, which no caller reaches).
 * @return The rounded quotient; the sum `a + b / 2` is never formed, so it
 * cannot wrap.
 */
[[nodiscard]] constexpr uint64_t DivRoundU(uint64_t a, uint64_t b) noexcept {
  if (b == 0U) {
    return 0U;
  }
  const uint64_t quotient = a / b;
  return ((a % b) >= (b - (b / 2U))) ? (quotient + 1U) : quotient;
}

/**
 * @brief `a * b`, or the largest `uint64_t` when the product does not fit.
 * @param a First factor.
 * @param b Second factor.
 * @return The saturated product.
 */
[[nodiscard]] constexpr uint64_t SatMul(uint64_t a, uint64_t b) noexcept {
  if ((a != 0U) && (b > (kMaxU64 / a))) {
    return kMaxU64;
  }
  return a * b;
}

/**
 * @brief `a + b`, or the largest `uint64_t` when the sum does not fit.
 * @param a First term.
 * @param b Second term.
 * @return The saturated sum.
 */
[[nodiscard]] constexpr uint64_t SatAdd(uint64_t a, uint64_t b) noexcept {
  return (b > (kMaxU64 - a)) ? kMaxU64 : (a + b);
}

/**
 * @brief The absolute difference of two signed 64-bit values, as an unsigned
 * number.
 * @param a First value.
 * @param b Second value; `|a - b|` must fit 64 bits (the callers pass 32-bit
 * temperatures).
 * @return `|a - b|`.
 */
[[nodiscard]] constexpr uint64_t AbsDiff(int64_t a, int64_t b) noexcept {
  return static_cast<uint64_t>((a >= b) ? (a - b) : (b - a));
}

/**
 * @brief A signed 64-bit value clamped to the `int32_t` range.
 * @param v The value.
 * @return `v`, or the nearest `int32_t` limit.
 */
[[nodiscard]] constexpr int32_t ClampToInt32(int64_t v) noexcept {
  constexpr int64_t kLowest = std::numeric_limits<int32_t>::min();
  constexpr int64_t kHighest = std::numeric_limits<int32_t>::max();
  int64_t out = (v < kLowest) ? kLowest : v;
  out = (out > kHighest) ? kHighest : out;
  return static_cast<int32_t>(out);
}

} // namespace ntc_thermistor::detail

#endif // NTC_THERMISTOR_MATH_HPP
