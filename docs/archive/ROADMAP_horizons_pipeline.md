# Roadmap — NASA JPL HORIZONS → System JSON Pipeline

*Extending the existing HORIZONS fetch to automatically produce simulation-ready `systems/*.json` files from real ephemeris data.*

**Author**: Sinan Can Demir  
**Last Updated**: April 2026  
**GitHub**: https://github.com/sinan-can-demir/orbital-mechanics-engine

---

## Motivation

The engine already fetches real ephemeris data from NASA JPL HORIZONS
(`src/io/horizons.cpp`). However the pipeline stops at saving raw text — a human
still has to manually read the output and hand-write a `systems/*.json` file.

This roadmap closes that gap. The goal is a single CLI command that:

1. Fetches position and velocity vectors for any set of bodies from HORIZONS
2. Parses the raw text response automatically
3. Writes a valid `systems/*.json` that `loadSystemFromJSON()` can consume directly
4. Runs the simulation immediately if desired

**Before this roadmap:**
```bash
# Step 1 — fetch (already works)
orbit-sim fetch --body 399 --start 2025-01-01 --stop 2025-01-02 \
                --step "6 h" --output earth_raw.txt

# Step 2 — manually read earth_raw.txt and hand-write systems/earth.json
# (painful, error-prone, not reproducible)

# Step 3 — run
orbit-sim run --system systems/earth_moon.json
```

**After this roadmap:**
```bash
# One command does everything
orbit-sim build-system \
    --bodies 10,399,301 \
    --epoch "2025-01-01" \
    --output systems/earth_moon_real.json

# Optionally run immediately
orbit-sim build-system --bodies 10,399,301 --epoch "2025-01-01" \
                        --output systems/earth_moon_real.json --run \
                        --steps 876000 --dt 60
```

---

## Current State (Honest Baseline)

| Component | Status | Notes |
|-----------|--------|-------|
| `fetchHorizonsEphemeris()` GET mode | ✅ Working | Hits `ssd-api.jpl.nasa.gov/horizons_file.api` |
| `fetchHorizonsEphemerisPOST()` POST mode | ✅ Working | Hits `ssd.jpl.nasa.gov/api/horizons.api` |
| Raw text saved to file | ✅ Working | `"result"` field extracted from JSON wrapper |
| HORIZONS text parser | ❌ Missing | Nobody reads `$$SOE`/`$$EOE` markers |
| Mass lookup table | ❌ Missing | HORIZONS gives positions/velocities, not mass |
| JSON system writer | ❌ Missing | No code writes `systems/*.json` programmatically |
| `build-system` CLI command | ❌ Missing | Does not exist yet |
| Multi-body fetch in one command | ❌ Missing | Current fetch handles one body at a time |

**Known constraints going into this work:**
- `EPHEM_TYPE=VECTORS` is already hardcoded in both fetch modes — correct for our use case
- `loadSystemFromJSON()` expects exactly: `name`, `mass`, `position [x,y,z]`, `velocity [vx,vy,vz]`
- HORIZONS returns positions in **km** and velocities in **km/s** — must convert to **meters** and **m/s**
- HORIZONS does not return body mass — requires a hardcoded lookup table for known bodies
- The `$$SOE` / `$$EOE` markers delimit the actual data block in HORIZONS text output
- Each body requires a **separate** HORIZONS API call — they cannot be batched in one request

---

## What HORIZONS Actually Returns

Understanding the raw text format is essential before writing any parser.

A typical HORIZONS vector table response looks like this:

```
*******************************************************************************
Ephemeris / WWW_USER ...
*******************************************************************************
...
$$SOE
2460676.500000000 = A.D. 2025-Jan-01 00:00:00.0000 TDB
 X =-2.627653140025671E+07 Y = 1.444218499425369E+08 Z = 1.898104419804503E+04
 VX=-2.981142022421017E+01 VY=-5.399982640548080E+00 VZ= 9.016363228819699E-04
 LT= 4.944735147427052E+02 RG= 1.482209034698547E+08 RR=-3.019720994543136E-01
2460677.500000000 = A.D. 2025-Jan-02 00:00:00.0000 TDB
 X = ...
$$EOE
*******************************************************************************
```

