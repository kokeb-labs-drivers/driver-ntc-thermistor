# Changelog

All notable changes to this repository are listed here. Versions follow [Semantic Versioning](https://semver.org/).
Entries under **Breaking** need source changes in code that used the affected API; any of them calls for a major
version bump (a minor bump before 1.0).

## [Unreleased]

### Added

- First version: a generic driver for NTC thermistors read through a resistor divider into an ADC port.
  - `NtcThermistor<Port>` with `EnsureInitialized()`, `Read()`, `ReadResistance()`, `ReadRawCode()`, `GetLastCode()`,
    `SetConfig()` and `GetConfig()`; the port
    needs `EnsureInitialized()` and `ReadRaw()`, and may add `DelayUs()` and `SetBias()`.
  - Pure conversion functions with no port: `Convert()`, `ConvertResistance()`, `ToMilliKelvin()`,
    `ToMilliFahrenheit()`, `SelfHeatingMicrowatts()` and `Validate()`. All arithmetic is integer: no floating point, no
    libm.
  - A part is data: a `Curve` that is a table of 2 to 256 rows (interpolated linearly in ln R), Steinhart-Hart
    coefficients (`sh_a_e15`, `sh_b_e15`, `sh_c_e15`, Q16 integer maths, with a mandatory valid window) or R25 and B
    (beta mode, likewise). `tools/ntc_table.py` makes and checks a table from coefficients, R25 and B, or a vendor list.
    Two presets: a generic 10 kOhm / B 3950 class and a TDK table that is unverified until
    the datasheet is supplied.
  - The bound `uncertainty_mc` is a bound on the declared tolerances: R, the series resistor, B (beta and table
    modes), `curve_error_mc` and one converter step.
  - Both divider topologies, ratiometric or with separate supply and reference, open and short margins, averaging with a
    spread check, an optional bias switch (waits are capped at 1 s), and `uncertainty_mc` on every reading. With a
    reference above the supply the fault margins count from the highest code the supply can produce.
  - Errors `OpenCircuit`, `ShortCircuit`, `BelowRange`, `AboveRange`, `Unstable` and `CodeOutOfScale`.
  - Host tests against an independent seven-part model, a fuzz target, compile-fail tests, a host example and an ESP-IDF
    example.
