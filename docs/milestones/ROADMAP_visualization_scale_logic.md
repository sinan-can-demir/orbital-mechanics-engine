## Milestone Roadmap: Generic N-Body Visualization

**Goal**: Make the viewer work correctly for any N-body system without hardcoded assumptions.

---

## Current State (Honest Baseline)

| Component | Status | Problem |
|-----------|--------|---------|
| `radiusForBody()` | ❌ Hardcoded | Name lookup table, breaks for unknown bodies |
| `colorForBody()` | ❌ Hardcoded | Name lookup table, breaks for unknown bodies |
| `DIST_VIS_SCALE` | ❌ Hardcoded | Single constant, wrong for different systems |
| `MOON_EXAGGERATION` | ❌ Hardcoded | Assumes Moon exists by name |
| Mass in viewer | ❌ Missing | CSV has no mass data, viewer can't use it |
| `buildOrbitBuffers()` | ✅ Fixed | Moved outside render loop |
| Frame-rate cap | ✅ Fixed | Delta-time accumulator added |
| Stride metadata | 🚧 Planned | Not yet implemented |

---

## Phase 1 — CSV Metadata Foundation

*Everything else depends on this. The viewer needs to know mass, stride, and dt from the CSV itself — no external files, no hardcoding.*

**Goal**: One self-describing CSV file contains everything the viewer needs.

### 1.1 Extend metadata comment in `simulation.cpp`

Change the header write to include mass per body:

```cpp
// Format:
// # stride=1 dt=60.0 bodies=Sun:1.989e30,Earth:5.972e24,Moon:7.342e22
file << "# stride=" << stride
     << " dt=" << dt
     << " bodies=";

for (size_t i = 0; i < bodies.size(); ++i)
{
    file << bodies[i].name << ":" << bodies[i].mass;
    if (i + 1 < bodies.size()) file << ",";
}
file << "\n";
```

**Why mass?** The viewer needs it to compute visual radius generically. This is the only physical quantity missing from the CSV right now.

### 1.2 Parse metadata in `initBodiesFromCSV()`

```cpp
struct CSVMetadata
{
    int    stride  = 1;
    double dt      = 3600.0;
    std::unordered_map<std::string, double> masses;  // name → kg
};

static CSVMetadata g_metadata;
```

Parse the comment line before the column header:

```cpp
if (line.rfind("# ", 0) == 0)
{
    // parse stride=, dt=, bodies=
    // store into g_metadata
    std::getline(file, line); // advance to actual header
}
```

### 1.3 Store mass in `BodyRenderInfo`

```cpp
struct BodyRenderInfo
{
    std::string name;
    double      mass = 0.0;   // ← add this
    glm::vec3   color;
    float       radius;
    // ... rest unchanged
};
```

After parsing the metadata, assign mass when building body info:

```cpp
auto it = g_metadata.masses.find(name);
if (it != g_metadata.masses.end())
    body.mass = it->second;
```

**Definition of Done**:
```bash
orbit-sim run --system systems/earth_moon.json --steps 1000 --dt 60
# First line of CSV:
# stride=1 dt=60.0 bodies=Sun:1.98847e+30,Earth:5.9722e+24,Moon:7.342e+22
```

---

## Phase 2 — Generic Visual Radius

*Replace the name-based lookup table with physics-based scaling.*

**Goal**: Any body in any system gets a correct visual size relative to other bodies.

### 2.1 Implement `computeVisualRadius()`

```cpp
static float computeVisualRadius(double mass)
{
    // Only one reference value needed:
    // Earth mass → Earth visual size in GL units
    const double M_EARTH    = 5.9722e24;
    const float  R_EARTH_GL = 0.25f;

    if (mass <= 0.0) return 0.1f;  // fallback for unknown mass

    // Radius scales as cube root of mass
    // Physically: R ∝ M^(1/3) assuming roughly constant density
    double ratio = mass / M_EARTH;
    float  r     = R_EARTH_GL * static_cast<float>(std::cbrt(ratio));

    // Clamp: tiny bodies still visible, giants don't swamp view
    return std::clamp(r, 0.05f, 4.0f);
}
```

### 2.2 Remove `radiusForBody()`

Delete the entire function. Replace every call with:

```cpp
body.radius = computeVisualRadius(body.mass);
```

### 2.3 Handle the zero-mass case

If mass is missing from metadata (e.g. old CSV without the comment), fall back gracefully:

```cpp
if (body.mass <= 0.0)
{
    std::cerr << "⚠️ No mass data for " << body.name
              << " — using default visual radius\n";
    body.radius = 0.2f;
}
```

