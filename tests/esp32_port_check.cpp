/**
 * @file esp32_port_check.cpp
 * @brief The ESP32 example port compiled on the host against stub ESP-IDF headers: it starts
 * without a bias pin, a failed start-up can be retried and configures the pin then, the divider is
 * powered only while it is read, and a width the chip does not offer is refused.
 *
 * It guards the logic of the example (the bug it prevents: a retry that reports success without
 * having configured the pin). It does not test ESP-IDF.
 */
#include "esp32_ntc_thermistor_port.hpp"
#include "ntc_thermistor.hpp"
#include "ntc_thermistor_presets.hpp"
#include "test_harness.hpp"

using ntc_thermistor::Error;

namespace {

ntc_thermistor::Config ExampleConfig() {
  ntc_thermistor::Config c;
  c.curve = ntc_thermistor::presets::kGeneric10kB3950;
  c.min_temperature_mc = -40'000;
  c.max_temperature_mc = 125'000;
  return c;
}

void ResetStubs() {
  g_stub_gpio_level = 0U;
  g_stub_gpio_configured = 0;
  g_stub_gpio_config_fails = false;
  g_stub_delay_us = 0U;
  g_stub_units_created = 0;
}

TEST_CASE("The ESP32 port with the default pins starts, reads, and settle_us only adds a wait") {
  ResetStubs();
  Esp32NtcThermistorPort port;
  ntc_thermistor::Config c = ExampleConfig();
  c.settle_us = 300;
  ntc_thermistor::NtcThermistor<Esp32NtcThermistorPort> sensor(port, c);
  REQUIRE(sensor.EnsureInitialized().ok());
  const auto r = sensor.Read();
  REQUIRE(r.ok());
  CHECK_EQ(r.value.code, static_cast<uint32_t>(kStubRawCount));
  CHECK_EQ(g_stub_delay_us, 300U);
  CHECK_EQ(g_stub_gpio_configured, 0);
}

TEST_CASE("With a bias pin the divider is configured off, powered while read, and off again") {
  ResetStubs();
  Esp32NtcThermistorPins pins;
  pins.bias_gpio = 4;
  Esp32NtcThermistorPort port(pins);
  ntc_thermistor::NtcThermistor<Esp32NtcThermistorPort> sensor(port, ExampleConfig());
  REQUIRE(sensor.EnsureInitialized().ok());
  CHECK_EQ(g_stub_gpio_configured, 1);
  CHECK_EQ(g_stub_gpio_level, 0U);
  REQUIRE(sensor.Read().ok());
  CHECK_EQ(g_stub_gpio_level, 0U); // off after the read
}

TEST_CASE("A failed start-up is retried: the pin is configured on the second call, not skipped") {
  ResetStubs();
  Esp32NtcThermistorPins pins;
  pins.bias_gpio = 4;
  Esp32NtcThermistorPort port(pins);
  ntc_thermistor::NtcThermistor<Esp32NtcThermistorPort> sensor(port, ExampleConfig());
  g_stub_gpio_config_fails = true;
  CHECK_EQ(sensor.EnsureInitialized().error, Error::Bus);
  CHECK_EQ(g_stub_gpio_configured, 0);
  g_stub_gpio_config_fails = false;
  REQUIRE(sensor.EnsureInitialized().ok());
  CHECK_EQ(g_stub_gpio_configured, 1); // configured now
  CHECK_EQ(g_stub_units_created, 1);   // the ADC unit of the first attempt is reused
  REQUIRE(sensor.Read().ok());
}

TEST_CASE(
    "A width outside 9 to 13 bits is refused at start-up; one the chip lacks fails the read") {
  ResetStubs();
  Esp32NtcThermistorPins pins;
  pins.bitwidth = ADC_BITWIDTH_DEFAULT;
  Esp32NtcThermistorPort refused(pins);
  ntc_thermistor::NtcThermistor<Esp32NtcThermistorPort> a(refused, ExampleConfig());
  CHECK_EQ(a.EnsureInitialized().error, Error::Bus);
  CHECK_EQ(g_stub_units_created, 0);
  pins.bitwidth = ADC_BITWIDTH_10; // named by ESP-IDF, but the stub chip converts at 12 only
  Esp32NtcThermistorPort chip_lacks(pins);
  ntc_thermistor::NtcThermistor<Esp32NtcThermistorPort> b(chip_lacks, ExampleConfig());
  REQUIRE(b.EnsureInitialized().ok());
  CHECK_EQ(b.Read().error, Error::Bus);
}

} // namespace
