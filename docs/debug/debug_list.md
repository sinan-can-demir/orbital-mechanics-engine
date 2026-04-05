# Architectural Debug List

*Repo-specific structural issues identified by reviewing the codebase against
`docs/architecture.md`. This is not a list of random cleanup tasks. It is a
prioritized guide for reducing coupling and making the engine easier to extend,
test, and bind from Python.*

**Last Updated**: April 2026

---

## How to use this list

Do not try to fix everything at once.

Use this order:

1. fix boundary problems that block future work
2. fix file/module ownership problems
3. fix naming and header hygiene
4. ignore cosmetic cleanup until the architecture is calmer

If a task feels stressful, that usually means it is too large. Split it into:

1. identify the boundary
2. extract one piece
3. verify behavior still works

---

## Priority 0 — Highest-Leverage Refactors

These are the changes that unlock cleaner Phase 2 and Phase 3 work.

### 1. `runSimulation()` is doing too many jobs

**Files:**
- `include/simulation.h`
- `src/core/simulation.cpp`

**Problem:**
`runSimulation()` currently:
- advances the integrator
- computes diagnostics
- derives output paths
- creates directories
- opens files
- writes trajectory CSV
- writes conservation CSV
- writes eclipse logs
- prints user-facing status messages

This is the clearest "god function" in the repo.

**Why it matters:**
- blocks a clean Python API
- makes testing harder
- couples core physics to file output and CLI behavior
- makes every future feature want to add one more parameter

**Target refactor:**
Split into at least three responsibilities:

1. simulation runner
2. result object
3. export helpers

Suggested direction:

```cpp
SimulationResult result = runSimulation(bodies, options);
writeTrajectoryCSV(result, path);
writeConservationCSV(result, path);
writeEclipseCSV(result, path);
```

**Definition of done:**
- core simulation can run fully in memory
- CSV writing is optional
- CLI still works unchanged from the user’s perspective

---

### 2. Core simulation API is file-oriented instead of result-oriented

**Files:**
- `include/simulation.h`
- `src/core/simulation.cpp`

**Problem:**
The public API is centered around:

```cpp
runSimulation(..., outputPath, integrator, stride)
```

That is a CLI/export shape, not a reusable library shape.

**Why it matters:**
- weak foundation for pybind11
- awkward for tests
- hard to expose result arrays to Python

**Target refactor:**
Introduce explicit data models:

1. `SimulationOptions`
2. `SimulationResult`

**Definition of done:**
- `runSimulation` or equivalent returns a structured result
- output writing happens elsewhere

---

### 3. CLI main still owns business logic that should become reusable services

**Files:**
- `src/cli/main.cpp`

**Problem:**
`main.cpp` is not just dispatching commands. It also:
- parses body ID CSVs
- resolves default values
- resolves integrator strings
- assembles `BuildSystemOptions`
- decides post-build run behavior
- builds default output paths

Some of that is normal for CLI. Some of it is application/service logic that
should live outside `main()`.

**Why it matters:**
- hard to test non-CLI behavior without going through `main`
- increases duplication risk when Python arrives

**Target refactor:**
Keep `main()` as a thin adapter:

1. parse arguments
2. call service functions
3. print/report errors

Move reusable logic into helpers or service-layer functions.

**Definition of done:**
- `main()` is mostly command dispatch and error handling
- integrator parsing and run configuration logic can be tested independently

---

## Priority 1 — Module Boundary Problems

These are structural issues that violate the intended layer boundaries.

### 4. `validateSystemFile()` mixes validation with printing

**Files:**
- `include/validate.h`
- `src/io/validate.cpp`

**Problem:**
The validator:
- loads the system
- decides validity
- prints a report directly to stdout/stderr

That makes it hard to reuse in tests, Python, or future GUI flows.

**Why it matters:**
Validation should produce data. Interfaces should decide how to display it.

**Target refactor:**
Create something like:

```cpp
ValidationReport validateSystem(const std::string& path);
```

Then:
- CLI prints the report
- tests assert on report fields

**Definition of done:**
- validation logic is reusable without terminal output
- CLI output is a wrapper around report data

---

### 5. JSON loading and validation are too loosely separated

