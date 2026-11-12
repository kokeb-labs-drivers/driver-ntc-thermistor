# Testing and CI

How the driver is checked: host tests against a model that shares no code with it, a fuzzer that controls what the ADC
returns, compiled documentation, an install test, Cortex-M and ESP32 builds, static analysis and measured quality
gates, on every pull request.

## Host tests

```bash
cmake -S . -B build -DNTC_THERMISTOR_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

| File | Covers |
| --- | --- |
| [`tests/test_ntc_thermistor.cpp`](../tests/test_ntc_thermistor.cpp) | `Validate()` at every boundary (as `static_assert`), presets as data, lifecycle, the rails and margins in both topologies, the window and offset, averaging, `Unstable`, the bound, the helpers |
| [`tests/test_model.cpp`](../tests/test_model.cpp) | The driver against the model: `Ln16()`, exactness at the rows, agreement with the true curve of every part over its range, interpolation bounds, round trip, symmetry, separate supply and reference, no wrap, purity |
| [`tests/test_curves.cpp`](../tests/test_curves.cpp) | Steinhart-Hart mode against `std::log` in both topologies and its validation, the bound's terms (B on a table, `curve_error_mc`), the faults with a reference above the supply, the table ends, the cap on the waits |
| [`tests/generated/`](../tests/generated) | The output of `tools/ntc_table.py` (built at test time) is a valid table that `Convert()` follows to 0.06 K; the tool's own checks run as a test |
| [`tests/test_bias.cpp`](../tests/test_bias.cpp) | The order of port calls and the divider off on every path |
| [`tests/test_errors.cpp`](../tests/test_errors.cpp) | Every error value the driver can return is returned by a real call; `ToString`, `Result`, version |
| [`tests/test_kconfig.cpp`](../tests/test_kconfig.cpp) | The menuconfig defaults, the curve choices |
| [`tests/mock_ntc_thermistor.hpp`](../tests/mock_ntc_thermistor.hpp) | The model and the mock ports |
| [`tests/compile_fail/`](../tests/compile_fail) | Ports and configurations that must not compile |
| [`tests/check_install.cmake`](../tests/check_install.cmake) | `find_package(ntc_thermistor)` from an installed package |
| [`tests/docs/`](../tests/docs) | Every code example on the docs pages, compiled |

The tests build with `-fno-exceptions -fno-rtti` and warnings as errors, on GCC and Clang.

## The model and the parts

The model is written independently of the driver: floating point (`long double`), a divider and ADC model, and the
resistance law of each part, with no code from `inc/`. It covers seven parts; the TDK preset is checked against the
model's separately typed copy of its rows (P7), and tables are also derived from the beta parts at 10, 5 and 2.5 degree
steps for the step rule:

| Part | Law | Driver gets | Why |
| --- | --- | --- | --- |
| P1 | Beta 3950, 10 kOhm | R25 and B | The generic class |
| P2 | Beta 4250, 100 kOhm | R25 and B | A high R25 |
| P3 | Beta 3100, 2.2 kOhm | R25 and B | A low R25, hot resistances of a few ohms |
| P4 | Steinhart-Hart, 47 kOhm | A table, 2.5 degree rows | A curve that is not beta |
| P5 | Steinhart-Hart, 10 kOhm | A table, uneven rows | Coarse and uneven spacing |
| P6 | Beta 3380, 10 kOhm at 20 degrees | R25 and B | A reference other than 25 degrees |
| P7 | The example part's rows, typed again | A table | A table that is data, not a formula |

Each case runs for the parts and for both divider topologies, 10, 12 and 16 bits. The interpolation bounds the
tests assert are pinned numbers measured from the model (see [Conversion](conversion.md)), not loosened to pass.

## Fuzz tests

Everything the driver reads is a code from the port's ADC, so the code is the untrusted input.
[`tests/fuzz/fuzz_ntc_thermistor.cpp`](../tests/fuzz/fuzz_ntc_thermistor.cpp) lets libFuzzer choose a configuration (a
beta curve, the example preset, the generic preset, a table built from fuzzed pairs, or none), then up to 48 driver
calls, and on every conversion what the ADC does: pass the code the divider model gives for the simulated resistance,
fail, return a rail code, a value above scale, or a code that wobbles. After every call it checks the driver's promises
and aborts, naming the line, when one breaks:

- a failed call carries an `Error` this driver returns and a zeroed value; a node code at the open rail is never `Ok`
  and never the other fault;
- before `EnsureInitialized()` succeeds no port call is made; `InvalidArgument` means none was made; an empty curve, or
  a beta curve without a window, is always `InvalidArgument`;
- an `Ok` temperature lies inside the window and, in table mode, inside the table's span plus the offset; in beta mode
  the temperature is above absolute zero before the offset;
- `Ok` never comes from a failed sample, a sample at or above 2^bits, or a spread above `max_spread_codes`;
  `uncertainty_mc` is at least 1 and the mean code is the rounded mean;
- `SetBias` is switched on and off the same number of times in a read and the last call is off, on every error path;
- over 64 sorted probe codes `Convert` is monotone and its errors come as contiguous runs in order;
- every ADC call has `channel == Config::adc_channel`, a read makes at most `sample_count` conversions, and a bad
  `SetConfig()` leaves the old configuration.

```bash
# libFuzzer needs Clang (on macOS, Homebrew's LLVM)
cmake -S . -B build-fuzz -DCMAKE_CXX_COMPILER=clang++ -DNTC_THERMISTOR_BUILD_TESTS=ON -DNTC_THERMISTOR_BUILD_FUZZERS=ON
cmake --build build-fuzz --target fuzz_ntc_thermistor
./build-fuzz/tests/fuzz/fuzz_ntc_thermistor -max_total_time=120 tests/fuzz/corpus/fuzz_ntc_thermistor
```

The same file also builds without libFuzzer as `fuzz_replay_fuzz_ntc_thermistor`, and CTest replays every input in
[`tests/fuzz/corpus/fuzz_ntc_thermistor/`](../tests/fuzz/corpus/fuzz_ntc_thermistor) on GCC and Clang
(`ntc_thermistor_fuzz_ntc_thermistor_corpus`): the seeds, and every input that ever broke a promise (name it
`regress-*`), so a fixed bug stays fixed. CI fuzzes for 120 s on every pull request, from the seeds plus a corpus it
keeps between runs (job `Host quality / fuzz`).

## Quality gates

The `Host quality` job measures the library code, `inc/` and `src/` only (tests and examples are left out), and
fails when a value is over the limits of this repository's assurance level, `high` (set in
[`.kokeb/handbook.yml`](../.kokeb/handbook.yml), defined by the handbook's ADR-0020):

| Gate | Tool and how it is measured | Limit at `high` |
| --- | --- | --- |
| Complexity | lizard 1.24.1, modified cyclomatic complexity (`-m`: a `switch` counts once) per function | CCN at most 15 |
| Function length | lizard, lines of code per function | at most 60 |
| Parameters | lizard, per function | at most 6 |
| Line coverage | GCC 14 `--coverage -O0`, every CTest, gcovr 8.7 `--merge-lines` (a template's lines count once across its instances), `--exclude-unreachable-branches --exclude-throw-branches` | at least 85 % |
| Branch coverage | same run | at least 75 % |
| MC/DC | GCC `-fcondition-coverage`, same run | measured, not gated at `high` (100 % at `safety`) |
| Sanitizers | Clang, `-fsanitize=address,undefined -fno-sanitize-recover=all`, every CTest | no report |
| Fuzzing | libFuzzer with ASan and UBSan, 120 s per target | no broken promise |

The reports are in the run's `host-quality-reports` artifact and its step summary. To run the gates locally:

```bash
# Coverage (GCC 14 or later), then the complexity limits
cmake -S . -B build-cov -DCMAKE_CXX_COMPILER=g++-14 -DNTC_THERMISTOR_BUILD_TESTS=ON \
  -DCMAKE_CXX_FLAGS="--coverage -O0 -fcondition-coverage"