**Definition of Done**:
- Load `earth_moon.json` simulation → Sun visually larger than Earth, Moon visually smaller
- Load a hypothetical binary star system → both stars sized relative to each other
- No body names referenced anywhere in radius logic

---

## Phase 3 — Generic Color

*Replace the name-based color lookup with deterministic hashing.*

**Goal**: Any body gets a unique, consistent color automatically.

### 3.1 Implement hash-based color

```cpp
static glm::vec3 computeBodyColor(const std::string& name)
{
    // Hash the name → deterministic color
    // Same name always gets same color across runs
    size_t hash = std::hash<std::string>{}(name);

    // Keep colors bright enough to see on dark background
    float r = 0.35f + 0.65f * ((hash & 0xFF)         / 255.0f);
    float g = 0.35f + 0.65f * (((hash >> 8)  & 0xFF) / 255.0f);
    float b = 0.35f + 0.65f * (((hash >> 16) & 0xFF) / 255.0f);

    return glm::vec3(r, g, b);
}
```

### 3.2 Remove `colorForBody()`

Delete the function. Replace every call with:

```cpp
body.color = computeBodyColor(body.name);
```

### 3.3 Keep the legend colors in sync

The HUD legend draws Sun/Earth/Moon with hardcoded colors right now:

```cpp
// Current — hardcoded
drawLegendBox(..., glm::vec3(1.4f, 1.1f, 0.3f));  // Sun color

// New — read from body data
drawLegendBox(..., g_bodies[g_bodyIndex["Sun"]].color);
```

But this also means the legend itself should be generic — draw one box per body, not just three. See Phase 5.

**Definition of Done**:
- Load any system → all bodies get distinct colors
- Same CSV loaded twice → identical colors both times
- No body names referenced anywhere in color logic

---

## Phase 4 — Generic Distance Scaling

*Replace the hardcoded `DIST_VIS_SCALE` with auto-computed scaling from the data.*

**Goal**: Any system auto-fits to a reasonable view on startup.

### 4.1 Remove `DIST_VIS_SCALE` constant

```cpp
// DELETE THIS:
static constexpr float DIST_VIS_SCALE = 0.02f;

// REPLACE WITH:
static float g_distVisScale = 1.0f;  // computed after loading
```

### 4.2 Implement `computeDistScale()`

Called after all positions are loaded, before building GPU buffers:

```cpp
static float computeDistScale(const std::vector<BodyRenderInfo>& bodies)
{
    float maxDist = 0.0f;

    for (const auto& body : bodies)
    {
        for (const auto& pos : body.positions)
        {
            // pos is already in GL units (after DIST_SCALE_METERS)
            maxDist = std::max(maxDist, glm::length(pos));
        }
    }

    if (maxDist < 1e-9f) return 1.0f;

    // Scale so the outermost orbit fits within TARGET_GL units
    const float TARGET_GL = 80.0f;
    return TARGET_GL / maxDist;
}
```

### 4.3 Apply scale after loading

```cpp
// After all positions loaded into body.positions:
g_distVisScale = computeDistScale(g_bodies);

// Then re-scale all positions:
for (auto& body : g_bodies)
    for (auto& pos : body.positions)
        pos *= g_distVisScale;
```

### 4.4 Remove hardcoded Moon exaggeration

```cpp
// DELETE THIS BLOCK ENTIRELY:
if (MOON_EXAGGERATION != 1.0f)
{
    auto itEarth = g_bodyIndex.find("Earth");
    auto itMoon  = g_bodyIndex.find("Moon");
    // ...
}
```

The auto-scaling handles this — if Moon's orbit is small relative to the system, it will naturally be closer to Earth in the scaled view.

### 4.5 Add live tuning keys

```cpp
case GLFW_KEY_LEFT_BRACKET:   // '[' — zoom out system
    g_distVisScale *= 0.8f;
    reapplyDistScale();
    std::cout << "🔭 Dist scale: " << g_distVisScale << "\n";
    break;

case GLFW_KEY_RIGHT_BRACKET:  // ']' — zoom in system
    g_distVisScale *= 1.25f;
    reapplyDistScale();
    std::cout << "🔭 Dist scale: " << g_distVisScale << "\n";
    break;
```

**Definition of Done**:
- Load Earth-Moon CSV → Moon orbit visible, not clipped
- Load Solar System CSV → all planets visible in one view
- No `DIST_VIS_SCALE`, `MOON_EXAGGERATION`, or body-name checks anywhere

