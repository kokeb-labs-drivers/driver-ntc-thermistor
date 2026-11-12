---
hide:
  - navigation
  - toc
---

<div class="kb-hero" markdown>
<span class="kb-kicker">Driver</span>

# NTC thermistor driver

<p>A C++20 driver for any NTC thermistor read through a resistor divider into an ADC input: it turns a raw code into
milli-degrees Celsius with integers only.</p>

[Quickstart](quickstart.md){ .md-button .md-button--primary }
[API guide](api-guide.md){ .md-button }
[Source on GitHub](https://github.com/kokeb-labs-drivers/driver-ntc-thermistor){ .md-button }
</div>

The driver knows no part. A thermistor is **data**: a table of resistance and temperature rows, Steinhart-Hart
coefficients, or R25 and B for the beta equation, given in a `Curve`. The driver reads the divider node through a small
port class you write, finds the thermistor's resistance, and looks the temperature up on the curve. Open and short
circuits, a node outside the
ADC's range and a part outside its rated window are errors with their own names, and every reading carries a bound.

<div class="kb-chip" markdown>
<div class="kb-chip__head">
<span class="kb-chip__kicker">Temperature sensor, two terminals</span>
<span class="kb-chip__name">NTC thermistor</span>
<span class="kb-chip__vendor">Any maker: the part is data</span>
</div>

Measures :   Temperature from the curve you declare, as milli-degrees Celsius

Interface :   A voltage divider into one ADC channel; optionally a GPIO that powers the divider only while it is read

Arithmetic :   Integers only: ohms, milli-degrees, millivolts, parts per million; no floating point, no library calls
</div>

## What you get

- **Any NTC, no change of code.** A part is a `Curve`; the driver ships two presets and the way to write your own.
  See [Presets](presets.md) and [Configuration](configuration.md).
- **Three ways to describe a part**: a table of any 2 to 256 rows and any spacing, interpolated linearly in ln R,
  Steinhart-Hart coefficients, or R25 and B. `tools/ntc_table.py` makes a table from any of them or from a vendor's
  list. See [Conversion](conversion.md).
- **Faults by name**: `OpenCircuit`, `ShortCircuit`, `BelowRange`, `AboveRange`, `Unstable`, `CodeOutOfScale`.
  See [Troubleshooting](troubleshooting.md).
- **A bound on every reading** from the tolerances you declare and one converter step.
- **Tested without hardware** against a model that shares no code with the driver. See [Testing and CI](testing.md).

!!! warning "Not verified on hardware"
    The driver is host-tested against a model of the divider and fuzzed; it has not been run on a thermistor. The
    example preset (a TDK part) is a secondary listing of the vendor's table and is **unverified until the datasheet
    is supplied**. Nothing generic depends on it. See [Status and limitations](limitations.md).

## Datasheet

<!-- generated: datasheet-card -->
| Document | Revision | Get it |
| --- | --- | --- |
| Reference: Beta equation, Steinhart-Hart equation and the voltage divider (Public references n/a) | n/a, n/a | [vendor page](https://en.wikipedia.org/wiki/Steinhart%E2%80%93Hart_equation) |
| Datasheet: NTCG163JF103FT1S chip NTC thermistor (worked example; unverified until the datasheet is supplied) (TDK unverified) | unverified, unverified | [vendor page](https://product.tdk.com/en/search/sensor/ntc/chip-ntc-thermistor/info?part_no=NTCG163JF103FT1S) |

Written against these revisions; the SHA-256 of each file is in [`docs/datasheets.yml`](datasheets.yml). Where each part
of the driver is in the datasheet: [Datasheet map](datasheet.md).
<!-- /generated -->
