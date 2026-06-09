# Refactoring Guidelines

*Practical rules for restructuring this codebase without breaking it.*

**Author**: Sinan Can Demir
**Project**: Orbital Mechanics Engine
**Last Updated**: April 2026

---

## The Core Rule

> Never change behavior and structure at the same time.

A refactor changes *where* code lives. It must not change *what* it does.
The moment you add a feature while refactoring, you lose your safety net —
you can't tell if a bug came from the restructure or the new feature.

---

## 1. Read Before You Touch Anything

Before refactoring a function, read every caller of that function.

For `runSimulation()`, that means tracing through the CLI before touching the
signature. Ask:

- What does the caller pass in?
- What does it expect back?
- What side effects does the caller depend on? (files written, console output, return codes)

You cannot safely change a function you haven't traced through its callers.

---

## 2. Find the Seam

A **seam** is a natural boundary in code where you can insert a new interface
without changing behavior. Look for where distinct concerns are tangled together
and identify the line between them.

Example — `runSimulation()` does three distinct things:

```
runSimulation()
├── opens files                      ← I/O concern
├── writes CSV headers               ← I/O concern
├── for each step:
│   ├── rk4Step() / leapfrogStep()   ← physics concern  ✓ already clean
│   ├── computes conservation        ← physics concern  ✓ already clean
│   ├── writes positions to CSV      ← I/O concern
│   └── writes conservation to CSV   ← I/O concern
└── closes files                     ← I/O concern
```

The seam is between the physics loop and the file writing. The physics is
already clean. The refactor's job is to pull them apart at that boundary.

---

## 3. The Strangler Fig Pattern

Don't delete the old function. Grow the new one alongside it, then make the
old one call the new one.

**Wrong approach — rewrite from scratch:**
```cpp
// High risk. Breaks the CLI. Hard to compare against known-good behavior.
void runSimulation(...) {
    // completely new code
}
```

**Right approach — introduce alongside:**
```cpp
// Step 1: new function handles only physics
SimulationResult simulate(bodies, options);

// Step 2: old function becomes a wrapper — CLI still works
void runSimulation(...) {
    auto result = simulate(bodies, options);
    exportToCSV(result, outputPath);
}
```

The CLI never breaks. The new function can be tested independently. The wrapper
can be deleted later once nothing depends on the old interface.

---

## 4. The Extraction Sequence

Do this in order. Each session should be small enough to complete in 45–90 minutes.

### Session 1 — Extract repeated logic into a helper

This conservation block appears almost identically in both `runSimulation()`
and `runSimulationAdaptive()`:

```cpp
physics::Conservations C = physics::compute(bodies);
double Lmag = std::sqrt(C.L[0]*C.L[0] + C.L[1]*C.L[1] + C.L[2]*C.L[2]);
double Pmag = ...
double dE   = (C.total_energy - E0) / std::abs(E0);
double dL   = ...
double dP   = ...
```

Extract it into one function: `ConservationSnapshot computeSnapshot(bodies, C0)`.
Compile. Run tests. Stop.

### Session 2 — Introduce the new types, connect nothing

```cpp
struct SimulationSnapshot {
    int    step;
    double time_s;
    std::vector<vec3> positions;
    ConservationSnapshot conservation;
};

struct SimulationResult {
    std::vector<std::string>         body_names;
    std::vector<SimulationSnapshot>  snapshots;
};
```

Don't connect these to anything yet. Just make them compile and exist.

### Session 3 — Make the loop fill `SimulationResult`

Change the loop inside `runSimulation()` to also push into a result vector,
alongside the existing file writes. At this point both behaviors run in
parallel — old and new — so you can compare outputs.

Do not remove any existing file writes in this session.

### Session 4 — Extract CSV writing into a standalone function

Move all `file <<` lines into:

```cpp
void exportCSV(const SimulationResult& result, const std::string& outputPath);
```

Now `runSimulation()` becomes:

```cpp
SimulationResult result = runSimulationCore(bodies, steps, dt, integrator, stride);
exportCSV(result, outputPath);
```

### Session 5 — Verify the CLI is identical

Run the existing tests. Diff the CSV output against a known-good run stored
in `results/`. If they match, the refactor is complete for this boundary.

---

## 5. Compile After Every Meaningful Change

Before running the test suite, just check it compiles. If you make 10 changes
before compiling, you can't localize the error. One change → compile → next
change.

---

## 6. Clean Git State Before Each Session

Your working tree must be clean before starting a refactor session.

```bash
git stash    # or commit what you have
```

You want `git diff` to show exactly what this session changed — nothing else.
If you mix refactoring with uncommitted feature work, the diff becomes
unreadable and rollback becomes painful.

---

## 7. Never Combine a Refactor with a Feature

If you find a bug while refactoring, note it and fix it in a separate commit.
If you want to add a feature while refactoring, stop and do the refactor first.

One commit = one kind of change.

---

## Session Checklist

Before opening any file:
- [ ] Git working tree is clean
- [ ] You have identified the one target for this session
- [ ] You know what the observable behavior should remain after the change

During the session:
- [ ] Compile after each meaningful change
- [ ] Do not combine refactoring with feature work
- [ ] Stop if you find yourself touching unrelated files

After the session:
- [ ] Tests still pass
- [ ] Write one sentence about what you changed
- [ ] Write one sentence about the next smallest step

---

## When to Stop a Session

Stop if:
- You are touching files unrelated to the original target
- You forgot what the goal was
- You are about to mix a refactor with a new feature
- You are guessing instead of making one clear structural change

When that happens: commit what compiles cleanly, write a note, and stop.
That is not failure. That is good engineering pacing.

---

## Priority Order for This Codebase

The highest-value cleanups for the JOSS submission, in order:

1. **Simulation vs. export separation** — physics loop must not write files directly
2. **Validation vs. printing separation** — validation logic must not `cout` to terminal
3. **CLI vs. service-logic separation** — `main.cpp` should dispatch, not implement
4. **Generic viewer loading** — viewer loader must not assume Sun/Earth/Moon

Do these gradually. Each one directly unblocks a downstream task.

---

## References

- `docs/milestones/ROADMAP_refactor_sessions.md` — session-by-session task list
- `docs/milestones/ROADMAP_python_binding_architecture.md` — why the simulation/export
  separation is required before Python bindings
- `docs/ROADMAP.md` — overall project status and JOSS checklist
