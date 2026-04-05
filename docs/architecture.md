# Architecture and Codebase Design Guidelines

*Practical rules for keeping the orbital mechanics engine clean, extensible,
and reviewable as the project grows.*

**Author**: Sinan Can Demir  
**Last Updated**: April 2026

---

## Why this document exists

This repository has grown beyond a single-file or single-feature student
project. At this stage, code quality depends less on whether individual
functions work and more on whether the structure stays coherent as new features
arrive.

This document is a working reference for:

1. where code should live
2. what each module is allowed to depend on
3. how to add features without coupling everything together
4. what "good structure" means for this codebase

The goal is not abstract architecture purity. The goal is to make future work
faster, safer, and easier to review.

---

## Design Principles

These are the rules to preserve as the project evolves.

### 1. Keep the simulation core independent

The physics engine should not depend on:
- CLI parsing
- file naming conventions
- terminal printing
- OpenGL viewer logic
- Python binding details

If the core can run entirely in memory with no user interface attached, the
architecture is in good shape.

### 2. Separate computation from I/O

A function that computes physics should not also decide:
- where output files go
- how CSV columns are formatted
- what to print to stdout

Good pattern:
- core computes results
- exporter writes results
- CLI decides paths and user messages

### 3. Prefer stable data models over ad hoc parameter lists

When a function starts accumulating many unrelated parameters, replace them with
an options or result struct.

Good examples:
- `SimulationOptions`
- `SimulationResult`
- `HorizonsState`

This improves readability and makes Python bindings easier later.

### 4. Dependency direction must stay one-way

Higher-level layers may depend on lower-level layers.
Lower-level layers must not depend on higher-level layers.

Allowed direction:

```text
cli / viewer / python
        ↓
      io layer
        ↓
   simulation core
        ↓
  math + small utilities
```

Forbidden direction:
- `src/core` including viewer headers
- `src/core` depending on CLI argument parsing
- `src/core` writing user-facing help text

### 5. A new feature should fit into an existing boundary

When adding a feature, ask:

1. Is this core physics?
2. Is this input/output?
3. Is this user interface?
4. Is this visualization?
5. Is this analysis tooling?

If the answer is mixed, the feature probably needs to be split into smaller
parts.

---

## Current High-Level Structure

The repository already has a useful separation:

```text
include/      public headers
src/core/     physics and simulation logic
src/io/       JSON, HORIZONS, validation, system writing
src/cli/      command-line entry points and argument handling
src/viewer/   OpenGL visualization
python/       analysis and plotting scripts
tests/        C++ tests
docs/         roadmap, methods, validation, architecture notes
systems/      input system definitions
```

This is a solid foundation. The next step is to enforce clearer rules within
that structure.

---

## Target Layering Rules

Use the following mental model when placing code.

## Layer 1: Foundations

Purpose:
- small value types
- math helpers
- constants
- low-level utilities that are not domain workflows

Examples:
- `vec3`
- physical constants
- small helper functions with no side effects

Should not know about:
- JSON
- HORIZONS
- CLI flags
- OpenGL
- notebooks

## Layer 2: Core Simulation

Purpose:
- body state
- force calculations
- integrators
- conservation diagnostics
- eclipse calculations if treated as a simulation diagnostic

Examples:
- `simulation.cpp`
- `conservations.cpp`
- `barycenter.cpp`
- `eclipse.cpp`

Should depend only on:
- foundations
- data structures needed to describe simulation input/output

Should not:
- open files
- decide output path names
- print progress to terminal
- parse command-line options

## Layer 3: Data and Service Layer

Purpose:
- load systems from JSON
- parse HORIZONS outputs
- validate configuration files
- write systems or exported results

Examples:
- `json_loader.cpp`
- `horizons_parser.cpp`
- `system_writer.cpp`
- `validate.cpp`

This layer translates between external formats and internal data models.

## Layer 4: Interfaces

Purpose:
- CLI
- viewer
- future Python bindings

Examples:
- `src/cli/*`
- `src/viewer/*`
- future `src/python/*`

This layer is allowed to:
- parse arguments
- open windows
- print help text
- choose output file names
- convert exceptions into user-facing errors

