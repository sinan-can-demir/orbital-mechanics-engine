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
| `test_two_body.cpp` | C++ | Circular orbit period matches Kepler's 3rd law |
| `test_leapfrog.cpp` | C++ | Symplectic energy drift bounded and non-monotonic |
| `test_barycenter.cpp` | C++ | COM position and velocity < 1e-10 after normalization |
| `test_horizons_parser.cpp` | C++ | HORIZONS SI unit conversion |
| `test_system_writer.cpp` | C++ | JSON system file round-trip |
| `test_cli.cpp` | C++ | CLI exit codes and output file generation |
| `test_python_api.py` | Python | Import, simulate, NumPy shapes, energy conservation |

---

## Accuracy thresholds

| Quantity | Threshold | Integrator |
|----------|-----------|------------|
| Relative energy drift `\|dE/E₀\|` | < 1e-5 | RK4, 10k steps, dt=60s |
| Relative energy drift `\|dE/E₀\|` | < 1e-10 | RK45, 1-year Earth-Moon |
| COM position after normalization | < 1e-10 m | — |
| COM velocity after normalization | < 1e-10 m/s | — |

See `CONTRIBUTING.md` for the full policy on accuracy thresholds in PRs.