Key observations:
- Everything between `$$SOE` and `$$EOE` is data
- Each epoch has **two lines** — a timestamp line and an `X Y Z` line, then a `VX VY VZ` line
- Units are **km** and **km/s** — must multiply by 1000 to get meters and m/s
- We only need the **first epoch** for initial conditions — ignore the rest
- `LT`, `RG`, `RR` on the third data line are light time and range — we ignore these
- Scientific notation (`E+08`) is standard C++ `stod()` compatible

---

## HORIZONS Body ID Reference

For convenience — the NAIF IDs needed for common solar system bodies:

| Body | HORIZONS ID | Notes |
|------|-------------|-------|
| Sun | 10 | Solar System barycenter is `0` — use `10` for the Sun itself |
| Mercury | 199 | |
| Venus | 299 | |
| Earth | 399 | |
| Moon | 301 | |
| Mars | 499 | |
| Jupiter | 599 | |
| Saturn | 699 | |
| Uranus | 799 | |
| Neptune | 899 | |

Center body for all fetches: `@0` (Solar System Barycenter) — gives positions
in the inertial barycentric frame that your simulation expects.

---

## Mass Lookup Table

HORIZONS does not return mass. We need a hardcoded table in the source.
These values match `utils.h` already — just need to be formalized into a
lookup structure.

```cpp
// src/io/horizons_parser.cpp
static const std::unordered_map<std::string, double> BODY_MASSES_KG = {
    {"Sun",     1.98847e30},
    {"Mercury", 3.3011e23},
    {"Venus",   4.8675e24},
    {"Earth",   5.9722e24},
    {"Moon",    7.342e22},
    {"Mars",    6.4171e23},
    {"Jupiter", 1.8982e27},
    {"Saturn",  5.6834e26},
    {"Uranus",  8.681e25},
    {"Neptune", 1.02413e26},
};

static const std::unordered_map<std::string, std::string> ID_TO_NAME = {
    {"10",  "Sun"},
    {"199", "Mercury"},
    {"299", "Venus"},
    {"399", "Earth"},
    {"301", "Moon"},
    {"499", "Mars"},
    {"599", "Jupiter"},
    {"699", "Saturn"},
    {"799", "Uranus"},
    {"899", "Neptune"},
};
```

**Important:** If a user requests a body not in this table (e.g., an asteroid),
the parser should warn and use `mass = 0.0`, which is physically wrong but
allows the fetch to complete. Document this clearly in the warning message.

---

## Phase 1 — HORIZONS Text Parser

*Goal: Parse the raw HORIZONS text file and extract position + velocity for one epoch.*  
*Estimated effort: 3–4 hours*

This is the core of the entire pipeline. Everything else builds on it.

### 1.1 Create `horizons_parser.h` / `horizons_parser.cpp`

**File**: `include/horizons_parser.h` (new)

```cpp
#ifndef HORIZONS_PARSER_H
#define HORIZONS_PARSER_H

#include <string>

/**
 * HorizonsState
 * @brief: Position and velocity extracted from one HORIZONS epoch.
 *         Units are already converted to SI (meters, m/s).
 */
struct HorizonsState {
    double x,  y,  z;   // position (meters)
    double vx, vy, vz;  // velocity (m/s)
    std::string epoch;   // human-readable epoch string, e.g. "2025-Jan-01"
};

/**
 * parseHorizonsVectors
 * @brief: Extracts the FIRST epoch from a HORIZONS vector table text file.
 *         Looks for $$SOE / $$EOE markers, reads X Y Z and VX VY VZ,
 *         converts km → m and km/s → m/s.
 *
 * @param path    Path to the raw HORIZONS text file
 * @param state   Output state (filled on success)
 * @return true on success, false if file is missing or format is unexpected
 */
bool parseHorizonsVectors(const std::string& path, HorizonsState& state);

#endif // HORIZONS_PARSER_H
```

**File**: `src/io/horizons_parser.cpp` (new)

The parser algorithm:

```
1. Open file, scan line by line
2. Skip everything until we find a line containing "$$SOE"
3. Read and discard the first data line (it is the epoch timestamp — save epoch string)
4. Read next line → parse X, Y, Z   (km → multiply by 1000.0 → meters)
5. Read next line → parse VX, VY, VZ (km/s → multiply by 1000.0 → m/s)
6. Stop — we only need the first epoch
7. Return false if $$SOE is never found or lines are malformed
```