This layer should not implement physics rules directly.

---

## File Placement Guidelines

Use these rules when deciding where new code belongs.

### Put code in `src/core/` if it:
- advances the simulation state
- computes a physical quantity
- implements an integrator
- computes diagnostics from body states

### Put code in `src/io/` if it:
- reads or writes JSON, CSV, or text files
- parses HORIZONS responses
- validates external input
- serializes simulation output

### Put code in `src/cli/` if it:
- parses argv
- dispatches commands
- prints usage/help/errors for command-line users

### Put code in `src/viewer/` if it:
- loads rendered trajectory data
- builds meshes
- manages camera/input/render state

### Put code in `python/` if it:
- is pure Python analysis or plotting support
- is notebook helper logic
- is not required to compile the C++ engine

### Put code in `tests/` if it:
- verifies behavior
- reproduces a bug
- locks down scientific or interface assumptions

---

## Public Header Rules

Headers in `include/` should define stable interfaces, not leak implementation
details unnecessarily.

### Good header habits

1. include only what is needed
2. prefer forward declarations when practical
3. expose data models and function declarations, not unrelated helpers
4. keep headers focused on one module responsibility

### Bad header habits

1. including heavy headers everywhere by default
2. mixing unrelated APIs in one header
3. putting too much implementation in headers without need
4. exposing viewer or CLI details through core headers

### Naming guideline

Prefer names that describe role, not temporary implementation history.

Good:
- `simulation_result.h`
- `simulation_options.h`
- `csv_exporter.h`
- `integrators.h`

Less good:
- `utils.h` for unrelated domain logic
- `main.h` for shared project-wide declarations

If a file name becomes vague, split or rename it.

---

## Core API Guidelines

The most important architectural improvement for this codebase is to make the
core API usable without the CLI.

### Preferred pattern

```cpp
SimulationOptions options;
SimulationResult result = runSimulation(bodies, options);
writeTrajectoryCSV(result, path);
```

### Avoid this pattern in new code

```cpp
runSimulation(bodies, steps, dt, outputPath, integrator, stride);
```

Why:
- it mixes simulation with export concerns
- it makes Python bindings awkward
- it makes testing harder
- it encourages every new feature to add another flag to one giant function

### Recommended core objects

As the project matures, prefer introducing explicit types such as:

1. `SimulationOptions`
2. `SimulationResult`
3. `IntegratorConfig`
4. `ExportOptions`

This gives the codebase a stable vocabulary.

---

## Data Ownership Guidelines

When results are shared across modules, ownership must be obvious.

### Preferred rule

The code that creates a result object owns it until it hands it to another
layer explicitly.

Example:
- core creates `SimulationResult`
- CLI may export it
- Python bindings may expose views into it

### Important implication for Python

If Python is going to receive NumPy views later, result buffers should be:
- contiguous
- stable in memory
- owned by a dedicated result object

That means flattened arrays are often better than nested vectors for
Python-facing output.

---

## Directory-Level Guidelines for Future Growth

If the project grows, use this target organization.

```text
include/
  core/
  io/
  viewer/
  python/

src/
  core/
  io/
  cli/
  viewer/
  python/
```

You do not need to do this reorganization immediately, but keep it in mind if
the root `include/` directory becomes crowded.

### Suggested future additions

If Phase 3 lands:

```text
include/core/simulation_options.h
include/core/simulation_result.h
include/io/csv_exporter.h
src/python/module.cpp
src/python/py_simulation.cpp
src/python/py_result.cpp
```

If the number of integrators grows:

```text
include/core/integrators.h
src/core/integrators/
  rk4.cpp
  leapfrog.cpp
  rk45.cpp
```

If diagnostics grow:

```text
include/core/diagnostics.h
src/core/diagnostics/
  conservation.cpp
  eclipse.cpp
  residuals.cpp
```

The point is not to over-engineer now. The point is to have a direction ready
before the current files become overloaded.

---

## Adding a New Feature: Decision Checklist

Before writing code, answer these questions:

### 1. What layer does this belong to?

Pick one primary layer:
- core
- io
- cli
- viewer
- python

If the answer is two or more layers, split the task.

