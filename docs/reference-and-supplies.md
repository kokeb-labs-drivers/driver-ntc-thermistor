# Reference, range and supplies

The divider and the ADC share a voltage, or they do not. This page says which case you have, what to declare, and how
to keep the node inside the ADC's range.

## Ratiometric: the common case

If the divider's supply **is** the ADC's reference (a MCU whose ADC measures against its own 3.3 V, with the divider
on the same rail), the code is a pure ratio and the voltage drops out. Leave `adc_vref_mv` and
`divider_supply_mv` at 0. Noise on the supply moves both together.

## Not ratiometric

If the reference is another voltage (an ADC with an internal 1.1 V, 2.5 V or 3.3 V reference; a divider on 5 V
read by a 3.3 V ADC), set **both**:

- `divider_supply_mv`: the voltage the divider is powered from;
- `adc_vref_mv`: the ADC's full scale.

The node share is then `code / 2^bits * vref / supply`. If the supply is larger than the reference, choose the series
resistor so that the node stays below the reference over the whole temperature range. When the reference is above the
supply, the node cannot read above the code `2^bits * supply / reference`, and the margin of the open rail (or the
short rail of a high-side divider) is counted down from there: a 12-bit converter with a 3300 mV reference on a
3000 mV supply tops out at code 3723, and with the default margin of 2 the codes from 3720 up are the fault.

```cpp title="configuration.cpp"
--8<-- "tests/docs/configuration.cpp:non-ratiometric"
```

With a separate reference the open and short margins must be at least 1 code: a code at the very rail cannot be told
from a wrong supply value. The conversion depends on the two voltages directly: a 1 % error in `divider_supply_mv` is
a 1 % error in the node share, which is more than a degree at the ends of a part. Measure the supply or use a
ratiometric divider.

## An ADC that is not linear to its reference

Some ADCs (for example a microcontroller ADC with attenuation) have a full scale that depends on a setting and a
transfer curve that is not exactly straight. The driver takes a raw count and a full scale in millivolts: it does not
calibrate. Either calibrate in the port (return a code scaled to the full scale), or declare the effective full scale
from a measurement of your board. The ESP32 example does the second and says so in its README.

## Range

The node is at its middle where the thermistor equals the series resistor. The usable range of the part is where the
node is not within the margins of a rail and where one ADC step is worth less than the accuracy you need:
[Hardware](hardware.md#what-an-adc-step-is-worth).

## A supply that collapses

A low-side divider whose supply has collapsed reads as a short (the node is at ground); the driver cannot tell the two
apart. Measure the supply on a second channel if that matters.
