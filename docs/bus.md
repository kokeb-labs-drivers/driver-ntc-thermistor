# On the bus

An NTC thermistor has no bus: it is a resistor in a voltage divider, and your port reads the divider's midpoint on an
ADC channel in millivolts. The divider and the ADC are on the [Hardware](hardware.md) page.

## Signal timing

The example part's datasheet (TDK NTCG163JF103FT1S, Rev.3) gives no timing: no thermal time constant, no response
time and no settling time, and the driver depends on none. A divider switched from an MCU pin, or a filter capacitor
at the ADC input, settles in a time set by your own parts; give it the wait in your port before the first
conversion. So this page has no waveform.

## Several devices

Each thermistor has its own divider and needs its own ADC channel, or a channel of an analog multiplexer in front of
one. Nothing is shared but the supply and the ground, so there is no topology figure.

## How these figures are made

A bus figure is a model in `docs/model/` that cites the datasheet section and page of every number;
`kokeb-docs figures` draws it and the build fails on a number without a citation. This driver has none, because the
datasheet gives no timing to draw.
