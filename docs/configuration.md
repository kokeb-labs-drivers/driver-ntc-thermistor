# Configuration

`ntc_thermistor::Config` describes the thermistor, the divider, the sampling and the window. It has **no default
part**: a `Config{}` is `InvalidArgument` until a `Curve` is declared, so the driver never assumes one.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Public references n/a n/a:

    - Beta equation and the B25/85 pair: Beta model, 1/T = 1/T0 + ln(R/R0) / B, p. 1
    - Voltage divider and the series resistor: Divider, V = V_s R_n / (R_s + R_n), p. 1

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

Check a configuration without touching hardware with `ntc_thermistor::Validate(config)`, in a `static_assert` if it
is `constexpr`. The driver applies the same rules in `EnsureInitialized()` and `SetConfig()`.

## The curve

| Field | Default | Meaning |
| --- | --- | --- |
| `curve.table` | empty | A table: 2 to 256 `TablePoint{resistance_ohm, temperature_mc}` rows. Non-empty selects table mode |
| `curve.r0_ohm` | 0 | Beta mode: R at `ref_temperature_mc`, 1 to 10 000 000 |
| `curve.ref_temperature_mc` | 25 000 | T0 of `r0_ohm`, in milli-degrees Celsius |
| `curve.b_kelvin` | 0 | Beta mode: B, 1000 to 8000 |
| `curve.b_t2_mc` | 85 000 | The second temperature of the B pair; documentation, not used in the maths |
| `curve.r_tol_ppm` | 0 | Tolerance of R at the reference and of the rows; 0 = not declared |
| `curve.b_tol_ppm` | 0 | Tolerance of B, in beta and table mode; 0 = not declared; must be 0 with Steinhart-Hart |
| `curve.sh_a_e15`, `sh_b_e15`, `sh_c_e15` | 0 | Steinhart-Hart A, B, C in units of 1e-15 per kelvin; a non-zero `sh_b_e15` selects the mode |
| `curve.curve_error_mc` | 0 | The curve's own error (a fit, an interpolation), added to the bound; 0 to 100 000 |

A curve is exactly one of a valid table, valid Steinhart-Hart coefficients or a valid beta description; an empty curve
is refused. See [Presets](presets.md) for the two shipped, [Conversion](conversion.md#making-a-table) for the
table generator, and how to write your own.

```cpp title="configuration.cpp"
--8<-- "tests/docs/configuration.cpp:own-part"
```

## The divider

| Field | Default | Meaning |
| --- | --- | --- |
| `adc_channel` | 0 | Passed to the port's `ReadRaw()` |
| `adc_bits` | 12 | The ADC's resolution, 8 to 24; a sample at or above 2^bits is `CodeOutOfScale` |
| `topology` | `NtcLowSide` | `NtcLowSide` (thermistor to ground) or `NtcHighSide` (thermistor to the supply) |
| `series_ohm` | 10 000 | The other resistor, 1 to 16 777 216 |
| `series_tol_ppm` | 0 | Its tolerance |
| `adc_vref_mv`, `divider_supply_mv` | 0, 0 | 0 and 0 is ratiometric; otherwise both are 1 to 20 000 mV |
| `open_margin_codes`, `short_margin_codes` | 2, 2 | Codes at each rail that count as a fault; at least 1 when `adc_vref_mv` is set |

```cpp title="configuration.cpp"
--8<-- "tests/docs/configuration.cpp:high-side"
```

```cpp title="configuration.cpp"
--8<-- "tests/docs/configuration.cpp:non-ratiometric"
```

See [Reference, range and supplies](reference-and-supplies.md) for when the divider is not ratiometric.

## Sampling

| Field | Default | Meaning |
| --- | --- | --- |
| `sample_count` | 1 | Conversions averaged per reading, 1 to 16 |
| `sample_delay_us` | 0 | Wait between samples, at most 1 000 000; the port needs `DelayUs()` |
| `settle_us` | 0 | Wait after `SetBias(true)`, at most 1 000 000; the port needs `SetBias()` and `DelayUs()` |
| `max_spread_codes` | 0 | Largest sample minus smallest; more is `Unstable`; 0 = not checked |

A wait the port cannot make is `InvalidArgument` from `Validate`-time checks, before any I/O, rather than a silently
skipped wait.

```cpp title="configuration.cpp"
--8<-- "tests/docs/configuration.cpp:averaging"
```

## Window and offset

| Field | Default | Meaning |
| --- | --- | --- |
| `min_temperature_mc`, `max_temperature_mc` | none | The valid window. **Mandatory in beta mode**; in table mode optional, narrowing the table |
| `offset_mc` | 0 | Added to the result, saturating; magnitude at most 20 000 |

## Menuconfig

With ESP-IDF, `ntc_thermistor::KconfigDefaults()` returns a `Config` from `idf.py menuconfig`: the channel, the bits,
the series resistor, the divider side, the averaging, the curve (a preset, or your R25 and B) and the window. Outside
ESP-IDF it returns the `Config{}` defaults, which are invalid until you declare a curve.

```cpp title="configuration.cpp"
--8<-- "tests/docs/configuration.cpp:kconfig"
```

Next: [Reference, range and supplies](reference-and-supplies.md).
