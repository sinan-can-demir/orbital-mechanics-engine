# Roadmap — Low-Stress Refactor Sessions

*A practical checklist for architectural cleanup without turning it into a
giant rewrite.*

**Author**: Sinan Can Demir  
**Last Updated**: April 2026

---

## How to use this roadmap

This is not a sprint plan.
This is a calm, repeatable rhythm:

- work `45–90 minutes` at a time
- fix one function, one header, or one boundary per session
- stop when the code still compiles or the change is clearly isolated
- do not combine refactoring with a major new feature in the same session

The goal is not speed.
The goal is to reduce confusion over time.

---

## Session Rules

- [ ] Pick one small target before opening files
- [ ] Write down what boundary feels wrong
- [ ] Make one focused change only
- [ ] Run one quick verification step
- [ ] Leave a short note for your next session
- [ ] Stop before the refactor spreads into unrelated subsystems

Good session targets:
- one helper extraction
- one header cleanup
- one naming cleanup
- one piece of file I/O moved out of core
- one command-specific parsing helper

Bad session targets:
- rewrite the whole simulation engine
- redesign all architecture at once
- add Python bindings while refactoring core
- fix every viewer issue in one sitting

---

## Phase 1 — Small Wins First

*Goal: reduce noise and lower the mental load before touching deeper boundaries.*

- [ ] Remove `include/main.h` as a catch-all header
- [ ] Replace `main.h` usage with direct includes in `src/cli/main.cpp`
- [ ] Extract integrator string parsing into a helper function
- [ ] Rename the default output path away from `orbit_three_body.csv`
- [ ] Reduce unnecessary includes in `include/cli.h`
- [ ] Reduce unnecessary includes in `include/validate.h`
- [ ] Review `utils.h` and decide whether it should become `physical_constants.h`

**Definition of done:**
- you can open the CLI layer and see clearer dependencies immediately

---

## Phase 2 — Validation and Loader Boundaries

*Goal: make data-loading and validation behavior easier to reason about.*

- [ ] Decide whether JSON loading should be strict or separated into parse + validate
- [ ] Document that decision briefly in `docs/architecture.md` if it becomes stable
- [ ] Refactor `validateSystemFile()` so validation logic is separate from printing
- [ ] Introduce a reusable validation result/report type
- [ ] Make invalid JSON failures clearer and more structured
- [ ] Add or improve one test for invalid system input

**Definition of done:**
- validation can be reused without relying on terminal output

---

## Phase 3 — Viewer Cleanup

*Goal: remove old project-history assumptions from the viewer path.*

- [ ] Audit `src/viewer/csv_loader.cpp` for hardcoded Sun/Earth/Moon assumptions
- [ ] Move Moon exaggeration policy out of the CSV parsing path
- [ ] Separate generic data loading from visualization-specific transforms
- [ ] Make the loader behavior more obviously generic-N-body
- [ ] Add one quick verification run with a non-Earth-Moon dataset if available

**Definition of done:**
- viewer loading is less tied to one historical scenario

---

## Phase 4 — Service Logic Out of `main.cpp`

*Goal: make the CLI thinner and easier to test.*

- [ ] Move body-ID splitting helpers out of `main.cpp` if they are reusable
- [ ] Move integrator resolution out of inline CLI logic
- [ ] Reduce default-value decision logic inside `main.cpp`
- [ ] Keep `main()` focused on parse → dispatch → report errors
- [ ] Make one command path easier to test without going through all of `main`

**Definition of done:**
- `main.cpp` reads more like a dispatcher than an implementation file

---

## Phase 5 — Simulation API Cleanup

*Goal: prepare the core for Python bindings and cleaner testing.*

- [ ] Introduce `SimulationOptions`
- [ ] Introduce `SimulationResult`
- [ ] Identify the minimum data `SimulationResult` must own
- [ ] Move trajectory writing out of the core simulation loop
- [ ] Move conservation CSV writing out of the core simulation loop
- [ ] Move eclipse log writing out of the core simulation loop if practical
- [ ] Keep the current CLI behavior working through wrappers if needed

**Definition of done:**
- the simulation engine can run without being forced to write files

---

## Phase 6 — Lower-Layer Output Cleanup

*Goal: reduce direct printing from non-UI layers.*

- [ ] Identify which messages belong in CLI only
- [ ] Remove direct console printing from validation logic where possible
- [ ] Reduce direct console printing in service-layer helpers where practical
- [ ] Decide where exceptions are better than `bool + print`
- [ ] Keep only user-interface messaging in CLI/viewer layers

**Definition of done:**
- lower layers return data or errors instead of narrating to the terminal

---

## Phase 7 — Test-As-You-Refactor

*Goal: use tests to make cleanup safer, not perfect.*

- [ ] Add one regression test when a refactor fixes a real bug
- [ ] Add one narrow test around integrator parsing if extracted
- [ ] Add one narrow test around validation/report behavior
- [ ] Add one narrow test around any new simulation result API
- [ ] Prefer small trustworthy tests over big fragile ones

**Definition of done:**
- each meaningful refactor makes the codebase slightly safer to change again

---

## Weekly Rhythm Option

If you want a low-stress schedule:

### Week 1

- [ ] Do 2-3 small header / naming / include cleanups

### Week 2

- [ ] Refactor validation boundary a little
- [ ] Add one test around it

### Week 3

- [ ] Clean one viewer assumption
- [ ] Verify nothing obvious broke

### Week 4

- [ ] Start introducing `SimulationOptions` or a first result object draft

This is enough.
You do not need to do architectural cleanup full-time.

---

## Rule for Ending a Session

Stop the session if:

- you are touching unrelated files “just because”
- you forgot the original goal
- you are about to mix refactoring with a new feature
- you are guessing instead of making one clear structural improvement

When that happens:

- [ ] write one sentence about what you learned
- [ ] write one sentence about the next smallest step
- [ ] stop for the day

That is not failure.
That is good engineering pacing.

---

## Final Note

Refactoring is not something you “finish.”
You use it to remove the pressure points that make future work slower.

For this project, the highest-value cleanup is:

- [ ] simulation vs export separation
- [ ] validation vs printing separation
- [ ] CLI vs service-logic separation
- [ ] generic viewer loading

Do those gradually, and the codebase will get much easier to work with.