**Files:**
- `include/json_loader.h`
- `src/core/json_loader.cpp`
- `src/io/validate.cpp`

**Problem:**
`loadSystemFromJSON()` reads raw JSON and constructs bodies directly.
Validation logic mostly happens later or indirectly.

**Why it matters:**
- malformed input is caught late
- loader and validator responsibilities are blurred
- Python/API work will want stronger error contracts

**Target refactor:**
Choose one clear approach:

1. strict loader that validates while loading and throws structured errors
2. separate parse + validate stages with explicit report/result types

Do not keep the current halfway model.

**Definition of done:**
- invalid files fail clearly and predictably
- load and validate responsibilities are explicit

---

### 6. Viewer CSV loader is still hardcoded to one scenario

**Files:**
- `src/viewer/csv_loader.cpp`

**Problem:**
The loader still assumes:
- Sun
- Earth
- Moon
- a fixed CSV shape
- Moon exaggeration policy inside the loader

This conflicts with the repo’s generic N-body direction.

**Why it matters:**
- viewer layer is still partially tied to old project assumptions
- loader owns visualization policy instead of just loading data

**Target refactor:**
Split responsibilities:

1. generic trajectory loader
2. viewer-side transform/exaggeration policy

**Definition of done:**
- loader handles arbitrary body lists
- exaggeration or frame transforms are not embedded in file parsing

---

### 7. `main.h` is a catch-all header

**Files:**
- `include/main.h`

**Problem:**
`main.h` includes almost everything:
- barycenter
- body
- cli
- horizons
- json loader
- ray
- simulation
- utils
- validate
- vec3

This is classic "god header" behavior.

**Why it matters:**
- weakens module boundaries
- increases compile coupling
- hides actual dependencies

**Target refactor:**
Delete `main.h` as a shared include hub.

Have each `.cpp` include only what it directly needs.

**Definition of done:**
- `src/cli/main.cpp` includes its own real dependencies directly
- no catch-all project header exists

---

## Priority 2 — API and Naming Hygiene

These are not as urgent as the items above, but they matter for long-term
clarity.

### 8. `utils.h` is too vague for a core public header

**Files:**
- `include/utils.h`

**Problem:**
`utils.h` currently stores physical constants and orbital constants. The name is
too broad for something that is effectively part of the scientific core.

**Why it matters:**
Vague names attract unrelated helpers over time.

**Target refactor:**
Rename or split into more explicit headers, for example:

1. `physical_constants.h`
2. `orbital_constants.h`

**Definition of done:**
- no important domain concepts live in vaguely named utility buckets

---

### 9. `CLIOptions` is becoming an everything-struct

**Files:**
- `include/cli.h`
- `src/cli/cli.cpp`

**Problem:**
One struct currently carries fields for:
- run
- fetch
- build-system
- shared flags

That is manageable now, but it will get worse as new commands and Python-facing
concepts appear.

**Why it matters:**
- encourages accidental command coupling
- weakens validation
- makes parser behavior harder to reason about

**Target refactor:**
Keep the parser simple, but move toward command-specific config types:

1. `RunCommandOptions`
2. `FetchCommandOptions`
3. `BuildSystemOptions`

The top-level parse result can still use a tagged command type.

**Definition of done:**
- command-specific fields do not all live in one bag forever

---

### 10. Integrator parsing is duplicated as ad hoc string logic

**Files:**
- `src/cli/main.cpp`

**Problem:**
Integrator string resolution happens inline in the CLI run path.

**Why it matters:**
- logic will be duplicated for Python later
- error behavior will drift across interfaces

**Target refactor:**
Centralize:

```cpp
Integrator parseIntegratorName(const std::string&);
std::string integratorName(Integrator);
```

**Definition of done:**
- string-to-enum behavior lives in one place

---

### 11. Some defaults still leak old project history

**Files:**
- `src/cli/main.cpp`

**Problem:**
Default output path:

```cpp
build/orbit_three_body.csv
```

This is a leftover from an earlier project phase and no longer matches the
generic N-body direction.

**Why it matters:**
Naming affects how reusable and intentional the codebase feels.

**Target refactor:**
Use a neutral default such as:
- `build/orbit_output.csv`
- derived from system name

