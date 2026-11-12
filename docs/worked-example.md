# Worked example: a 10 kOhm chip thermistor

One part end to end, with its numbers: the TDK NTCG163JF103FT1S, a 10 kOhm 0603 chip thermistor, read by a 12-bit
ADC. This is the only page that depends on that part's datasheet (TDK, Rev.3, 8 May 2023, one page, p. 1).

!!! warning "The table rows are unverified"
    R25, B, the tolerances, the package and the ratings below are from the datasheet. The 39 resistance rows are
    **not**: the sheet prints no table and links a separate step file (403 to an automated fetch). The arithmetic (the
    divider, the interpolation, the step sizes) is computed by the driver and holds for any part with these rows.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From TDK none printed Rev.3:

    - Example part: R-T table (rows unverified): Electrical Characteristics, 1degC Step Data link (the table itself is not in the PDF), [p. 1](https://product.tdk.com/system/files/dam/doc/product/sensor/ntc/chip-ntc-thermistor/data_sheet/datasheet_ntcg163jf103ft1s.pdf#page=1)
    - Example part: R25, B and tolerances: Electrical Characteristics: Resistance, B value, [p. 1](https://product.tdk.com/system/files/dam/doc/product/sensor/ntc/chip-ntc-thermistor/data_sheet/datasheet_ntcg163jf103ft1s.pdf#page=1)
    - Example part: package and ratings: Dimension and Construction; Electrical Characteristics: power, dissipation, current, voltage, range, [p. 1](https://product.tdk.com/system/files/dam/doc/product/sensor/ntc/chip-ntc-thermistor/data_sheet/datasheet_ntcg163jf103ft1s.pdf#page=1)

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## The part, as the datasheet gives it

| What | Value | Source |
| --- | --- | --- |
| R25 | 10 kOhm, 1 % | Datasheet p. 1, Electrical Characteristics |
| B25/85 | 3435 K, 1 % | Datasheet p. 1 |
| B25/50 | 3380 K, typical | Datasheet p. 1 |
| Operating temperature | -40 to +150 degrees Celsius | Datasheet p. 1 |
| Maximum rated power | 125 mW at 25 degrees Celsius | Datasheet p. 1 (a secondary listing said 100 mW: wrong) |
| Thermal dissipation constant | 1 mW/degC at 25 degrees Celsius | Datasheet p. 1 |
| Permissive operating current | 0.31 mA at 25 degrees Celsius | Datasheet p. 1 |
| Maximum voltage | 5 V | Datasheet p. 1 |
| Package | 0603 (JIS1608): 1.6 x 0.8 x 0.8 mm, grade AEC-Q200 | Datasheet p. 1, Dimension and Construction |
| Rows | 39, -40 to +150 degrees Celsius every 5 | **Unverified**: not in the sheet; consistent with its B values |
| Thermal time constant | not given | The sheet lists none |

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

## What is left to confirm

1. Download the vendor's 1 degC step file named on the datasheet (`rt_sheets/ntcg163jf103ft1s.csv` under the same
   product folder; it answered 403 to an automated fetch) and compare its 5 degree rows with
   `kTdkNtcg163jf103ft1sRows` in `inc/ntc_thermistor_presets.hpp`.
2. Correct any row in the preset and in the model's copy in `tests/mock_ntc_thermistor.hpp`: the model's copy is typed
   separately, so a changed row fails until both agree. Then delete the word "unverified" on the pages that carry it.
3. Check your supply against the sheet's limits (5 V maximum voltage, 0.31 mA permissive current at 25 degrees
   Celsius): 3.3 V across 10 kOhm and 10 kOhm is 0.165 mA at 25 degrees Celsius. `SelfHeatingMicrowatts()` gives the power.
