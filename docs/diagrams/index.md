# Figures

<!-- Written by kokeb-docs figures from docs/diagrams/figures/*.py. Do not edit by hand. -->

Every figure on this site is drawn by a Python module from facts extracted from the code, so it is rebuilt,
not redrawn, when the code changes. CI runs `kokeb-docs figures --check` and fails when a
committed figure no longer matches the code. Each figure below lists the files it is drawn from.

## Inside the NTC thermistor (generic)

<!-- figure: chip-blocks -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-blocks.dark.svg">
  <img src="chip-blocks.svg"
    alt="Block diagram of the NTC thermistor (generic): its function blocks, how they connect and which
    package pins reach each block.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/ntc_thermistor.hpp`](../../inc/ntc_thermistor.hpp)
- [`inc/ntc_thermistor_adc_interface.hpp`](../../inc/ntc_thermistor_adc_interface.hpp)

## What the NTC thermistor (generic) does

<!-- figure: chip-capabilities -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-capabilities.dark.svg">
  <img src="chip-capabilities.svg"
    alt="Capability map of the NTC thermistor (generic): Voltage divider (supported): Turns the voltage at
    the divider node into the thermistor's resistance, thermistor low side or high side; Beta equation
    (supported): Temperature from R25 and B (B25/85 by default), in fixed point, inside the rated window
    you declare; Steinhart-Hart (supported): Temperature from the three Steinhart-Hart coefficients, in
    integers; Resistance table (supported): Temperature from a table of resistance and temperature rows,
    interpolated in ln R; any part is data; Open, short and range faults (supported): Names a cut wire,
    a short, a part outside its window, noisy samples and an ADC that disagrees; Reading uncertainty
    (supported): Bounds every reading from the tolerances you declare and one converter step; Self-
    heating (supported): Estimates the power the divider dissipates in the thermistor, to size the
    series resistor; Switched bias (supported): Switches the divider supply from an MCU pin and waits
    for it to settle, to cut current and self-heating.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/ntc_thermistor.hpp`](../../inc/ntc_thermistor.hpp)
- [`inc/ntc_thermistor_adc_interface.hpp`](../../inc/ntc_thermistor_adc_interface.hpp)

## NTC thermistor (generic): Low-side divider

<!-- figure: chip-circuit-divider -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-circuit-divider.dark.svg">
  <img src="chip-circuit-divider.svg"
    alt="Schematic of the NTC thermistor (generic) low-side divider: the chip with U2 MCU, R1 resistor 10 kΩ,
    C1 capacitor 100 nF; the MCU wires carry the port hooks ReadRaw().">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/ntc_thermistor.hpp`](../../inc/ntc_thermistor.hpp)
- [`inc/ntc_thermistor_adc_interface.hpp`](../../inc/ntc_thermistor_adc_interface.hpp)

## NTC thermistor (generic): High-side divider

<!-- figure: chip-circuit-divider-high -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-circuit-divider-high.dark.svg">
  <img src="chip-circuit-divider-high.svg"
    alt="Schematic of the NTC thermistor (generic) high-side divider: the chip with U2 MCU, R1 resistor 10
    kΩ, C1 capacitor 100 nF; the MCU wires carry the port hooks ReadRaw().">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/ntc_thermistor.hpp`](../../inc/ntc_thermistor.hpp)
- [`inc/ntc_thermistor_adc_interface.hpp`](../../inc/ntc_thermistor_adc_interface.hpp)

## NTC thermistor (generic)

<!-- figure: chip-hero -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-hero.dark.svg">
  <img src="chip-hero.svg"
    alt="Overview of the NTC thermistor (generic), a resistor whose resistance falls as it heats: with a
    series resistor it makes a voltage divider an ADC can read, and a curve (a table, Steinhart-Hart
    coefficients, or R25 and B) turns the resistance into a temperature. A MCU talks to it over ADC
    input; it reads back Temperature. Headline figures: 10 kΩ resistance at 25 °C, ±1 % (the example
    part); 3435 K B value at 25/85 °C, ±1 % (the example part); -40 to 150 °C operating temperature (the
    example part); 0.31 mA permissive operating current at 25 °C (the example part).">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/ntc_thermistor.hpp`](../../inc/ntc_thermistor.hpp)
- [`inc/ntc_thermistor_adc_interface.hpp`](../../inc/ntc_thermistor_adc_interface.hpp)

## NTC thermistor (generic) package, 0603

<!-- figure: chip-outline -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-outline.dark.svg">
  <img src="chip-outline.svg"
    alt="Outline of the NTC thermistor (generic) in 0603 drawn to scale from above: body 1.6 x 0.8 x 0.8 mm,
    2 pins.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/ntc_thermistor.hpp`](../../inc/ntc_thermistor.hpp)
- [`inc/ntc_thermistor_adc_interface.hpp`](../../inc/ntc_thermistor_adc_interface.hpp)

## NTC thermistor (generic) pinout, 0603

<!-- figure: chip-pinout -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-pinout.dark.svg">
  <img src="chip-pinout.svg"
    alt="Pinout of the NTC thermistor (generic) in 0603, top view: 2 pins, each with its name, its electrical
    type as a colour and the driver hook that uses it.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/ntc_thermistor.hpp`](../../inc/ntc_thermistor.hpp)
- [`inc/ntc_thermistor_adc_interface.hpp`](../../inc/ntc_thermistor_adc_interface.hpp)

## Wiring: Low-side divider

<!-- figure: chip-wiring-divider -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-wiring-divider.dark.svg">
  <img src="chip-wiring-divider.svg"
    alt="Wiring of the NTC thermistor (generic) for the Low-side divider: MCU ADC input to T1 (ReadRaw()).
    Supplies, grounds and the support parts are listed under the drawing.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/ntc_thermistor.hpp`](../../inc/ntc_thermistor.hpp)
- [`inc/ntc_thermistor_adc_interface.hpp`](../../inc/ntc_thermistor_adc_interface.hpp)

## Wiring: High-side divider

<!-- figure: chip-wiring-divider-high -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-wiring-divider-high.dark.svg">
  <img src="chip-wiring-divider-high.svg"
    alt="Wiring of the NTC thermistor (generic) for the High-side divider: MCU ADC input to T2. Supplies,
    grounds and the support parts are listed under the drawing.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/ntc_thermistor.hpp`](../../inc/ntc_thermistor.hpp)
- [`inc/ntc_thermistor_adc_interface.hpp`](../../inc/ntc_thermistor_adc_interface.hpp)

## From an ADC code to a temperature

<!-- figure: conversion-pipeline -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="conversion-pipeline.dark.svg">
  <img src="conversion-pipeline.svg"
    alt="Pipeline of six stages: the mean of the samples, the classification of the code as open circuit,
    short circuit or out of scale, the divider's node ratio giving the resistance, the curve (table,
    Steinhart-Hart or beta) giving the temperature, the window and offset, and the uncertainty bound.">
</picture>
<!-- /figure -->

Figure module: [`docs/diagrams/figures/conversion-pipeline.py`](../../docs/diagrams/figures/conversion-pipeline.py)

Drawn from:

- `ConvertCode()` in [`inc/ntc_thermistor_conversion.hpp`](../../inc/ntc_thermistor_conversion.hpp)
- `FineOfCode()` in [`inc/ntc_thermistor_conversion.hpp`](../../inc/ntc_thermistor_conversion.hpp)
- `CurveTemperature()` in [`inc/ntc_thermistor_conversion.hpp`](../../inc/ntc_thermistor_conversion.hpp)

## The layers, one of them yours

<!-- figure: driver-layers -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="driver-layers.dark.svg">
  <img src="driver-layers.svg"
    alt="Layer diagram: your application calls the NtcThermistor core, which asks for ADC conversions through
    the ADC port contract to the port class you write, which reads the divider; the core and the
    contract are the same on every MCU.">
</picture>
<!-- /figure -->

Figure module: [`docs/diagrams/figures/driver-layers.py`](../../docs/diagrams/figures/driver-layers.py)

Drawn from:

- `public calls` in [`inc/ntc_thermistor.hpp`](../../inc/ntc_thermistor.hpp)
- `AdcInterface` in [`inc/ntc_thermistor_adc_interface.hpp`](../../inc/ntc_thermistor_adc_interface.hpp)