Parsing one data line — extract the three scientific notation values:

```cpp
// Line format:  " X = 1.234E+08  Y =-5.678E+07  Z = 9.012E+03"
// Strategy: find '=' signs, call std::stod() on what follows each one
// stod() handles scientific notation natively — no custom parsing needed

bool parseXYZ(const std::string& line,
              double& a, double& b, double& c)
{
    // Find positions of the three '=' characters
    // Split on '=' and parse the numeric token after each
    // Return false if any parse fails
}
```

**Why this approach is safe:** `std::stod()` handles `1.234E+08`, `-5.678e-03`,
and leading spaces natively. No regex, no custom float parser needed.

### 1.2 Unit conversion

This is physically important — get it wrong and your simulation will be off by
a factor of 1000.

```cpp
// HORIZONS outputs:  km  and  km/s
// Simulation needs:  m   and  m/s

const double KM_TO_M = 1000.0;

state.x  = raw_x  * KM_TO_M;
state.y  = raw_y  * KM_TO_M;
state.z  = raw_z  * KM_TO_M;
state.vx = raw_vx * KM_TO_M;
state.vy = raw_vy * KM_TO_M;
state.vz = raw_vz * KM_TO_M;
```

**Physics intuition:** Earth orbits at ~30 km/s. HORIZONS will give `VY = 2.978E+01`.
After multiplying by 1000 you get `29780 m/s` — which matches `earth_moon.json`
exactly. This is your sanity check.

### 1.3 Write a unit test for the parser

**File**: `tests/test_horizons_parser.cpp` (new)

```cpp
// Save a known HORIZONS snippet to a temp file
// Call parseHorizonsVectors()
// Assert:
//   - x is within 1e6 meters of expected Earth position
//   - vx is within 1.0 m/s of expected Earth velocity
//   - epoch string is not empty
```

**Definition of Done:**
```bash
# Given a saved HORIZONS text file, this should print correct SI values
parseHorizonsVectors("test_earth.txt", state);
// state.vy ≈ 29780.0  (Earth orbital velocity in m/s)
// state.x  ≈ 1.496e11 (Earth distance from Sun in meters)
```

---

## Phase 2 — JSON System Writer

*Goal: Given a list of body names, states, and masses — write a valid `systems/*.json`.*  
*Estimated effort: 1–2 hours*

This is straightforward because `loadSystemFromJSON()` already defines the exact
schema we need to write.

### 2.1 Create `system_writer.h` / `system_writer.cpp`

**File**: `include/system_writer.h` (new)

```cpp
#ifndef SYSTEM_WRITER_H
#define SYSTEM_WRITER_H

#include "body.h"
#include <string>
#include <vector>

/**
 * writeSystemJSON
 * @brief: Writes a list of CelestialBody objects to a systems/*.json file
 *         in the format loadSystemFromJSON() expects.
 *
 * @param bodies     Bodies to write (name, mass, position, velocity must be set)
 * @param systemName Human-readable name for the "name" field in JSON
 * @param epoch      Epoch string, e.g. "2025-01-01 00:00:00 TDB"
 * @param outputPath Path to write the JSON file
 * @return true on success
 */
bool writeSystemJSON(const std::vector<CelestialBody>& bodies,
                     const std::string& systemName,
                     const std::string& epoch,
                     const std::string& outputPath);

#endif // SYSTEM_WRITER_H
```

The writer uses `nlohmann::json` (already in your project) to build the object
and write it — same library used by `json_loader.cpp`:

```cpp
json j;
j["name"]  = systemName;
j["epoch"] = epoch;
j["note"]  = "Generated from NASA JPL HORIZONS. Do not edit manually.";

for (const auto& b : bodies) {
    json body;
    body["name"] = b.name;
    body["mass"] = b.mass;
    body["position"] = { b.position.x(), b.position.y(), b.position.z() };
    body["velocity"] = { b.velocity.x(), b.velocity.y(), b.velocity.z() };
    j["bodies"].push_back(body);
}

std::ofstream out(outputPath);
out << j.dump(2);  // pretty-print with 2-space indent
```

### 2.2 Round-trip validation

After writing, immediately reload with `loadSystemFromJSON()` and verify:
- Same number of bodies
- Position vectors match to within floating point tolerance
- Mass values match exactly

