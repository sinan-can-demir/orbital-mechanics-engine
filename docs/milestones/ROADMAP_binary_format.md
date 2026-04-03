# Roadmap — Binary Simulation Format

*Replacing the CSV pipeline with a chunked binary format + JSON sidecar to handle large simulation outputs (1GB+) in the OpenGL viewer.*

**Author**: Sinan Can Demir  
**Last Updated**: April 2026  
**Motivation**: A 1-year Earth–Moon simulation at `dt=60s` produces a ~1 GB CSV file. `initBodiesFromCSV()` in `orbit_viewer.cpp` loads all frames into RAM unconditionally, and the render loop has no frame-rate cap — together these make the viewer unusable for large runs.

---

## Current State (Problem Summary)

| Component | Status | Problem |
|-----------|--------|---------|
| `runSimulation()` writer | ✅ Working | Writes text CSV — slow, ~4× larger than binary |
| `initBodiesFromCSV()` | ✅ Working | Loads **all** frames into RAM unconditionally |
| Render loop timing | ❌ Broken | No frame-rate cap — burns GPU, plays back instantly |
| Viewer memory usage | ❌ Broken | 1 GB CSV → ~300M `glm::vec3` in RAM → crash/lag |
| Python plotting | ✅ Working | Reads CSV — needs a binary reader added |

**Known limitations going into this work:**
- `CLIOptions` has no `format` field — `--format binary` is not yet parsed
- `csv_loader.cpp` hardcodes Moon exaggeration (15×) and is not generic
- The viewer assumes `x_Sun`, `x_Earth`, `x_Moon` columns — `orbit_viewer.cpp` partially fixes this but `csv_loader.cpp` does not
- No render loop timing exists anywhere — `glfwSwapBuffers()` runs uncapped

---

## Binary File Format Specification

Before implementation, the format must be fixed so writer and reader stay in sync.

### Sidecar JSON (`.orb.json`)

Human-readable metadata written alongside the binary file:

```json
{
  "version": 1,
  "bodies": ["Sun", "Earth", "Moon"],
  "num_bodies": 3,
  "num_frames": 876000,
  "dt": 60.0,
  "stride": 1,
  "integrator": "rk4",
  "system": "systems/earth_moon.json",
  "epoch": "2025-01-01T00:00:00"
}
```

### Binary Data File (`.orb`)

Fixed-width, row-major layout. No compression initially (add later if needed).

```
[ HEADER — 64 bytes ]
  magic number    : uint32  (0x4F524249 = "ORBI")
  version         : uint32  (1)
  num_bodies      : uint32
  num_frames      : uint64
  dt              : double
  stride          : uint32
  reserved        : padding to 64 bytes

[ DATA BLOCK — num_frames × num_bodies × 3 × 8 bytes ]
  For each frame:
    For each body (in order from JSON sidecar):
      x : double  (meters)
      y : double  (meters)
      z : double  (meters)
```

**Size calculation:**
- 3 bodies × 3 coords × 8 bytes = 72 bytes/frame
- 876,000 frames (1 year at dt=60s) = **~63 MB**
- 10 bodies (Solar System) = **~210 MB**
- Compare: current CSV at same size = **~1 GB**

Conservation columns (energy, momentum, drift) are kept in a separate `.conservation.csv` — these are small and humans need to read them for debugging. They do not go in the binary file.

---

## Phase 1 — Fix the Render Loop (Do This First)

*Goal: Make the viewer usable with the existing CSV before touching any format.*  
*Estimated effort: 1–2 hours*

**Why first:** Even after switching to binary, a broken render loop will still make the viewer unusable. Fix the foundation before building on it.

### 1.1 Add frame-rate cap and delta-time accumulation

**File**: `src/viewer/orbit_viewer.cpp`

Add to the top of the render loop:

```cpp
// Real-time playback control
static double g_lastTime    = 0.0;
static double g_simSpeed    = 1.0;   // simulation frames per real second
static double g_accumulator = 0.0;
```

Replace the unconditional `g_frameIndex++` with:

```cpp
double now = glfwGetTime();
double dt_real = now - g_lastTime;
g_lastTime = now;

// Cap delta time to avoid spiral of death on slow frames
dt_real = std::min(dt_real, 0.05);

g_accumulator += dt_real * g_simSpeed;
while (g_accumulator >= 1.0) {
    g_frameIndex = (g_frameIndex + 1) % g_numFrames;
    g_accumulator -= 1.0;
}
```

**Physics connection:** `g_simSpeed` is the number of simulation frames consumed per real second. At `g_simSpeed = 1440` and `dt=60s`, 1 real second = 1440 simulation frames = 1 simulated day. This is how time acceleration works in real mission planning tools.

### 1.2 Wire up `+` / `-` keys for playback speed

**File**: `src/viewer/orbit_viewer.cpp` → `key_callback()`

```cpp
case GLFW_KEY_EQUAL:  // '+' key
    g_simSpeed *= 2.0;
    std::cout << "⏩ Sim speed: " << g_simSpeed << " frames/s\n";
    break;
case GLFW_KEY_MINUS:
    g_simSpeed = std::max(1.0, g_simSpeed * 0.5);
    std::cout << "⏪ Sim speed: " << g_simSpeed << " frames/s\n";
    break;
case GLFW_KEY_SPACE:
    g_simSpeed = (g_simSpeed > 0.0) ? 0.0 : 1440.0;
    break;
```

### 1.3 Add viewer-side frame skip on load

**File**: `src/viewer/orbit_viewer.cpp` → `initBodiesFromCSV()`

Add a `--skip N` argument to the viewer so large CSVs can be sampled:

```cpp
// Only push frame if (lineCount % g_loadSkip == 0)
if (lineCount % g_loadSkip == 0) {
    for (size_t bi = 0; bi < g_bodies.size(); ++bi)
        g_bodies[bi].positions.push_back(framePos[bi]);
}
```

**Definition of Done:**
- Viewer plays back at a stable 60 fps
- Space pauses, `+`/`-` control speed
- `--skip 10` on a 1 GB CSV loads in seconds

---

## Phase 2 — Binary Writer in `runSimulation()`

*Goal: Emit `.orb` + `.orb.json` when `--format binary` is passed.*  
*Estimated effort: 3–4 hours*

### 2.1 Add `format` field to `CLIOptions`

**File**: `include/cli.h`

```cpp
struct CLIOptions {
    // ... existing fields ...
    std::string format = "csv";   // "csv" or "binary"
};
```

**File**: `src/cli/cli.cpp` → `parseCLI()`

```cpp
else if (a == "--format" && i + 1 < argc) {
    opt.format = argv[++i];
}
```

**File**: `src/cli/main.cpp` → run block

```cpp
std::cout << " - Format:     " << opt.format << "\n";
runSimulation(bodies, steps, dt, outPath, integrator, stride, opt.format);
```

### 2.2 Add binary writer to `runSimulation()`

**File**: `include/simulation.h`

```cpp
void runSimulation(std::vector<CelestialBody>& bodies, int steps, double dt,
                   const std::string& outputPath,
                   Integrator integrator = Integrator::RK4,
                   int stride = 1,
                   const std::string& format = "csv");
```

**File**: `src/core/simulation.cpp`

Extract a `writeBinaryHeader()` and `writeBinaryFrame()` helper. Keep the CSV path completely untouched — the `format` parameter just selects which branch runs.

Binary write per frame (after the stride check):

```cpp
if (format == "binary") {
    for (const auto& b : bodies) {
        double x = b.position.x(), y = b.position.y(), z = b.position.z();
        binFile.write(reinterpret_cast<const char*>(&x), sizeof(double));
        binFile.write(reinterpret_cast<const char*>(&y), sizeof(double));
        binFile.write(reinterpret_cast<const char*>(&z), sizeof(double));
    }
}
```

### 2.3 Write the JSON sidecar

Derive sidecar path from output path:
`results/earth_moon.orb` → `results/earth_moon.orb.json`

Write it after the simulation loop completes (when `num_frames` is known).

### 2.4 Keep conservation logging in CSV

