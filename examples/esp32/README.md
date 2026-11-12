# NTC thermistor ESP32 example

An ESP-IDF application that runs the driver on an ESP32-C6 and logs the temperature once a second. It is built by CI
on every pull request; it has not been run on hardware.

## Wiring

| ESP32-C6 | Divider | Notes |
| --- | --- | --- |
| GPIO0 (ADC1 channel 0) | Node between the series resistor and the thermistor | 12 dB attenuation, 12-bit raw count |
| 3V3 | Supply end of the divider | Or a GPIO (`bias_gpio`) that powers the divider only while it is read |
| GND | Ground end of the divider | |

The ESP32 ADC is not ratiometric against the 3.3 V supply, so the example declares the supply
(`divider_supply_mv = 3300`) and the full scale of the 12 dB range (`adc_vref_mv = 3100`, a typical value, not a
measured one). Measure your board and change both. With the 3300 mV supply and a 3100 mV full scale, a node above
3100 mV clips: a 10 kOhm divider then reads `OpenCircuit` from about -38 degC down with the example preset.

With no `bias_gpio` the port's `SetBias()` does nothing and succeeds, so the default pins work as they are and
`settle_us` only adds a wait; set `bias_gpio` to power the divider only while it is read, and `settle_us` to the time
the node needs to settle. The port takes its resolution from `Esp32NtcThermistorPins::bitwidth`, which the example sets
to the configured `adc_bits`. The ESP32 ADC converts at a fixed width (12 bits on the ESP32-C6; 9 to 12 on the ESP32; 13
on the ESP32-S2): set `NTC_THERMISTOR_ADC_BITS` to a width the chip offers, or the port refuses to start
(`EnsureInitialized failed: Bus`). The menu allows 8 to 24 for ports with an external converter.

## Build and flash

```bash
git submodule update --init scripts      # the ESP-IDF build scripts
. $IDF_PATH/export.sh                     # ESP-IDF v5.5
./scripts/build_app.sh list
./scripts/build_app.sh ntc_thermistor_basic_example Debug
./scripts/flash_app.sh flash_monitor ntc_thermistor_basic_example Debug
```

`idf.py menuconfig` shows "NTC thermistor" under Component config: the ADC channel and resolution, the series
resistor, the divider side, the averaging, the curve (a preset or your own R25 and B) and the valid window.

## Expected output

```text
I (312) ntc_thermistor_example: NTC thermistor driver 0.1.0
I (820) ntc_thermistor_example: 23450 mC +- 310 mK, 10870 ohm
```

An `EnsureInitialized failed: InvalidArgument` line means the configuration is not valid (for a beta curve, the window
is missing). A `read failed: OpenCircuit` line means the thermistor is not connected.

## Files

| File | What it is |
| --- | --- |
| [`main/ntc_thermistor_basic_example.cpp`](main/ntc_thermistor_basic_example.cpp) | The example app |
| [`main/esp32_ntc_thermistor_port.hpp`](main/esp32_ntc_thermistor_port.hpp) | The ESP-IDF port: raw ADC count, optional bias GPIO, delay |
| [`CMakeLists.txt`](CMakeLists.txt) | The ESP-IDF project; `EXTRA_COMPONENT_DIRS` points at the repository root, which is the component |
| [`app_config.yml`](app_config.yml) | The app matrix for the scripts, CI and the docs |