This catches any JSON serialization bugs before they corrupt a simulation.

**Definition of Done:**
```bash
# Written file should be identical in structure to earth_moon.json
# and loadable by the existing json loader without modification
```

---

## Phase 3 — Multi-Body Fetch Orchestrator

*Goal: Fetch multiple bodies in sequence, parse each, write one combined JSON.*  
*Estimated effort: 3–4 hours*

This is where the pipeline becomes a single command. Each body needs its own
HORIZONS request — they cannot be batched. The orchestrator manages this loop.

### 3.1 Create `horizons_builder.h` / `horizons_builder.cpp`

**File**: `include/horizons_builder.h` (new)

```cpp
#ifndef HORIZONS_BUILDER_H
#define HORIZONS_BUILDER_H

#include "body.h"
#include <string>
#include <vector>

struct BuildSystemOptions {
    std::vector<std::string> bodyIds;   // HORIZONS IDs e.g. {"10","399","301"}
    std::string epoch;                  // "2025-01-01"
    std::string center   = "@0";        // Solar System Barycenter
    std::string output;                 // path for systems/*.json
    bool        usePost  = false;       // use POST API mode
    bool        verbose  = false;
};

/**
 * buildSystemFromHorizons
 * @brief: Fetches position/velocity for each body ID from HORIZONS,
 *         looks up mass from internal table, writes a systems/*.json.
 *
 * @param opts  Options struct
 * @return true if all bodies fetched and JSON written successfully
 */
bool buildSystemFromHorizons(const BuildSystemOptions& opts);

#endif // HORIZONS_BUILDER_H
```

**File**: `src/io/horizons_builder.cpp` (new)

Algorithm:

```
For each body ID in opts.bodyIds:
    1. Map ID → name using ID_TO_NAME table
       If unknown ID → warn, use ID string as name
    2. Build HorizonsFetchOptions for a 1-step fetch:
       start = opts.epoch
       stop  = opts.epoch + 1 day   (minimum window HORIZONS accepts)
       step  = "1 d"
    3. Call fetchHorizonsEphemeris() or fetchHorizonsEphemerisPOST()
       Save to a temp file (e.g. /tmp/orb_fetch_{id}.txt)
    4. Call parseHorizonsVectors() on the temp file
       Extract first epoch only
    5. Look up mass from BODY_MASSES_KG table
       If missing → mass = 0.0, print warning
    6. Construct CelestialBody with parsed state + looked-up mass
    7. Delete temp file

Call writeSystemJSON() with all collected bodies
Call loadSystemFromJSON() to round-trip validate
Print summary
```

### 3.2 Handle the minimum time window

HORIZONS requires `stop > start`. A one-day window with `step = "1 d"` gives
exactly one epoch — the start time. This is the minimum valid request.

```cpp
// Always use a 1-day window regardless of what the user wants
// We only need the first epoch for initial conditions
HorizonsFetchOptions hopt;
hopt.start_time = opts.epoch;
hopt.stop_time  = nextDay(opts.epoch);  // simple string arithmetic: add 1 to day
hopt.step_size  = "1 d";
```

`nextDay()` is a small helper — parse `YYYY-MM-DD`, increment the day integer,
handle month boundaries. Alternatively use `std::tm` from `<ctime>`.

### 3.3 Temp file management

Use a deterministic temp path so crashes leave debuggable files:

```cpp
std::string tempPath = "/tmp/orb_fetch_" + bodyId + ".txt";
// ... fetch and parse ...
std::filesystem::remove(tempPath);  // clean up on success
// On failure: leave the file so the user can inspect it
```

**Definition of Done:**
```bash
orbit-sim build-system \
    --bodies 10,399,301 \
    --epoch "2025-01-01" \
    --output systems/earth_moon_2025.json

# Produces:
# systems/earth_moon_2025.json  ← valid, loadable by loadSystemFromJSON()
# Console shows:
#   ✅ Sun      | pos=(0.00e+00, 0.00e+00, ...) | vel=(...)  | mass=1.99e+30
#   ✅ Earth    | pos=(...)                      | vel=(...)  | mass=5.97e+24
#   ✅ Moon     | pos=(...)                      | vel=(...)  | mass=7.34e+22
#   ✅ Written: systems/earth_moon_2025.json (3 bodies)
#   ✅ Round-trip validation passed
```

