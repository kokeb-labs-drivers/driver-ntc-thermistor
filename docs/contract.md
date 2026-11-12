# Contract

What the NTC thermistor driver promises, what it needs, and the rules a caller follows.

The driver inherits the
[Kokeb Labs driver contract](https://github.com/kokeb-labs/.github/blob/main/standards/driver.md);
this page states only what is specific to it.

## Promises

- The constructor is `noexcept`, stores the configuration, does no I/O and cannot fail.
- `EnsureInitialized()` checks the configuration (no I/O), brings the port's ADC up and, if the port has `SetBias()`,
  switches the divider off. A thermistor has no identity, so there is nothing to check on the part.
- Every read returns `Error::NotInitialized` before `EnsureInitialized()` succeeds and makes no port call. No call
  starts anything as a side effect.
- `Convert()`, `ConvertResistance()` and `Validate()` are pure: equal inputs, equal outputs, no state.
- All arithmetic is integer. No intermediate overflows or wraps: 64-bit intermediates are bounded by the limits
  `Validate()` enforces, and a result that does not fit saturates (the bound, the offset) or is an error (a
  resistance beyond 32 bits is `BelowRange`).
- A read takes at most `sample_count` conversions. With `SetBias()` the order is on, settle, samples, off, convert, and
  the divider is off again on every path, including every error.
- A failed call returns an `Error` this driver returns and a zero value. `Ok` never comes from a failed sample, a sample
  at or above 2^`adc_bits`, or samples whose spread is above `max_spread_codes`.
- An `Ok` temperature is inside the window (and, in table mode, inside the table's span) before the offset is added;
  `uncertainty_mc` is at least 1.
- A node at the open rail is never `Ok` and never reported as a short; the open and short margins follow the fault,
  not the rail, and swap with the topology.
- `ReadRawCode()` is a single-shot raw access: one conversion, no averaging, no retry, the same bias sequence;
  `GetLastCode()` returns the last code the port gave, with no I/O. Neither has a side effect on the part.
- Every port failure propagates as `Error::Bus`. There are no retries: a failed sample ends the read.
- A configuration the driver cannot honour (a wait the port cannot make, a beta or Steinhart-Hart curve without a
  window, an empty curve, a wait over one second)
  is `InvalidArgument` before any I/O. `SetConfig()` leaves the old configuration on failure.

## Depends on

| Dependency | Version | Why |
| --- | --- | --- |
| A C++20 compiler | GCC 10, Clang 13 or later | Concepts constrain the port |
| CMake | 3.16 or later | Generates `ntc_thermistor_version.h` |
| Your port | | The ADC, and optionally a delay and a GPIO that powers the divider |

There is no dependency on a math library, the C++ standard library's `<cmath>` or floating point.

## Threading

Not thread-safe; takes no lock. One owner task, or one mutex around every call.

## Errors

| `Error` | Value | Returned when |
| --- | --- | --- |
| `Ok` | 0 | The call succeeded |
| `NotInitialized` | 1 | Before `EnsureInitialized()` succeeded |
| `InvalidArgument` | 2 | The configuration is not valid |
| `Bus` | 3 | The port failed |
| `OpenCircuit` | 16 | The node is at the open rail |
| `ShortCircuit` | 17 | The node is at the short rail |
| `BelowRange` | 18 | Colder than the curve or the window; or a resistance beyond 32 bits |
| `AboveRange` | 19 | Hotter than the curve or the window; or a resistance of zero |
| `Unstable` | 20 | The samples disagree by more than `max_spread_codes` |
| `CodeOutOfScale` | 21 | A sample is at or above 2^`adc_bits` |

`Timeout`, `DeviceIdMismatch`, `Crc`, `Busy`, `DeviceFault` and `Unsupported` are never returned; a test checks that
every value above is returned by some call.

## Memory

No heap, exceptions, RTTI or static mutable state. The object holds a port reference and a `Config`. A table is not
copied: the span points at your array, which must outlive the driver.

## Platform

Platform-free; the ESP-IDF port in `examples/esp32` is an example.

## Deviations from the standard

None.