Conservation columns (`dE_rel`, `dL_rel`, `dP_rel`, `E_total`, etc.) stay in a separate `.conservation.csv`. These are small, humans need to read them, and your existing Python plotting scripts (`energy_conservation.py`, `angular_momentum_plot.py`) must keep working without changes.

**Definition of Done:**
```bash
# This produces earth_moon.orb + earth_moon.orb.json + earth_moon.conservation.csv
orbit-sim run --system systems/earth_moon.json --steps 876000 --dt 60 --format binary --output results/earth_moon.orb
```

---

## Phase 3 — Chunked Binary Reader in the Viewer

*Goal: Replace `initBodiesFromCSV()` with a lazy-loading binary reader.*  
*Estimated effort: 4–5 hours*

This is the most important phase for actually fixing the viewer.

### 3.1 Design the chunk loader

Instead of loading all frames at startup, maintain a sliding window:

```
Total frames:  876,000
Window size:   5,000 frames  (~360 KB for 3 bodies)
Current chunk: frames [40000 .. 45000]
```

When `g_frameIndex` approaches the end of the current chunk, load the next one from disk in a background thread or simply on demand.

**File**: `include/viewer/binary_loader.h` (new file)

```cpp
class BinaryLoader {
public:
    bool open(const std::string& binPath, const std::string& jsonPath);
    bool loadChunk(size_t startFrame, size_t numFrames);

    size_t totalFrames() const;
    size_t numBodies()   const;
    const std::vector<std::string>& bodyNames() const;

    // Returns position of body bi at frame fi (within loaded chunk)
    glm::vec3 position(size_t bi, size_t fi) const;

private:
    std::ifstream      m_file;
    size_t             m_numBodies  = 0;
    size_t             m_totalFrames = 0;
    size_t             m_chunkStart = 0;
    std::vector<float> m_chunk;           // interleaved x,y,z per body per frame
    std::vector<std::string> m_bodyNames;
    std::streampos     m_dataStart;       // byte offset to first frame
};
```

### 3.2 Replace `initBodiesFromCSV()` in `orbit_viewer.cpp`

Detect format by file extension:
- `.orb` → use `BinaryLoader`
- `.csv` → keep existing `CSVLoader` (backwards compatible)

```cpp
if (csvPath.ends_with(".orb")) {
    initBodiesFromBinary(csvPath, csvPath + ".json");
} else {
    initBodiesFromCSV(csvPath);
}
```

### 3.3 Add chunk advancing to the render loop

After `g_frameIndex` advances:

```cpp
// Refill chunk if we're within 500 frames of the end
if (g_frameIndex > g_loader.chunkEnd() - 500) {
    g_loader.loadChunk(g_loader.chunkEnd(), CHUNK_SIZE);
}
```

**Definition of Done:**
```bash
orbit-viewer results/earth_moon.orb
# Opens instantly, plays smoothly, uses <50 MB RAM regardless of simulation length
```

---

## Phase 4 — Python Compatibility

*Goal: Existing plotting scripts work with binary files.*  
*Estimated effort: 1–2 hours*

### 4.1 Add `read_orb.py` utility

**File**: `python/read_orb.py`

```python
import numpy as np
import json
import struct
from pathlib import Path

def load_orb(path: str):
    """
    Load a .orb binary simulation file.
    Returns: dict with keys matching body names, each value is (N, 3) numpy array.
    """
    path = Path(path)
    meta_path = path.with_suffix(path.suffix + ".json")

    with open(meta_path) as f:
        meta = json.load(f)

    n_bodies = meta["num_bodies"]
    n_frames = meta["num_frames"]
    names    = meta["bodies"]

    # Skip 64-byte header
    data = np.fromfile(path, dtype=np.float64, offset=64)
    data = data.reshape(n_frames, n_bodies, 3)

    return {names[i]: data[:, i, :] for i in range(n_bodies)}
```

### 4.2 Update existing plotting scripts

Each script gets a 3-line shim at the top:

```python
# Try binary first, fall back to CSV
if os.path.exists("results/earth_moon.orb"):
    from read_orb import load_orb
    bodies = load_orb("results/earth_moon.orb")
    mx, my, mz = bodies["Moon"].T
else:
    df = pd.read_csv("build/orbit_three_body.csv")
    mx, my, mz = df["x_Moon"], df["y_Moon"], df["z_Moon"]
```

**Scripts to update:**
- `python/3Dplot.py`
- `python/3Dplot_earth_moon.py`
- `python/3Dexaggerated_plot.py`
- `python/3Draytracing.py`

**Definition of Done:**
```bash
python3 python/3Dplot.py  # works with .orb file, no CSV needed
```

---

## Phase 5 — CLI and Documentation Cleanup

*Goal: Everything is consistent and documented.*  
*Estimated effort: 1 hour*

### 5.1 Update CLI reference

**File**: `docs/orbital_mechanics_engine_cli_reference.md`

Add `--format` to the `run` command section:

```bash
./bin/orbit-sim run \
    --system ../systems/earth_moon.json \
    --steps 876000 \
    --dt 60 \
    --format binary \
    --output results/earth_moon.orb
```

### 5.2 Update Makefile targets

```makefile
run-earth-moon-binary: $(SIM_EXE)
	@$(SIM_EXE) run \
		--system $(SYSTEMS_DIR)/earth_moon.json \
		--steps 876000 \
		--dt 60 \
		--format binary \
		--output $(RESULTS_DIR)/earth_moon.orb

view-binary: $(VIEWER_EXE)
	@$(VIEWER_EXE) $(RESULTS_DIR)/earth_moon.orb
```

### 5.3 Update `.gitignore`

```
*.orb
*.orb.json
*.conservation.csv
```

---

## Implementation Priority Summary

| Priority | Phase | Task | Why |
|----------|-------|------|-----|
| 🔴 Now | 1.1 | Fix render loop timing | Viewer is currently broken for any large file |
| 🔴 Now | 1.2 | Add space/+/- playback controls | Zero-cost UX improvement |
| 🔴 Now | 1.3 | Viewer-side frame skip on load | Buys time before binary is ready |
| 🟠 Soon | 2.1–2.3 | Binary writer + JSON sidecar | Reduces 1 GB → ~60 MB |
| 🟠 Soon | 3.1–3.2 | Chunked binary reader | Fixes RAM problem permanently |
| 🟡 Medium | 3.3 | Chunk advancing in render loop | Needed for very long simulations |
| 🟡 Medium | 4.1 | `read_orb.py` utility | Python plotting compatibility |
| 🟡 Medium | 4.2 | Update plotting scripts | Full Python workflow restored |
| 🟢 Later | 5.1–5.3 | CLI docs + Makefile + gitignore | Polish |

---

## Versioning

| Version | Milestone |
|---------|-----------|
| Current | CSV-only pipeline, viewer broken on large files |
| v1.1.1 | Phase 1 complete — render loop fixed, viewer usable |
| v1.2 | Phase 2–3 complete — binary format, chunked viewer |
| v1.3 | Phase 4–5 complete — full Python + doc compatibility |

---

## Files Changed Summary

| File | Change |
|------|--------|
| `src/viewer/orbit_viewer.cpp` | Render loop timing, playback controls, frame skip, binary detection |
| `src/core/simulation.cpp` | Binary writer branch in `runSimulation()` |
| `include/simulation.h` | Add `format` param to `runSimulation()` |
| `include/cli.h` | Add `format` field to `CLIOptions` |
| `src/cli/cli.cpp` | Parse `--format` option |
| `src/cli/main.cpp` | Pass `format` to `runSimulation()` |
| `include/viewer/binary_loader.h` | New chunked binary loader interface |
| `src/viewer/binary_loader.cpp` | New chunked binary loader implementation |
| `python/read_orb.py` | New Python binary reader utility |
| `python/3Dplot.py` et al. | Binary fallback shim |
| `docs/orbital_mechanics_engine_cli_reference.md` | Document `--format` flag |
| `Makefile` | New binary run/view targets |
| `.gitignore` | Ignore `.orb` files |

---

*This roadmap is a living document. Update it as phases are completed.*