---

## Phase 5 — Generic HUD Legend

*The legend currently hardcodes three rows: Sun, Earth, Moon. It should show whatever bodies are actually in the simulation.*

**Goal**: Legend auto-generates from loaded body data.

### 5.1 Replace hardcoded legend draw calls

```cpp
// DELETE THIS (3 hardcoded boxes):
drawLegendBox(LEGEND_BASE_X, y0, ..., glm::vec3(1.4f, 1.1f, 0.3f));
drawLegendBox(LEGEND_BASE_X, y1, ..., glm::vec3(0.2f, 0.8f, 1.2f));
drawLegendBox(LEGEND_BASE_X, y2, ..., glm::vec3(0.85f, 0.85f, 0.92f));

// REPLACE WITH:
for (size_t i = 0; i < g_bodies.size(); ++i)
{
    float y = LEGEND_BASE_Y + static_cast<float>(i) * LEGEND_SPACING;
    bool  selected = (g_focusBodyIndex == static_cast<int>(i));
    float size     = selected ? LEGEND_SIZE_PX * 1.4f : LEGEND_SIZE_PX;
    drawLegendBox(LEGEND_BASE_X, y, size, g_bodies[i].color);
}
```

### 5.2 Replace hardcoded camera target enum

```cpp
// DELETE the CameraTarget enum entirely
// REPLACE WITH a simple index:
static int g_focusBodyIndex = -1;  // -1 = barycenter

// Click on legend box i → g_focusBodyIndex = i
// Camera target = g_bodies[g_focusBodyIndex].positions[frame]
```

### 5.3 Update click detection

```cpp
static void handleLegendClick(double mouseX, double mouseY)
{
    for (size_t i = 0; i < g_bodies.size(); ++i)
    {
        float y    = LEGEND_BASE_Y + static_cast<float>(i) * LEGEND_SPACING;
        float half = LEGEND_SIZE_PX * 0.5f;

        if (mouseX >= LEGEND_BASE_X - half && mouseX <= LEGEND_BASE_X + half &&
            mouseY >= y - half             && mouseY <= y + half)
        {
            g_focusBodyIndex = static_cast<int>(i);
            std::cout << "📌 Focus: " << g_bodies[i].name << "\n";
            return;
        }
    }
}
```

### 5.4 Update number key handling

```cpp
// Instead of hardcoded switch cases:
// Key '1' → focus body 0, '2' → body 1, etc.
if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9 && action == GLFW_PRESS)
{
    int idx = key - GLFW_KEY_1;
    if (idx < static_cast<int>(g_bodies.size()))
    {
        g_focusBodyIndex = idx;
        std::cout << "📌 Focus: " << g_bodies[idx].name << "\n";
    }
}
```

**Definition of Done**:
- Legend shows exactly the bodies in the loaded CSV, no more, no less
- Click any legend item → camera focuses that body
- `CameraTarget` enum deleted entirely

---

## Implementation Priority

| Priority | Phase | Task | Why |
|----------|-------|------|-----|
| 🔴 Now | 1.1–1.2 | CSV metadata comment + parser | Foundation for everything else |
| 🔴 Now | 1.3 | Mass in `BodyRenderInfo` | Needed before radius can be computed |
| 🟠 Soon | 2.1–2.3 | Generic visual radius | Fixes the Sun domination problem |
| 🟠 Soon | 3.1–3.2 | Hash-based color | Removes all name dependencies |
| 🟡 Medium | 4.1–4.5 | Auto distance scaling | Makes any system fit the view |
| 🟡 Medium | 5.1–5.4 | Generic HUD legend | Removes last hardcoded name assumptions |
| 🟢 After | — | I/O migration (HDF5/Binary) | Build on a clean viewer foundation |

---

## Versioning

| Version | Milestone |
|---------|-----------|
| Current | Hardcoded viewer, fixed render loop |
| v1.1.1 | Phase 1–2 complete — metadata + generic radius |
| v1.1.2 | Phase 3–4 complete — generic color + auto scaling |
| v1.2 | Phase 5 complete — fully generic viewer, I/O migration ready |

---

## Files Changed

| File | Change |
|------|--------|
| `src/core/simulation.cpp` | Add metadata comment to CSV header |
| `src/viewer/orbit_viewer.cpp` | All phases — generic radius, color, scale, legend |
| `include/viewer/csv_loader.h` | Add `CSVMetadata` struct |
| `docs/milestones/` | Add this roadmap as `ROADMAP_generic_viewer.md` |

---