---

## Phase 4 — CLI Integration

*Goal: Wire `build-system` into `orbit-sim` as a first-class command.*  
*Estimated effort: 2–3 hours*

### 4.1 Add `BuildSystemOptions` fields to `CLIOptions`

**File**: `include/cli.h`

```cpp
struct CLIOptions {
    // ... existing fields ...

    // build-system command
    std::string              buildBodies;   // raw comma-separated IDs: "10,399,301"
    std::string              buildEpoch;    // "2025-01-01"
    std::string              buildCenter;   // default "@0"
    bool                     buildRun = false; // run simulation immediately after
};
```

### 4.2 Parse `build-system` in `cli.cpp`

**File**: `src/cli/cli.cpp`

```cpp
// New options parsed when command == "build-system"
else if (a == "--bodies" && i + 1 < argc) {
    opt.buildBodies = argv[++i];   // "10,399,301"
}
else if (a == "--epoch" && i + 1 < argc) {
    opt.buildEpoch = argv[++i];
}
else if (a == "--center" && i + 1 < argc) {
    opt.buildCenter = argv[++i];
}
else if (a == "--run") {
    opt.buildRun = true;
}
```

Split the comma-separated body IDs in `main.cpp`:

```cpp
// "10,399,301" → {"10", "399", "301"}
std::vector<std::string> ids;
std::stringstream ss(opt.buildBodies);
std::string token;
while (std::getline(ss, token, ','))
    ids.push_back(token);
```

### 4.3 Handle `build-system` in `main.cpp`

**File**: `src/cli/main.cpp`

```cpp
if (opt.command == "build-system") {
    // Validate required args
    if (opt.buildBodies.empty()) {
        std::cerr << "❌ Must specify --bodies <id1,id2,...>\n";
        return 1;
    }
    if (opt.buildEpoch.empty()) {
        std::cerr << "❌ Must specify --epoch <YYYY-MM-DD>\n";
        return 1;
    }
    if (opt.output.empty()) {
        std::cerr << "❌ Must specify --output <file.json>\n";
        return 1;
    }

    BuildSystemOptions bopt;
    // ... populate from opt ...

    bool ok = buildSystemFromHorizons(bopt);
    if (!ok) return 1;

    // Optionally chain into simulation
    if (opt.buildRun) {
        auto bodies = loadSystemFromJSON(opt.output);
        runSimulation(bodies, opt.steps, opt.dt, opt.output + ".csv");
    }

    return 0;
}
```

### 4.4 Add help text

**File**: `src/cli/cli.cpp` → `printCommandHelp()`

```cpp
if (cmd == "build-system") {
    std::cout
        << "orbit-sim build-system — Fetch real ephemeris and build a system JSON\n\n"
        << "Options:\n"
        << "  --bodies 10,399,301    Comma-separated HORIZONS body IDs\n"
        << "  --epoch  YYYY-MM-DD    Epoch for initial conditions\n"
        << "  --center @0            Reference center (default: @0 = barycenter)\n"
        << "  --output FILE          Where to write the systems/*.json\n"
        << "  --run                  Run simulation immediately after building\n"
        << "  --steps N              (if --run) number of steps\n"
        << "  --dt T                 (if --run) timestep in seconds\n"
        << "  --post                 Use POST API mode\n"
        << "  --verbose              Show raw fetch details\n\n"
        << "Body IDs (common):\n"
        << "  10=Sun  199=Mercury  299=Venus  399=Earth  301=Moon\n"
        << "  499=Mars  599=Jupiter  699=Saturn  799=Uranus  899=Neptune\n\n"
        << "Example:\n"
        << "  orbit-sim build-system --bodies 10,399,301 \\\n"
        << "      --epoch 2025-01-01 --output systems/earth_moon_real.json\n";
    return;
}
```

### 4.5 Update CMakeLists.txt

**File**: `CMakeLists.txt`

Add the new source files to `orbit_core`:

```cmake
add_library(orbit_core STATIC
    src/core/simulation.cpp
    src/core/conservations.cpp
    src/core/barycenter.cpp
    src/core/eclipse.cpp
    src/core/json_loader.cpp
    src/io/horizons_parser.cpp    # NEW
    src/io/horizons_builder.cpp   # NEW
    src/io/system_writer.cpp      # NEW
)
```

