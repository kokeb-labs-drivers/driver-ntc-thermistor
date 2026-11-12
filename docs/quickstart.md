# Quickstart

Read a temperature from a thermistor on your MCU: wire a divider, write a small port class, call two driver functions.

## 1. Wire it

| MCU | Divider | Notes |
| --- | --- | --- |
| ADC input | Node between the series resistor and the thermistor | The thermistor goes to ground (low side) |
| 3.3 V | Series resistor, 10 k as a start | The same rail as the ADC reference, so the reading is ratiometric |
| GND | Thermistor | Same ground as the ADC |

The [Hardware](hardware.md) page says how to choose the series resistor and what to do about the ADC input.

## 2. Add the driver

```bash
git submodule add https://github.com/kokeb-labs-drivers/driver-ntc-thermistor.git external/driver-ntc-thermistor
```

## 3. Write the port

Two functions, one optional: bring the ADC up, and read one raw count of one channel.

```cpp title="my_port.hpp"
--8<-- "tests/docs/my_port.hpp:port"
```

## 4. Read the temperature

```cpp title="main.cpp"
--8<-- "tests/docs/quickstart.cpp:quickstart"
```

1. `Config` has **no default part**: you name one. The preset is a 10 kOhm, B 3950 thermistor, the common class
   sold as probes and beads (typical values, not a datasheet claim). Read the datasheet of yours and replace it: see
   [Presets](presets.md).
2. A beta curve needs the range the part is rated for (`min_temperature_mc`, `max_temperature_mc`): the equation
   extrapolates, so the driver will not guess one. A table carries its own range.
3. `EnsureInitialized()` checks the configuration (no I/O), brings the ADC up and switches the divider off. It is the
   only call that starts anything.
4. `Read()` takes `sample_count` conversions, averages them, finds the resistance and the temperature, and
   returns a `Reading`: `temperature_mc`, `resistance_ohm`, `code` and `uncertainty_mc`.

An error means something specific: a cut wire is `OpenCircuit`, a short is `ShortCircuit`. The table is in
[Troubleshooting](troubleshooting.md).

Next: [API guide](api-guide.md).
