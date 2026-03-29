# Tests

This directory contains validation tests for the orbital mechanics engine.

## Planned test suites

### Conservation law tests (`tests/core/`)
Verify that the RK4 and Leapfrog integrators preserve energy, linear momentum,
and angular momentum within acceptable bounds.

Target: `|dE/E₀|` < 1e-5 over 10,000 steps at dt=60s for the Earth–Moon system.

### Regression tests (`tests/regression/`)
Store known-good CSV outputs and assert that simulation results match them
within a tight tolerance after any code change.

### Integration tests (`tests/integration/`)
End-to-end tests: load a JSON system, run the CLI, parse the CSV output,
verify physical quantities.

## Running tests

Tests will be driven by CMake's CTest once implemented:

```bash
cd build
cmake ..
ctest --output-on-failure
```

See `CONTRIBUTING.md` for the physics accuracy thresholds required before
any PR can be merged.