### 2. What is the public interface?

Define:
- input type
- output type
- ownership
- failure mode

If you cannot state those clearly, the design is not ready.

### 3. What should be tested?

Every nontrivial feature should add at least one of:
- unit test
- regression test
- validation artifact

### 4. What documentation must change?

Usually one of:
- README
- CLI reference
- roadmap
- methods or validation docs

### 5. Does this increase coupling?

Warning signs:
- core now needs to know about paths or terminal output
- one feature requires touching many unrelated files
- new code reuses a vague helper because there is no clean abstraction

If coupling increases, stop and refactor before continuing.

---

## Code Smells to Watch For

These are the main structural warning signs in this repository.

### 1. "God functions"

A single function that:
- loads data
- runs physics
- writes files
- prints logs
- handles special cases

This is the clearest sign a boundary needs to be split.

### 2. "God headers"

Headers that become dumping grounds for unrelated declarations.

Examples to watch:
- overly broad utility headers
- shared headers with no single responsibility

### 3. Feature flags that leak across layers

If a new CLI flag forces changes deep into unrelated modules, the interface is
probably too weak or too entangled.

### 4. Duplicate logic in CLI and Python tools

If Python scripts reimplement logic the C++ core already knows, move the logic
into a shared layer instead.

### 5. Vague names

Names like `utils`, `main`, `helper`, `manager`, or `processor` are acceptable
only when the scope is genuinely small and obvious.

If a file becomes important, rename it to reflect what it actually owns.

---

## Testing Structure Guidelines

A better structure is not only about production code. Tests should mirror the
architecture.

### Test categories to aim for

#### Core physics tests
- force symmetry
- integrator behavior
- conservation properties
- barycenter normalization

#### I/O tests
- JSON load/validation
- HORIZONS parse behavior
- system writer round-trip

#### Interface tests
- CLI smoke tests
- future Python API tests

#### Validation tests
- known orbit behavior
- HORIZONS residual benchmarks

### Testing rule

When a bug is fixed, add the narrowest possible test that would have caught it.

That is how the test suite becomes a memory of the project instead of a demo.

---

## Documentation Structure Guidelines

Docs should also have roles.

### Recommended split

- `README.md`
  brief install, build, run, quick examples

- `docs/physics-and-methods.md`
  scientific and numerical explanation

- `docs/architecture.md`
  structure rules and module boundaries

- `docs/validation.md`
  evidence that the numerics behave as claimed

- `docs/milestones/*.md`
  implementation plans for future changes

### Documentation rule

Do not let roadmap documents become the only place where architecture decisions
exist. Once a design choice becomes permanent, move it into stable docs.

---

## Recommended Refactors Over Time

These are the highest-value structural improvements for this codebase.

### Near-term

1. introduce `SimulationOptions`
2. introduce `SimulationResult`
3. split file export out of the simulation loop
4. reduce reliance on vague shared headers
5. make tests run through `ctest`

### Mid-term

1. move integrators behind cleaner interfaces
2. separate diagnostics from stepping logic more clearly
3. add Python bindings on top of the refactored core
4. add CSV/export helpers as explicit I/O modules

### Long-term

1. organize headers by subsystem
2. standardize error handling strategy
3. introduce more explicit API boundaries for library use

---

## A Simple Standard for "Good Structure"

When evaluating a change, use this standard:

The codebase structure is improving if:

1. the core becomes easier to reuse
2. the interfaces become easier to explain
3. a feature can be tested without spinning up unrelated systems
4. the dependency graph becomes clearer, not blurrier
5. new contributors can guess where code belongs

The codebase structure is getting worse if:

1. more logic collects in a few large files
2. new features require edits across unrelated subsystems
3. file I/O and physics keep blending together
4. naming becomes less precise
5. docs describe a cleaner structure than the code actually has

---

## Final Guideline

Do not chase "enterprise architecture." Chase clean boundaries.

For this project, a strong codebase is one where:
- the simulation core can be reused by CLI, viewer, and Python
- I/O is explicit and isolated
- tests mirror real responsibilities
- file names and headers describe what they actually own
- new features make the design clearer instead of noisier

That is the standard worth building toward.
