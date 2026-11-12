# Troubleshooting

Every error the driver returns, what it means, and what to measure. Probe the node with a multimeter or a scope first:
it should sit between the rails, move slowly with temperature, and be about `Rs / (Rs + Rn)` of the supply.

| Error | What it means | Check |
| --- | --- | --- |
| `InvalidArgument` | The configuration is not valid | Call `Validate(config)` and look at each field against [Configuration](configuration.md): is there a curve? a window for a beta or Steinhart-Hart curve? a wait over 1 000 000 us? both voltages or neither? a delay without `DelayUs()`? |
| `NotInitialized` | A read before `EnsureInitialized()` succeeded | Call it first and check its result |
| `Bus` | `ReadRaw()` or `SetBias()` returned `false` | The ADC unit, the channel and the pin in your port; the bias GPIO |
| `OpenCircuit` | The node is at the open rail | A cut wire or missing part (the node floats to the supply on a low-side divider); a divider that is not powered; a thermistor not soldered |
| `ShortCircuit` | The node is at the short rail | A shorted part or wire; a collapsed divider supply on a low-side divider; the wrong channel; the divider side declared the wrong way round |
| `BelowRange` | Colder than the curve or the window, or the resistance is beyond 32 bits | The part really is colder; the series resistor or its declared value is wrong; the window is too narrow |
| `AboveRange` | Hotter than the curve or the window, or a resistance of zero | The part really is hotter; check the topology; a node at the rail with a margin of 0 |
| `Unstable` | The samples differ by more than `max_spread_codes` | A floating or noisy input; the ADC's source impedance is too high for the divider (add 100 nF, lower `Rs`); a switching load on the same rail |
| `CodeOutOfScale` | A sample is at or above 2^`adc_bits` | The port returns more bits than `adc_bits` says: fix one of them |

## A reading is plausible but wrong

| Symptom | Cause |
| --- | --- |
| Off by a constant | The thermistor's own tolerance or the series resistor's: measure R with a meter at a known temperature; set `offset_mc` or the real `series_ohm` |
| Off by more at the ends than in the middle | A beta curve on a part that is not one: use Steinhart-Hart coefficients or a table ([Conversion](conversion.md)) |
| Too warm, and warmer the more you read | Self-heating: lower the supply, raise `Rs`, or power the divider only while sampling (`SetBias()`) |
| Off by a few percent with a separate ADC reference | `adc_vref_mv` and `divider_supply_mv` are the real voltages? [Reference, range and supplies](reference-and-supplies.md) |
| Moves in the wrong direction | `topology` is the wrong way round |
| Resolution coarse at the ends | Normal: one step is worth more where the node nears a rail ([Hardware](hardware.md#what-an-adc-step-is-worth)) |

## The unit of the number

`temperature_mc` is **milli**-degrees Celsius: 25 000 is 25 degrees. `ToMilliKelvin()` and `ToMilliFahrenheit()`
convert.
