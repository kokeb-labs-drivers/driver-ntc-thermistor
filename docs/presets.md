# Presets

A preset is a `Curve` the driver ships as an example. The driver includes the presets header nowhere; a part is added
by writing a `Curve`, never by changing the driver.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    - Beta equation and the B25/85 pair: Public references n/a n/a, Beta model, 1/T = 1/T0 + ln(R/R0) / B, p. 1
    - Steinhart-Hart and the table tool: Public references n/a n/a, Steinhart-Hart, 1/T = a + b ln R + c ln(R)^3, p. 1
    - Example part: R-T table (rows unverified): TDK none printed Rev.3, Electrical Characteristics, 1degC Step Data link (the table itself is not in the PDF), [p. 1](https://product.tdk.com/system/files/dam/doc/product/sensor/ntc/chip-ntc-thermistor/data_sheet/datasheet_ntcg163jf103ft1s.pdf#page=1)
    - Example part: R25, B and tolerances: TDK none printed Rev.3, Electrical Characteristics: Resistance, B value, [p. 1](https://product.tdk.com/system/files/dam/doc/product/sensor/ntc/chip-ntc-thermistor/data_sheet/datasheet_ntcg163jf103ft1s.pdf#page=1)

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

```cpp
#include "ntc_thermistor_presets.hpp"
```

| Preset | Mode | R25 | B | Source | Status |
| --- | --- | --- | --- | --- | --- |
| `presets::kGeneric10kB3950` | Beta | 10 kOhm | 3950 K (B25/50) | Typical values of the common 10 kOhm / 3950 K class | Typical, not a datasheet claim |
| `presets::kTdkNtcg163jf103ft1s` | Table, 39 rows, -40 to +150 degC every 5 | 10 kOhm | 3435 K (B25/85) | The worked example's TDK NTCG163JF103FT1S | R25, B, tolerances: TDK Rev.3, p. 1. Rows: **unverified** |

Neither carries a window. A beta curve needs `min_temperature_mc` and `max_temperature_mc` in the `Config`; a table
carries its own range.

!!! warning "The rows are unverified"
    The datasheet of `kTdkNtcg163jf103ft1s` (TDK, Rev.3, 8 May 2023, one page) confirms R25 10 kOhm +-1 %, B25/85
    3435 K +-1 %, B25/50 3380 K typ. and the range -40 to +150 degrees Celsius. It prints **no resistance table**: the
    step data is a separate file (403 to an automated fetch). The 39 rows are a secondary listing, consistent with
    those B values (a test checks it) but not compared row by row. A TDK NTCG163JF103FT1 (the 125 degree grade) is
    another part: verify against its own sheet. Do not rely on the rows for a product until you have compared them
    with the vendor's step file; the driver and its tests do not depend on them, and the rest of these pages hold for
    any part.

!!! note "The generic preset is a convenience"
    There is no single datasheet behind "10 kOhm, B 3950": it is the typical value of a class sold as probes and
    beads. Read the datasheet of yours and override R25, B and the window.

## Add your own part

A part is a `Curve`. From the datasheet take the table, the Steinhart-Hart coefficients, or R25 and B.

**From R25 and B** (beta mode):

```cpp title="configuration.cpp"
--8<-- "tests/docs/configuration.cpp:own-part"
```

`static_assert` on `Validate()` turns a bad value into a compile error, with the same rules the driver applies at
start-up.

**From Steinhart-Hart coefficients** (A, B, C in units of 1e-15 per kelvin, a window, and the fit's error):

```cpp title="configuration.cpp"
--8<-- "tests/docs/configuration.cpp:steinhart-hart"
```

**From a table** (any 2 to 256 rows):

```cpp title="configuration.cpp"
--8<-- "tests/docs/configuration.cpp:table"
```

The rows must outlive the driver: a `static constexpr` array. Resistance must fall strictly and temperature rise
strictly; rows that round to the same logarithm are refused. `tools/ntc_table.py` makes a validated table from
coefficients, from R25 and B, or from a vendor's list, and prints the interpolation error
([Conversion](conversion.md#making-a-table)).

**To contribute a preset**, add the rows and the source (document, table, revision) to the comment above them, add the
part to the test model with the datasheet's own numbers, and mark it unverified until a reviewer has compared it with
the vendor's own table.
