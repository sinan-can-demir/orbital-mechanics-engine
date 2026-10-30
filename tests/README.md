# Tests

This directory contains the full test suite for the orbital mechanics engine.

## Running all tests

### C++ tests (via CTest)

```bash
cd build && ctest --output-on-failure
```

### Python API tests (via pytest)

```bash
pytest tests/test_python_api.py -v
```

---

## Test files

| File | Type | What it tests |
|------|------|---------------|
| `test_conservation.cpp` | C++ | Energy drift < 1e-5 over 10k steps (RK4) |
| `test_two_body.cpp` | C++ | RK4 two-body radius/energy stability over 30 days; swept angle matches Kepler's 3rd law |
| `test_leapfrog.cpp` | C++ | Symplectic energy drift bounded and non-monotonic |
| `test_yoshida4.cpp` | C++ | Yoshida4 energy bounded and better than Leapfrog at the same dt |
| `test_hermite.cpp` | C++ | Hermite O(dt^4) convergence; beats RK4 at equal step count |
| `test_barycenter.cpp` | C++ | COM position < 1e-6 m and velocity < 1e-9 m/s after normalization |
| `test_horizons_parser.cpp` | C++ | HORIZONS SI unit conversion |
| `test_system_writer.cpp` | C++ | JSON system file round-trip |
| `test_golden.cpp` | C++ | Pins integrator output (RK4, Leapfrog, Yoshida4, Hermite, RK4+GR, RK45) and CSV export against `tests/golden/` |
| `test_cli.cpp` | C++ | CLI exit codes, output file generation, help text |
| `test_python_api.py` | Python | Import, simulate, NumPy shapes, validation errors; every integrator cross-checked against the C++ golden values |

Shared C++ helpers (`CHECK`/`CHECK_NEAR`, `makeSunEarth`, `maxEnergyDrift`, `TempDir`) live in
`test_helpers.h`. New C++ tests are registered with `add_orbit_test(...)` in `CMakeLists.txt`.

---

## Golden tests

`test_golden` is the safety net for refactors that must not change results.

```bash
build/bin/test_golden                  # compare against tests/golden/ (what ctest runs)
GOLDEN_STRICT=1 build/bin/test_golden  # bit-identical values / byte-identical CSV text
build/bin/test_golden --record         # regenerate tests/golden/ (run from the repo root)
```

Run with `GOLDEN_STRICT=1` while refactoring numerics; CI uses the default tolerances (1e-9
relative) so it stays portable across compilers. Only `--record` when a change is *meant* to
alter results, and say so in the PR.

---

## Accuracy thresholds

| Quantity | Threshold | Integrator |
|----------|-----------|------------|
| Relative energy drift `\|dE/E₀\|` | < 1e-5 | RK4, 10k steps, dt=60s |
| Relative energy drift `\|dE/E₀\|` | < 1e-7 | RK4, 30 days Sun-Earth, dt=60s |
| COM position after normalization | < 1e-6 m | — |
| COM velocity after normalization | < 1e-9 m/s | — |

See `CONTRIBUTING.md` for the full policy on accuracy thresholds in PRs.
