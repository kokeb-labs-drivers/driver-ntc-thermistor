# API guide

The API by task, with code that compiles (the host build compiles every example on this page).

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Public references n/a n/a:

    - Self-heating power: Dissipated power, P = V_s^2 R_n / (R_s + R_n)^2, p. 1

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## The object

`NtcThermistor<Port>` holds a reference to your port and a `Config`. The constructor does no I/O and cannot fail. The
object is not copyable or movable.

| Call | Returns | What it does |
| --- | --- | --- |
| `EnsureInitialized()` | `Result<void>` | Checks the configuration, brings the port's ADC up and switches the divider off. Returns `Ok` with no I/O once it has succeeded |
| `IsInitialized()` | `bool` | Whether `EnsureInitialized()` has succeeded |
| `Read()` | `Result<Reading>` | Samples, converts, returns the temperature, resistance, mean code and bound |
| `ReadResistance()` | `Result<uint32_t>` | The same sampling, stopping at the divider arithmetic: ohms |
| `ReadRawCode()` | `Result<uint32_t>` | One raw conversion in the same bias sequence, with no averaging and no maths: a diagnostic, no retry |
| `GetLastCode()` | `uint32_t` | The last code the port returned, even one out of scale; no I/O |
| `SetConfig(config)` | `Result<void>` | Replaces the configuration after validating it; the old one stays on failure |
| `GetConfig()` | `const Config&` | The configuration in use |

Free functions, in `inc/ntc_thermistor_conversion.hpp`, with no port and no state:

| Function | What it does |
| --- | --- |
| `Convert(config, code)` | The conversion of a code: the same maths as `Read()` |
| `ConvertResistance(config, code)` | The divider arithmetic only |
| `ToMilliKelvin(mc)`, `ToMilliFahrenheit(mc)` | Unit conversions, saturating |
| `SelfHeatingMicrowatts(config, r_ohm)` | The power the divider puts into the part |
| `Validate(config)` | The configuration check |

## Read a temperature

```cpp title="quickstart.cpp"
--8<-- "tests/docs/quickstart.cpp:quickstart"
```

`Reading` has `temperature_mc` (milli-degrees Celsius, window and offset applied), `resistance_ohm`, `code` (the mean)
and `uncertainty_mc` (a bound on the declared tolerances).

## Use the maths alone

```cpp title="api_tasks.cpp"
--8<-- "tests/docs/api_tasks.cpp:convert"
```

`Convert` validates the configuration first, so the same call checks a `Config` and a code in a unit test of your own
application.

## Handle every error

```cpp title="api_tasks.cpp"
--8<-- "tests/docs/api_tasks.cpp:errors"
```

| Error | Value | When |
| --- | --- | --- |
| `NotInitialized` | 1 | A read before `EnsureInitialized()` succeeded |
| `InvalidArgument` | 2 | The configuration is out of range or inconsistent (including a wait the port cannot make) |
| `Bus` | 3 | `ReadRaw()` or `SetBias()` returned `false` |
| `OpenCircuit` | 16 | The node is at the open rail: cut wire, missing part, divider not powered |
| `ShortCircuit` | 17 | The node is at the short rail: shorted part or wire |
| `BelowRange` | 18 | Colder than the curve or the window, or a resistance that does not fit 32 bits |
| `AboveRange` | 19 | Hotter than the curve or the window, or a resistance of zero |
| `Unstable` | 20 | The samples differ by more than `max_spread_codes` |
| `CodeOutOfScale` | 21 | A sample is at or above 2^`adc_bits` |

`Timeout`, `DeviceIdMismatch`, `Crc`, `Busy`, `DeviceFault` and `Unsupported` are never returned: a thermistor has
no bus, no identity and no checksum.

## Trust a reading only as far as its bound

```cpp title="api_tasks.cpp"
--8<-- "tests/docs/api_tasks.cpp:uncertainty"
```

## Change the configuration at run time

```cpp title="api_tasks.cpp"
--8<-- "tests/docs/api_tasks.cpp:replace-config"
```

## Other units

```cpp title="reference.cpp"
--8<-- "tests/docs/reference.cpp:units"
```

Next: [Configuration](configuration.md).
