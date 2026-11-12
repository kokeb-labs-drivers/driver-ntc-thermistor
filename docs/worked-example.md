# Worked example: a 10 kOhm chip thermistor

One part end to end, with its numbers: the TDK NTCG163JF103FT1S, a 10 kOhm 0603 chip thermistor, read by a 12-bit
ADC. This is the only page that depends on that part's datasheet, and the datasheet was **not available**.

!!! warning "Unverified until the datasheet is supplied"
    Every number below that comes from the part (the rows, B, the tolerances, the package, the ratings) is a secondary
    listing and has **not** been checked against the datasheet. The arithmetic (the divider, the interpolation, the
    step sizes) is computed by the driver and holds for any part with these rows.
    When the datasheet arrives, `docs/datasheets.yml` says how to confirm each value.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From TDK unverified unverified:

    - Example part: R-T table (unverified): R-T table, -40 to 150 degC (not read), p. 1
    - Example part: B, tolerances (unverified): R25, B, tolerances (not read), p. 1
    - Example part: package, ratings (unverified): Dimensions, ratings (not read), p. 1

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## The part, as listed

| What | Value | Status |
| --- | --- | --- |
| R25 | 10 kOhm, 1 % | Unverified |
| B25/85 | 3435 K, 1 % | Unverified |
| Rows | 39, -40 to +150 degrees Celsius every 5 | Unverified |
| Package | 0603 chip | Unverified |
| Power rating and dissipation constant | Secondary listings give two values (125 and 100 mW; 1 mW/K and none) | In conflict: read the sheet |

## The configuration

```cpp
#include "ntc_thermistor_presets.hpp"

constexpr ntc_thermistor::Config kConfig{.curve = ntc_thermistor::presets::kTdkNtcg163jf103ft1s};
```

The default divider is a 10 kOhm series resistor, low side, ratiometric, on a 12-bit ADC. A table carries its range, so
no window is needed.

## What a reading looks like

Simulated divider, low side, 12 bits (derived from the unverified rows; the host example prints these):

| True (degC) | Resistance (ohm) | Code | Read (degC) | Bound (K) |
| --- | --- | --- | --- | --- |
| -20 | 67 800 | 3570 | -20.02 | 0.26 |
| 0 | 27 280 | 2997 | 0.01 | 0.27 |
| 25 | 10 000 | 2048 | 25.00 | 0.30 |
| 60 | 3 019 | 950 | 59.99 | 0.37 |
| 100 | 975 | 364 | 99.99 | 0.52 |
| 145 | 346 | 137 | 144.99 | 0.88 |

The bound comes from the declared 1 % on R25 and one ADC step; it grows at the ends because one step is worth more
there ([Hardware](hardware.md#what-an-adc-step-is-worth)). The end rows are reached to within one code: a code that
rounds just past -40 or +150 degrees is `BelowRange` or `AboveRange`, as it should be.

## Beta against the table

A beta curve with B25/85 = 3435 K is exact at 25 and 85 degrees and wrong by +3.9 K at -35 and +3.0 K at +145 (the
table in [Conversion](conversion.md#beta-against-a-table-the-same-part)). That is the reason tables exist: the same
driver, the same divider, the same code, a different `Curve`.

## Confirming it with the datasheet

1. Fill in the document entry `ds` in [`docs/datasheets.yml`](datasheets.yml): number, revision, date, `url`, `pdf`
   and `sha256` (`shasum -a 256`). Never commit the PDF.
2. Compare the sheet's table with `kTdkNtcg163jf103ft1sRows` in `inc/ntc_thermistor_presets.hpp`; correct any row, and
   the B value and tolerances.
3. Resolve the conflicts above (power rating, dissipation constant) and write the right ones on this page.
4. Replace the `section` and `page` placeholders in the `ds` rows of the datasheet topics with the sheet's own, delete
   every "unverified" in this page, in [Presets](presets.md), in the header of `ntc_thermistor_presets.hpp` and in
   `docs/hardware.yml`.
5. Run the tests: the model's copy of the table is typed separately, so a changed row fails until both agree.
