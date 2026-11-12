# NTC thermistor host example

The driver on your PC, with no hardware. The host port in
[`host_ntc_thermistor_port.hpp`](host_ntc_thermistor_port.hpp) connects the real driver to a simulated divider and
converter (the model the host tests use). [`main.cpp`](main.cpp) reads four simulated parts over a range of
temperatures, then provokes each fault.

## Build and run

From the repository root, with CMake 3.16+ and a C++20 compiler:

```bash
cmake -S examples/host -B build-example
cmake --build build-example
./build-example/ntc_thermistor_host_example
```

## Expected output

The values below are from one run; the last line is the exit code check, and the program exits 0 only when it is
printed.

```text
NTC thermistor driver 0.1.0 on the host
Table part (preset, low-side divider, 12 bit)
    25 C: read 25.027 C +- 0.298 K, R 9990 ohm, code 2047
Beta part (generic 10 kOhm / B 3950)
    25 C: read 25.000 C +- 0.022 K, R 10000 ohm, code 2048
Faults
  thermistor cut (code 4095): OpenCircuit
  thermistor shorted (code 0): ShortCircuit
all readings within their bounds
```

The table part is the example preset, whose rows are unverified until the datasheet is supplied; the host example
uses a simulated part built from the same rows, so it checks the driver, not the data.

The host tests build this folder too and run the example (CTest `ntc_thermistor_host_example`), so CI fails if it
stops compiling or a reading leaves its stated bound.

## Files

| File | What it is |
| --- | --- |
| [`main.cpp`](main.cpp) | The example: four parts, two divider topologies, the faults |
| [`host_ntc_thermistor_port.hpp`](host_ntc_thermistor_port.hpp) | The host port: `ReadRaw()` over the simulated divider, `SetBias()` and `DelayUs()` |
| [`CMakeLists.txt`](CMakeLists.txt) | Standalone project; adds the driver with `add_subdirectory(../..)` |
