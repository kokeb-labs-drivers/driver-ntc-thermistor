# Status and limitations

What the driver does not do, what is unverified, and what would settle each.

## Not verified on hardware

The driver is **host-tested against a model of the divider and fuzzed. It has not been run on a thermistor.** The bench
steps that would settle it: read a known part at an ice bath, room temperature and a warm bath with a reference
thermometer and compare with the bound; cut the thermistor and short it and see `OpenCircuit` and `ShortCircuit`; power
the divider from a GPIO and check the settle time against the filter.

The ratio convention (`code / 2^bits` against `code / (2^bits - 1)`) is inside the quantisation and pinned by a test; it
is not verified on hardware.

## The example preset is unverified

The TDK NTCG163JF103FT1S table, B value and tolerances are a secondary listing and were not checked against the
datasheet, whose PDF was not available. See [Presets](presets.md) and [Worked example](worked-example.md). Nothing
generic depends on it.

## What the driver does not do

- **Floating point and libm.** Everything is integer. Steinhart-Hart is a run-time mode (Q16 arithmetic, a cubic and an
  inverse) in its three-coefficient form with R in ohms; for a four-term polynomial, another unit or another
  normalisation, `tools/ntc_table.py list` makes a table from the vendor's own list.
- **Filtering.** Averaging is the driver's; an exponential or other filter across readings is application policy.
- **Self-heating correction.** The power is computed ([Hardware](hardware.md)); the dissipation constant is yours and is
  not applied. `SelfHeatingMicrowatts()` returns 0 for a ratiometric configuration (the supply is unknown), which is
  the same number as "no heating", and rounds twice, so it can be 1 microwatt off at small power.
- **Telling a collapsed supply from a short** on a low-side divider.
- **Extrapolation.** Outside a table or a window is an error, not a guess. A table end is a hard edge: a node a
  fraction of an ADC step beyond the first or last row is a range error. Extend the table.
- **A parallel (linearisation) resistor** across the thermistor. Tabulate the network: give `tools/ntc_table.py` a list
  of the parallel resistance at each temperature.
- **Fractional ohms in a table.** `TablePoint::resistance_ohm` is whole ohms, so a part below about 10 ohm (inrush
  limiters, a hot end below 1 ohm) is outside the useful range. The bounds are `r0_ohm` 1 to 10 MOhm, `b_kelvin` 1000
  to 8000, a series resistor to 16 MOhm, a resistance below 2^32 ohm.
- **A cut wire between the node and the ADC pin.** A floating input reads whatever it last held; the driver cannot tell
  it from a temperature. A pull resistor at the pin (or a second reading with the divider off) is yours.
- **Calibrating the ADC.** The driver takes a raw count and a full scale.
- **Thread safety.** One owner task per object.
- **Beta B in finer units.** `b_kelvin` is an integer; a part that quotes a decimal B rounds, under 0.01 K at the ends.

## What the bound is

`Reading::uncertainty_mc` is a bound on the tolerances you declare, as a linear sum: R, the series resistor, B (beta and
table modes), `curve_error_mc` and one converter step. It applies the tolerance of R as one ratio at every temperature,
where a vendor's tolerance grows away from 25 degrees Celsius: declare the largest. A part whose B tolerance is not
declared adds nothing for it.

## Waits are bounded

`sample_delay_us` and `settle_us` are at most 1 000 000 microseconds each (`Validate()` refuses more), so one `Read()`
takes at most `settle_us + (sample_count - 1) * sample_delay_us` of waiting, 16 seconds in the worst case.

## Choices the owner can change

Each is a small change if the default is not wanted: a fixed-point filter helper outside the class; the divider's code
convention; a series-resistor helper in code (the rule is in [Hardware](hardware.md)); a `dissipation_uw_per_k` field
with a correction flag; a tolerance in codes or ohms before a table end becomes a range error; the removal of
`uncertainty_mc` from `Reading`.