cmake --build build-cov && ctest --test-dir build-cov
gcovr --root . --filter inc/ --filter src/ --merge-lines --exclude-unreachable-branches --txt build-cov
lizard -m -C 15 -L 60 -a 6 inc src

# Sanitizers (Clang)
cmake -S . -B build-san -DCMAKE_CXX_COMPILER=clang++ -DNTC_THERMISTOR_BUILD_TESTS=ON \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all -O1 -g"
cmake --build build-san && ctest --test-dir build-san --output-on-failure
```

The fuzz gate runs locally as shown in [Fuzz tests](#fuzz-tests).

clang-tidy 20 ([`_config/.clang-tidy`](../_config/.clang-tidy): bugprone, CERT, clang-analyzer,
cppcoreguidelines, modernize, performance, portability and readability, each disabled check with its reason) and
cppcheck (strict) run in the `C++ lint` and `Repository checks` jobs and fail on any finding.

The `MISRA` job ([`ci-misra.yml`](../.github/workflows/ci-misra.yml)) runs the shared `ru-misra` workflow of
ci-tools at level `high`: it traces a host build and the Cortex-M build (`ntc_thermistor_optimised_build`) of every
translation unit, checks them against MISRA C++:2023 and the handbook's C++20 profile (the CodeQL coding-standards
and profile packs, plus the validated clang-tidy and cppcheck checks), and reports each finding against the
handbook's compliance matrix and deviation records. A finding is fixed in the code; a deliberate exception names
its deviation on the line before it (`// codeql::misra_deviation_next_line(MD-CPP-009)` above the `.ipp` include
of `inc/ntc_thermistor.hpp`). Until the repository is public the job runs with `open_source_prerelease: true`, the
declaration that the code is released under the licence in `LICENSE` and is to be published.

## CI workflows

Everything runs from [`ci.yml`](../.github/workflows/ci.yml), which calls one workflow file per check and ends in
the `CI gate` job, the single required check (`CI gate / gate`).

| Job | What it checks |
| --- | --- |
| Host tests | GCC, then Clang (one job), build and run every test above, then build and run `examples/host` on its own |
| Host quality | The gates above: complexity, coverage, sanitizers, fuzzing |
| Cross-compile | `arm-none-eabi-g++` for Cortex-M0+, M4F and M7F with the strict flags, warnings as errors (nothing linked) |
| C++ lint | clang-format and clang-tidy over the sources |
| MISRA | MISRA C++:2023 and the C++20 profile on traced host and Cortex-M builds, at level `high` (see [Quality gates](#quality-gates)) |
| Docs | Strict site build (`_config/mkdocs.yml`), generated-region drift, Doxygen (`_config/Doxyfile`), offline link check |
| Repository checks | One job: DCO on pull requests, the org standard, Markdown and YAML lint, the [Kokeb Labs handbook](https://github.com/kokeb-labs/handbook) rules for `.kokeb/handbook.yml` (by rule ID), template drift (warns only) and cppcheck over the driver sources |
| CI gate | Passes only if every job above passed: the one required check |

The **ESP32 examples** job builds every app in `examples/esp32/app_config.yml` with the repository root as the
driver's component. The gate needs it too.
