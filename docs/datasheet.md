# Datasheet map

The driver is generic: it follows the beta and Steinhart-Hart equations and the divider, not one datasheet. This page
says which documents stand behind it and where each part of the driver is described in them, so you can check any
behaviour against the source.

The one vendor datasheet is the worked example's: TDK, one page, Rev.3 of 8 May 2023, no document number printed. It
confirms R25, B, the tolerances, the ratings and the range; it prints **no resistance table** (a separate step file,
403 to an automated fetch), so the 39 rows of the preset stay **unverified**. Nothing generic depends on it.
[Worked example](worked-example.md) says what is left to confirm.

<!-- generated: datasheet-map -->
- **Reference: Beta equation, Steinhart-Hart equation and the voltage divider**, Public references n/a, revision n/a
  (n/a). Get it from the [vendor page](https://en.wikipedia.org/wiki/Steinhart%E2%80%93Hart_equation).
  SHA-256 of the file used: `n/a (a web page, not a file)`.
- **Datasheet: Chip NTC Thermistor, NTCG series, NTCG163JF103FT1S (datasheet)**, TDK none printed, revision Rev.3 (May
  8, 2023). Get it from the [vendor
  page](https://product.tdk.com/system/files/dam/doc/product/sensor/ntc/chip-ntc-thermistor/data_sheet/datasheet_ntcg163jf103ft1s.pdf)
  or as a
  [PDF](https://product.tdk.com/system/files/dam/doc/product/sensor/ntc/chip-ntc-thermistor/data_sheet/datasheet_ntcg163jf103ft1s.pdf).
  SHA-256 of the file used: `64d2925d33b75da76b13e498fa5e889def95b02ba72059528f05820585284fd4`.

| Topic | Datasheet section | Page | In the driver | Explained on |
| --- | --- | --- | --- | --- |
| Beta equation and the B25/85 pair | n/a: Beta model, 1/T = 1/T0 + ln(R/R0) / B | p. 1 | `Curve::r0_ohm`, `b_kelvin`, `b_t2_mc`, `presets::kGeneric10kB3950` | [Conversion](conversion.md), [Presets](presets.md), [Configuration](configuration.md) |
| Steinhart-Hart and the table tool | n/a: Steinhart-Hart, 1/T = a + b ln R + c ln(R)^3 | p. 1 | `Curve::sh_a_e15`, `sh_b_e15`, `sh_c_e15`, `Curve::table`, `tools/ntc_table.py` | [Conversion](conversion.md), [Presets](presets.md) |
| Voltage divider and the series resistor | n/a: Divider, V = V_s R_n / (R_s + R_n) | p. 1 | `Config::topology`, `series_ohm`, `adc_vref_mv`, `divider_supply_mv`, `Convert()` | [Hardware](hardware.md), [Conversion](conversion.md), [Configuration](configuration.md) |
| Self-heating power | n/a: Dissipated power, P = V_s^2 R_n / (R_s + R_n)^2 | p. 1 | `SelfHeatingMicrowatts()` | [Hardware](hardware.md), [API guide](api-guide.md) |
| Example part: R-T table (rows unverified) | none printed: Electrical Characteristics, 1degC Step Data link (the table itself is not in the PDF) | [p. 1](https://product.tdk.com/system/files/dam/doc/product/sensor/ntc/chip-ntc-thermistor/data_sheet/datasheet_ntcg163jf103ft1s.pdf#page=1) | `presets::kTdkNtcg163jf103ft1sRows`, `kTdkNtcg163jf103ft1s` | [Presets](presets.md), [Worked example: a 10 kOhm chip thermistor](worked-example.md) |
| Example part: R25, B and tolerances | none printed: Electrical Characteristics: Resistance, B value | [p. 1](https://product.tdk.com/system/files/dam/doc/product/sensor/ntc/chip-ntc-thermistor/data_sheet/datasheet_ntcg163jf103ft1s.pdf#page=1) | `Curve::r0_ohm`, `b_kelvin`, `b_t2_mc`, `r_tol_ppm`, `b_tol_ppm` | [Presets](presets.md), [Worked example: a 10 kOhm chip thermistor](worked-example.md) |
| Example part: package and ratings | none printed: Dimension and Construction; Electrical Characteristics: power, dissipation, current, voltage, range | [p. 1](https://product.tdk.com/system/files/dam/doc/product/sensor/ntc/chip-ntc-thermistor/data_sheet/datasheet_ntcg163jf103ft1s.pdf#page=1) | `SelfHeatingMicrowatts()` (the dissipation constant is yours to supply) | [Worked example: a 10 kOhm chip thermistor](worked-example.md), [Hardware](hardware.md) |
<!-- /generated -->

## When the datasheet and the driver differ

If your part's datasheet gives a table or R25 and B that differ from a preset, the datasheet wins: declare your own
`Curve` ([Presets](presets.md)). A difference between the equations and the driver's arithmetic is a bug report for
this repository.

The repository never contains a PDF: vendors own their documents. When you supply one, compute its SHA-256
(`shasum -a 256`) and put it in `docs/datasheets.yml`.
