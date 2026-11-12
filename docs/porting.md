# Porting

Run the driver on a new MCU by writing one class: the port.

The driver calls hooks on a class you derive from `ntc_thermistor::AdcInterface<Self>` (in
[`inc/ntc_thermistor_adc_interface.hpp`](../inc/ntc_thermistor_adc_interface.hpp)). The base declares no hook; the
driver template is constrained by the `ntc_thermistor::AdcPort` concept, so a missing or misdeclared hook is a
compile error with a message that names it.

## What you implement

| Method | Required | Contract |
| --- | --- | --- |
| `bool EnsureInitialized() noexcept` | yes | Bring the ADC up on the first call; `true` at once afterwards |
| `bool ReadRaw(uint8_t channel, uint32_t& code) noexcept` | yes | One conversion of `channel`: the raw count, 0 to 2^`adc_bits` - 1; no averaging, no volts; `false` on any failure |
| `void DelayUs(uint32_t us) noexcept` | only if the configuration waits | Wait at least `us` microseconds (`sample_delay_us`, `settle_us`) |
| `bool SetBias(bool on) noexcept` | no | Power the divider only while sampling; `false` when the pin could not be driven |

Optional hooks are found by name at compile time and must have exactly these signatures; a hook with the right name
and a wrong signature (a narrowed parameter, a missing `noexcept`, a `void` return) does not compile. The driver
refuses at `EnsureInitialized()` a configuration that waits for something the port cannot do: a `sample_delay_us` or
`settle_us` without `DelayUs()`, or a `settle_us` without `SetBias()`.

## The port

```cpp title="my_port.hpp"
--8<-- "tests/docs/my_port.hpp:port"
```

A port with every hook, powering the divider from a GPIO only while it is read (which also removes the self-heating
between readings):

```cpp
--8<-- "tests/docs/ports.cpp:full-port"
```

## Bias

When the port has `SetBias()`, every `Read()` runs this order: **on, settle, samples, off, convert**. The divider is
switched off on every path, including a failed switch-on, a failed sample, an out-of-scale code and `Unstable`.
`EnsureInitialized()` also switches it off once, so the first reading starts from a known state. A failed switch-off
after a good read is `Bus`: a divider left on is a fault. There are no retries.

The pin's idle level is "off": set it before the output is enabled (handbook KB-HW-018), or the divider glitches on at
start-up. A port for a board where the divider is always powered either leaves `SetBias()` out (then `settle_us` must be
0) or provides it as a successful no-op, as the ESP32 example does when no GPIO is set: a `SetBias()` that returns
`false` is a `Bus` error at `EnsureInitialized()`.

## Codes and millivolts

Return the raw count. If your SDK gives only calibrated millivolts, map them back to a code of `adc_bits` bits over the
full scale and declare that full scale (see [Reference, range and supplies](reference-and-supplies.md)):

```cpp
--8<-- "tests/docs/ports.cpp:millivolt-port"
```

The ESP32 example, [`esp32_ntc_thermistor_port.hpp`](../examples/esp32/main/esp32_ntc_thermistor_port.hpp), uses the
one-shot ADC and an optional GPIO.

## Describing your part and your hardware

Porting also covers the documentation of a board: [Hardware](hardware.md) has the recipe for adapting `hardware.yml`
and the chip card to your thermistor, and [Presets](presets.md) how to declare the part as data.

## Threads

The driver is not thread-safe. Give each driver object one owner task, or guard every call with one mutex.