**Definition of Done:**
```bash
orbit-sim build-system --help   # prints help correctly
orbit-sim build-system \
    --bodies 10,399,301 \
    --epoch "2025-01-01" \
    --output systems/earth_moon_real.json
orbit-sim validate --system systems/earth_moon_real.json  # passes
orbit-sim run --system systems/earth_moon_real.json --steps 8760 --dt 3600
```

---

## Phase 5 — Validation Against Existing Systems

*Goal: Verify that HORIZONS-generated systems produce physically correct simulations.*  
*Estimated effort: 2–3 hours*

This phase builds scientific credibility — the whole point of the HORIZONS
integration is accuracy.

### 5.1 Compare generated JSON against hand-written JSON

The existing `systems/earth_moon.json` uses approximate initial conditions.
After running `build-system` for the same bodies, compare:

```bash
orbit-sim info --system systems/earth_moon.json
orbit-sim info --system systems/earth_moon_real.json
```

Expected: Earth velocity magnitude ≈ 29780 m/s in both. Moon ≈ 30802 m/s.
Position magnitudes should match to within ~1% (since `earth_moon.json` uses
approximate circular orbits, not the true elliptical position).

### 5.2 Run conservation test on HORIZONS-generated system

```bash
orbit-sim run --system systems/earth_moon_real.json \
              --steps 10000 --dt 60 \
              --output results/horizons_test.csv

# Then check dE_rel in the CSV — should be < 1e-5
```

Reuse the existing `test_conservation.cpp` logic — just point it at the
new system file.

### 5.3 Ephemeris comparison study

This connects to the validation study described in `docs/validation.md`:

```
1. Fetch Earth position at T=0 from HORIZONS (epoch 2025-01-01)
2. Run simulation for 30 days
3. Fetch Earth position at T=30 days from HORIZONS
4. Compare simulated position vs HORIZONS position
5. Report position residual in km
```

Document results in `docs/validation.md` under a new section:
*"HORIZONS Pipeline Validation"*.

**Target accuracy:** position residual < 1000 km after 30 days.
This is achievable with RK4 at `dt=60s` for the Earth-Moon system.

**Definition of Done:**
- Generated JSON validates cleanly
- Conservation test passes (`|dE/E₀| < 1e-5`)
- 30-day position residual documented

---

## Phase 6 — Python Integration and Makefile Targets

*Goal: Make the full pipeline accessible from Python scripts and Makefile.*  
*Estimated effort: 1 hour*

### 6.1 Add Makefile targets

**File**: `Makefile`

```makefile
# Fetch and build Earth-Moon system at current epoch
build-earth-moon: $(SIM_EXE)
	@$(SIM_EXE) build-system \
		--bodies 10,399,301 \
		--epoch 2025-01-01 \
		--output systems/earth_moon_horizons.json

# Fetch and build Solar System at current epoch
build-solar-system: $(SIM_EXE)
	@$(SIM_EXE) build-system \
		--bodies 10,199,299,399,301,499,599,699,799,899 \
		--epoch 2025-01-01 \
		--output systems/solar_system_horizons.json

# Full pipeline: fetch → simulate → view
pipeline-earth-moon: build-earth-moon $(SIM_EXE) $(VIEWER_EXE)
	@$(SIM_EXE) run \
		--system systems/earth_moon_horizons.json \
		--steps 876000 --dt 60 \
		--output results/earth_moon_horizons.csv
	@$(VIEWER_EXE) results/earth_moon_horizons.csv
```

### 6.2 Add `fetch_and_build.py` helper

**File**: `python/fetch_and_build.py`

A thin Python wrapper that calls `orbit-sim build-system` and then
runs the Python plotting pipeline on the result:

