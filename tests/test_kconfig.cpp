/**
 * @file test_kconfig.cpp
 * @brief KconfigDefaults(): Config{} without Kconfig, the menuconfig values
 * with it (KCONFIG_SET).
 */
#include "ntc_thermistor_kconfig.hpp"
#include "test_harness.hpp"

#ifdef KCONFIG_SET
TEST_CASE("Kconfig values reach the Config") {
  constexpr ntc_thermistor::Config kCfg = ntc_thermistor::KconfigDefaults();
  CHECK_EQ(kCfg.adc_channel, 2);
  CHECK_EQ(kCfg.adc_bits, 10);
  CHECK_EQ(kCfg.series_ohm, 4700U);
  CHECK_EQ(kCfg.sample_count, 8);
  CHECK(kCfg.topology == ntc_thermistor::Topology::NtcHighSide);
  CHECK_EQ(kCfg.min_temperature_mc, -30'000);
  CHECK_EQ(kCfg.max_temperature_mc, 100'000);
  CHECK_EQ(kCfg.curve.r0_ohm, 4700U);
  CHECK_EQ(kCfg.curve.b_kelvin, 3977U);
  CHECK(kCfg.curve.table.empty());
  static_assert(ntc_thermistor::Validate(kCfg) == ntc_thermistor::Error::Ok);
}
#elif defined(KCONFIG_PRESET)
TEST_CASE("The menuconfig curve choice selects a preset") {
  constexpr ntc_thermistor::Config kCfg = ntc_thermistor::KconfigDefaults();
  CHECK_EQ(kCfg.curve.table.size(), 39U);
  CHECK_EQ(kCfg.adc_bits, 12);
  CHECK(ntc_thermistor::Validate(kCfg) == ntc_thermistor::Error::Ok);
}
#else
TEST_CASE("Without Kconfig, KconfigDefaults() equals Config{}: no part, so it "
          "is refused") {
  constexpr ntc_thermistor::Config kCfg = ntc_thermistor::KconfigDefaults();
  constexpr ntc_thermistor::Config kDefault{};
  CHECK_EQ(kCfg.adc_channel, kDefault.adc_channel);
  CHECK_EQ(kCfg.adc_bits, kDefault.adc_bits);
  CHECK_EQ(kCfg.series_ohm, kDefault.series_ohm);
  CHECK_EQ(kCfg.sample_count, kDefault.sample_count);
  CHECK(kCfg.topology == kDefault.topology);
  CHECK_EQ(kCfg.min_temperature_mc, kDefault.min_temperature_mc);
  CHECK_EQ(kCfg.max_temperature_mc, kDefault.max_temperature_mc);
  CHECK(kCfg.curve.table.empty());
  CHECK(ntc_thermistor::Validate(kCfg) == ntc_thermistor::Error::InvalidArgument);
}
#endif
