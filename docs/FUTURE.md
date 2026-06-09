# Future Development Ideas

This document tracks potential directions for the orbital mechanics engine. These are not commitments — just ideas worth exploring.

---

## I/O improvements

The current CSV output format is human-readable but slow for large simulations. Two improvements worth exploring:

**Binary output format** — a compact `.orb` file storing float64 positions with a JSON sidecar for metadata. Would enable stride=1 full-resolution trajectories without file size penalties.

**NASA SPICE/SPK support** — SPICE is NASA's standard format for ephemeris data. Supporting it directly would allow importing trajectories from any NASA mission and comparing simulation output against real spacecraft data.

---

## Native desktop application

A native desktop GUI (e.g. Qt) would make the engine accessible to users who are not comfortable with the command line or Jupyter notebooks. The OpenGL viewer already exists as a foundation — a GUI wrapper could expose system configuration, integrator selection, and real-time parameter tuning without touching any code.

Web apps are more accessible in theory but introduce browser compatibility issues and lose direct access to the filesystem and native performance. A native app keeps the C++ core intact.

---

## Systems database

The current `systems/` directory has a handful of JSON files. A curated library would be more useful:

- All major solar system configurations at different epochs
- Historical systems (e.g. early solar system with different planetary positions)
- Hypothetical systems (binary stars, hot Jupiters, unstable configurations)
- A CLI command to list and fetch systems by name

---

## Collision detection

Currently bodies pass through each other. A collision detector would compare the distance between any two bodies against the sum of their physical radii. When a collision is detected, the simulation could log the event and optionally halt.

---

## Merge detection and body merging

An extension of collision detection: when two bodies collide, merge them into a single body conserving mass and momentum. This would enable simulating accretion, planetary formation, and multi-body instability scenarios.

---

## Rogue body detection

A body "goes rogue" when its total mechanical energy becomes positive — meaning its kinetic energy exceeds the gravitational binding energy of the system and it will escape to infinity. Detecting this in real time would allow:

- Logging the ejection time and velocity
- Flagging unstable system configurations automatically
- Studying chaotic N-body dynamics where ejection probability is a measurable quantity

This is a well-defined physical condition: `E_total = KE + PE > 0`.

---

## Notes

These ideas vary in scope. Rogue body detection and collision detection are small additions to the existing simulation loop. Binary I/O and body merging are medium-sized projects. The native app and systems database are larger efforts.

Contributions toward any of these are welcome — see [CONTRIBUTING.md](../CONTRIBUTING.md).