**Definition of done:**
- defaults do not imply obsolete scope

---

## Priority 3 — Error and Ownership Boundaries

These are less visible than feature work, but they matter for a stable codebase.

### 12. I/O and service code use terminal printing as part of normal control flow

**Files:**
- `src/io/validate.cpp`
- `src/io/horizons_builder.cpp`
- `src/core/simulation.cpp`

**Problem:**
Several non-UI layers print directly to stdout/stderr as part of normal
operation.

**Why it matters:**
- makes reuse noisy
- complicates tests
- makes future Python bindings awkward

**Target refactor:**
Use one of:

1. returned reports/results
2. exceptions for failures
3. optional logger callback if needed

Reserve terminal output for CLI/viewer layers.

**Definition of done:**
- lower layers are usable without forcing console output

---

### 13. Temporary file handling in HORIZONS build flow is operationally correct but architecturally brittle

**Files:**
- `src/io/horizons_builder.cpp`

**Problem:**
The builder fetches raw data to `/tmp/orb_fetch_<id>.txt`, reparses it, then
deletes it.

**Why it matters:**
- workflow is file-system mediated when it could be in-memory
- temp file behavior becomes an implementation constraint
- failures can leave partial state around

**Target refactor:**
Long-term direction:

1. fetch raw response in memory
2. parse from string or stream
3. write only the final requested artifact

This is not urgent, but it is a good cleanup target later.

**Definition of done:**
- internal fetch/build flow does not require temp files unless explicitly requested

---

### 14. Validation and load errors should become more structured before Python bindings

**Files:**
- `src/core/json_loader.cpp`
- `src/io/validate.cpp`
- `src/cli/main.cpp`

**Problem:**
Errors are mostly:
- `runtime_error`
- printed warnings
- boolean success/failure

That is enough for CLI, but weak for library use.

**Why it matters:**
Python bindings will need consistent exception mapping and better failure
semantics.

**Target refactor:**
Standardize failure behavior for:

1. invalid config
2. invalid integrator
3. bad Horizons data
4. export failures

**Definition of done:**
- error behavior is predictable and interface-friendly

---

## Priority 4 — Nice-to-Have Structural Cleanup

These should not be allowed to distract from the higher-priority refactors.

### 15. Header dependency cleanup

**Files:**
- especially `include/cli.h`
- `include/validate.h`
- `include/main.h`

**Problem:**
Some headers include more than they need. For example, `cli.h` pulls in
`simulation.h` even though the CLI option struct should not need the full
simulation API just to exist.

**Target refactor:**
Minimize header includes and keep interfaces narrow.

**Definition of done:**
- headers include only what they directly require

---

### 16. Distinguish stable docs from milestone plans

**Files:**
- `docs/architecture.md`
- `docs/milestones/*.md`

**Problem:**
Some architectural direction still lives mostly in milestone docs.

**Why it matters:**
Once a design decision becomes permanent, it should move into stable docs.

**Target refactor:**
Keep using milestone docs for planning, but promote settled design rules into:
- `docs/architecture.md`
- `docs/validation.md`
- README where appropriate

---

## Suggested Fix Order

Do these in this order if you want the least painful path:

### Pass 1

1. remove `main.h`
2. extract integrator parsing helper
3. rename old defaults like `orbit_three_body.csv`
4. reduce header over-includes

These are small wins and will lower the stress level.

### Pass 2

1. split validation from validation-printing
2. decide strict loader vs parse+validate model
3. start moving viewer CSV loading toward generic behavior

These strengthen boundaries without a massive rewrite.

### Pass 3

1. introduce `SimulationOptions`
2. introduce `SimulationResult`
3. split export logic out of `runSimulation()`

This is the biggest architectural step and the one that most helps Python.

### Pass 4

1. reduce console printing in lower layers
2. improve HORIZONS build flow
3. standardize structured error behavior

This is polish on top of the stronger architecture.

---

## Final Note

This is not evidence that the codebase is bad. It is evidence that the project
has grown enough to need stronger boundaries.

The main thing to remember:

You do not need to "fix the architecture."
You need to remove the few structural pressure points that keep causing new
features to land in the wrong place.

Start with the simulation/export boundary. That is the most important one.
