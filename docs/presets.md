# Presets

A preset is a `Curve` the driver ships as an example. The driver includes the presets header nowhere; a part is added
by writing a `Curve`, never by changing the driver.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    - Beta equation and the B25/85 pair: Public references n/a n/a, Beta model, 1/T = 1/T0 + ln(R/R0) / B, p. 1
    - Steinhart-Hart and the table tool: Public references n/a n/a, Steinhart-Hart, 1/T = a + b ln R + c ln(R)^3, p. 1
    - Example part: R-T table (unverified): TDK unverified unverified, R-T table, -40 to 150 degC (not read), p. 1
    - Example part: B, tolerances (unverified): TDK unverified unverified, R25, B, tolerances (not read), p. 1

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

```cpp
#include "ntc_thermistor_presets.hpp"
```

| Preset | Mode | R25 | B | Source | Status |
| --- | --- | --- | --- | --- | --- |
| `presets::kGeneric10kB3950` | Beta | 10 kOhm | 3950 K (B25/50) | Typical values of the common 10 kOhm / 3950 K class | Typical, not a datasheet claim |
| `presets::kTdkNtcg163jf103ft1s` | Table, 39 rows, -40 to +150 degC every 5 | 10 kOhm | 3435 K (B25/85) | The worked example's TDK NTCG163JF103FT1S | **Unverified until the datasheet is supplied** |

Neither carries a window. A beta curve needs `min_temperature_mc` and `max_temperature_mc` in the `Config`; a table
carries its own range.

!!! warning "Unverified until the datasheet is supplied"
    The rows of `kTdkNtcg163jf103ft1s` are a secondary listing of the vendor's table and were **not** checked against
    the part's datasheet, whose PDF was not available. The 1 % tolerances are the manufacturer's product page, also
    unchecked. A TDK NTCG163JF103FT1 (the 125 degree grade) is another part: verify against its own sheet. Do not rely
    on the rows for a product until they are confirmed; the driver and its tests do not depend on them, and the rest of
    these pages hold for any part.

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
part to the test model with the datasheet's own numbers, and mark it unverified until a reviewer has the PDF.
