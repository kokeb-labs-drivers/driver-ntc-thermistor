# Examples

One folder per platform under `examples/`, each with its own port, built by CI on every pull request. The driver
itself has no platform code; [Platforms](platforms.md) lists what exists.

## Host

[`examples/host/`](../examples/host/README.md) runs the driver on any PC against a simulated divider, through the
host port [`host_ntc_thermistor_port.hpp`](../examples/host/host_ntc_thermistor_port.hpp). No hardware:

```bash
cmake -S examples/host -B build-example && cmake --build build-example && ./build-example/ntc_thermistor_host_example
```

It reads four parts with no change of code (the example preset as a table, the generic 10 kOhm / B 3950 preset, a 100
kOhm / B 4250 part declared in the example itself, and a high-side divider), then provokes each fault. The host tests
build it with warnings as errors and run it (CTest `ntc_thermistor_host_example`); it exits non-zero if a reading leaves
its bound.

## ESP32

An ESP-IDF application that runs the driver on an ESP32-C6; the wiring is in the
[example folder's README](../examples/esp32/README.md) and the port in
[`esp32_ntc_thermistor_port.hpp`](../examples/esp32/main/esp32_ntc_thermistor_port.hpp). The project points
`EXTRA_COMPONENT_DIRS` at the repository root, which is the driver's ESP-IDF component.

### The apps

<!-- generated: examples -->
| App | What it shows | Source | CI |
| --- | --- | --- | --- |
| `ntc_thermistor_basic_example` :material-star:{ title="featured" } | Read an NTC divider from menuconfig values and log the temperature once a second | [`ntc_thermistor_basic_example.cpp`](../examples/esp32/main/ntc_thermistor_basic_example.cpp) | yes |

Build and flash one (from `examples/esp32`, ESP-IDF environment loaded):

```bash
./scripts/build_app.sh ntc_thermistor_basic_example Release
./scripts/flash_app.sh flash_monitor ntc_thermistor_basic_example Release
```

Source of this table: [`examples/esp32/app_config.yml`](../examples/esp32/app_config.yml).
<!-- /generated -->

### Set up once

```bash
git submodule update --init examples/esp32/scripts   # the ESP-IDF build scripts
cd examples/esp32
. $IDF_PATH/export.sh                                 # ESP-IDF v5.5
./scripts/build_app.sh list                           # every app and build type
```

The scripts read [`app_config.yml`](../examples/esp32/app_config.yml) (the same file this page's table comes
from) and build each app as its own firmware image.

### What to read in an example

| You want to | Read |
| --- | --- |
| See the lifecycle and every `Result` checked | `ntc_thermistor_basic_example.cpp` |
| See a part declared as data | `examples/host/main.cpp` and [Presets](presets.md) |
| Write an ESP-IDF port | `esp32_ntc_thermistor_port.hpp` |
| Use menuconfig values | `ntc_thermistor_kconfig.hpp` and [`_config/Kconfig`](../_config/Kconfig) |
