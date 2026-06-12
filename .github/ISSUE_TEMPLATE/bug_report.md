---
name: Bug report
about: Simulation gives wrong results, crashes, or behaves unexpectedly
title: "[BUG] "
labels: bug
assignees: ''
---

## Description
A clear description of what went wrong.

## System file / setup
```json
// paste your system JSON or describe the setup (e.g. "solar system, 10 bodies")
```

## Command or code used
```bash
# CLI
orbit-sim run --system ... --steps ... --dt ... --integrator ...

# or Python
import orbit
result = orbit.simulate(...)
```

## Expected behaviour
What you expected to happen.

## Actual behaviour
What actually happened. Include error messages or unexpected output.

## Environment
- OS:
- orbit version (`orbit.__version__`):
- Python version:
- How installed (`pip install -e .` / release wheel):
