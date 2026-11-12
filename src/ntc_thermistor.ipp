/**
 * @file ntc_thermistor.ipp
 * @brief NTC thermistor driver implementation, included by
 * `ntc_thermistor.hpp`.
 * @copyright Copyright (c) 2026 The driver-ntc-thermistor authors.
 * SPDX-License-Identifier: MIT
 */
#ifndef NTC_THERMISTOR_IPP
#define NTC_THERMISTOR_IPP

namespace ntc_thermistor {

template <AdcPort Port>
Error NtcThermistor<Port>::check(const Config& config) noexcept {
  if (Validate(config) != Error::Ok) {
    return Error::InvalidArgument;
  }
  // A wait the port cannot make, or a settling time without a bias switch, is a
  // configuration error found before any I/O.
  bool needs_delay = (config.sample_count > 1U) && (config.sample_delay_us > 0U);
  if constexpr (detail::HasSetBias<Port>) {
    needs_delay = needs_delay || (config.settle_us > 0U);
  } else {
    if (config.settle_us > 0U) {
      return Error::InvalidArgument;
    }
  }
  if constexpr (!detail::HasDelayUs<Port>) {
    if (needs_delay) {
      return Error::InvalidArgument;
    }
  }
  return Error::Ok;
}

template <AdcPort Port>
Result<void> NtcThermistor<Port>::EnsureInitialized() noexcept {
  if (initialized_) {
    return Result<void>::Success();
  }
  if (check(config_) != Error::Ok) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  if (!port_.EnsureInitialized()) {
    return Result<void>::Failure(Error::Bus);
  }
  if constexpr (detail::HasSetBias<Port>) {
    if (!port_.SetBias(false)) {
      return Result<void>::Failure(Error::Bus);
    }
  }
  initialized_ = true;
  return Result<void>::Success();
}

template <AdcPort Port>
Result<void> NtcThermistor<Port>::SetConfig(const Config& config) noexcept {
  if (check(config) != Error::Ok) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  config_ = config;
  return Result<void>::Success();
}

template <AdcPort Port>
Result<uint32_t> NtcThermistor<Port>::sampleMean(const uint32_t count) noexcept {
  if (count == 0U) {
    return Result<uint32_t>::Failure(Error::InvalidArgument); // callers pass 1 or more
  }
  const uint32_t scale = detail::FullScale(config_.adc_bits);
  uint32_t sum = 0U;
  uint32_t lowest = scale;
  uint32_t highest = 0U;
  for (uint32_t i = 0U; i < count; ++i) {
    uint32_t code = 0U;
    if (!port_.ReadRaw(config_.adc_channel, code)) {
      return Result<uint32_t>::Failure(Error::Bus);
    }
    last_code_ = code;
    if (code >= scale) {
      return Result<uint32_t>::Failure(Error::CodeOutOfScale);
    }
    sum += code;
    lowest = (code < lowest) ? code : lowest;
    highest = (code > highest) ? code : highest;
    if constexpr (detail::HasDelayUs<Port>) {
      if (((i + 1U) < count) && (config_.sample_delay_us > 0U)) {
        port_.DelayUs(config_.sample_delay_us);
      }
    }
  }
  if ((config_.max_spread_codes > 0U) && ((highest - lowest) > config_.max_spread_codes)) {
    return Result<uint32_t>::Failure(Error::Unstable);
  }
  return Result<uint32_t>::Success((sum + (count / 2U)) / count);
}

template <AdcPort Port>
Result<uint32_t> NtcThermistor<Port>::acquireBiased(const uint32_t count) noexcept {
  if (!port_.SetBias(true)) {
    static_cast<void>(
        port_.SetBias(false)); // never leave the divider powered after a failed switch
    return Result<uint32_t>::Failure(Error::Bus);
  }
  if constexpr (detail::HasDelayUs<Port>) {
    if (config_.settle_us > 0U) {
      port_.DelayUs(config_.settle_us);
    }
  }
  const Result<uint32_t> mean = sampleMean(count);
  const bool off = port_.SetBias(false); // off after a good read and after every failure
  if (!mean) {
    return mean;
  }
  return off ? mean : Result<uint32_t>::Failure(Error::Bus);
}

template <AdcPort Port>
Result<uint32_t> NtcThermistor<Port>::acquire(const uint32_t count) noexcept {
  if (!initialized_) {
    return Result<uint32_t>::Failure(Error::NotInitialized);
  }
  if constexpr (detail::HasSetBias<Port>) {
    return acquireBiased(count);
  } else {
    return sampleMean(count);
  }
}

template <AdcPort Port>
Result<Reading> NtcThermistor<Port>::Read() noexcept {
  const Result<uint32_t> mean = acquire(config_.sample_count);
  if (!mean) {
    return Result<Reading>::Failure(mean.error);
  }
  return detail::ConvertCode(config_, mean.value);
}

template <AdcPort Port>
Result<uint32_t> NtcThermistor<Port>::ReadRawCode() noexcept {
  using detail::operator""_u32;
  return acquire(1_u32);
}

template <AdcPort Port>
Result<uint32_t> NtcThermistor<Port>::ReadResistance() noexcept {
  const Result<uint32_t> mean = acquire(config_.sample_count);
  if (!mean) {
    return mean;
  }
  return detail::ResistanceOfCode(config_, mean.value);
}

} // namespace ntc_thermistor

#endif // NTC_THERMISTOR_IPP