```python
"""
fetch_and_build.py
Convenience script: fetch real ephemeris → simulate → plot.
"""
import subprocess
import sys

BODIES  = "10,399,301"
EPOCH   = "2025-01-01"
OUTPUT  = "systems/earth_moon_horizons.json"
STEPS   = 876000
DT      = 60

# 1. Build system JSON from HORIZONS
subprocess.run([
    "./build/bin/orbit-sim", "build-system",
    "--bodies", BODIES,
    "--epoch",  EPOCH,
    "--output", OUTPUT,
], check=True)

# 2. Run simulation
subprocess.run([
    "./build/bin/orbit-sim", "run",
    "--system", OUTPUT,
    "--steps",  str(STEPS),
    "--dt",     str(DT),
    "--output", "results/earth_moon_horizons.csv",
], check=True)

# 3. Plot
subprocess.run(["python3", "python/energy_conservation.py"], check=True)
subprocess.run(["python3", "python/3Dplot_earth_moon.py"],   check=True)
```

### 6.3 Update CLI reference documentation

**File**: `docs/orbital_mechanics_engine_cli_reference.md`

Add a new section:

```
## 10. BUILD SYSTEM FROM HORIZONS

### Earth-Moon at 2025-01-01
./bin/orbit-sim build-system \
    --bodies 10,399,301 \
    --epoch 2025-01-01 \
    --output systems/earth_moon_real.json

### Solar System at 2025-01-01
./bin/orbit-sim build-system \
    --bodies 10,199,299,399,301,499,599,699,799,899 \
    --epoch 2025-01-01 \
    --output systems/solar_system_real.json

### Fetch and run immediately
./bin/orbit-sim build-system \
    --bodies 10,399,301 \
    --epoch 2025-01-01 \
    --output systems/earth_moon_real.json \
    --run --steps 876000 --dt 60 \
    --output results/earth_moon_real.csv
```

---

## Implementation Priority Summary

| Priority | Phase | Task | Why |
|----------|-------|------|-----|
| 🔴 Now | 1.1–1.2 | HORIZONS text parser + unit conversion | Everything else depends on this |
| 🔴 Now | 1.3 | Parser unit test | Catches unit conversion bugs before they corrupt simulations |
| 🟠 Soon | 2.1–2.2 | JSON system writer + round-trip validation | Needed before multi-body fetch |
| 🟠 Soon | 3.1–3.3 | Multi-body fetch orchestrator | Core of the pipeline |
| 🟠 Soon | 4.1–4.5 | CLI `build-system` command + CMake | Exposes everything to the user |
| 🟡 Medium | 5.1–5.2 | Validation against existing systems | Builds scientific credibility |
| 🟡 Medium | 5.3 | 30-day ephemeris residual study | Connects to `docs/validation.md` |
| 🟢 Later | 6.1–6.3 | Makefile targets + Python script + docs | Polish and convenience |

---

## Versioning

| Version | Milestone |
|---------|-----------|
| Current | Manual JSON authoring, raw HORIZONS text saved but not parsed |
| v1.2.1 | Phase 1–2 complete — parser + writer working, tested |
| v1.2.2 | Phase 3–4 complete — `build-system` CLI command working |
| v1.3 | Phase 5 complete — validation study documented |
| v1.3.1 | Phase 6 complete — Makefile + Python pipeline |

---

## Files Changed Summary

| File | Change |
|------|--------|
| `include/horizons_parser.h` | New — `HorizonsState` struct + `parseHorizonsVectors()` |
| `src/io/horizons_parser.cpp` | New — parser implementation |
| `include/system_writer.h` | New — `writeSystemJSON()` declaration |
| `src/io/system_writer.cpp` | New — JSON writer using nlohmann |
| `include/horizons_builder.h` | New — `BuildSystemOptions` + `buildSystemFromHorizons()` |
| `src/io/horizons_builder.cpp` | New — orchestrator: fetch → parse → write |
| `include/cli.h` | Add `buildBodies`, `buildEpoch`, `buildCenter`, `buildRun` fields |
| `src/cli/cli.cpp` | Parse new `build-system` options + help text |
| `src/cli/main.cpp` | Handle `build-system` command, chain into `runSimulation()` |
| `CMakeLists.txt` | Add 3 new source files to `orbit_core` |
| `tests/test_horizons_parser.cpp` | New — parser unit test |
| `Makefile` | New `build-earth-moon`, `build-solar-system`, `pipeline-earth-moon` targets |
| `python/fetch_and_build.py` | New — end-to-end Python convenience script |
| `docs/orbital_mechanics_engine_cli_reference.md` | Document `build-system` command |
| `docs/validation.md` | New section: HORIZONS Pipeline Validation |

---

*This roadmap is a living document. Update it as phases are completed.*
