# orbit-sim CLI --- Test Commands Reference

Author: Sinan Demir

This file lists **EVERY working CLI command** with example invocations
for fast testing.\
All commands assume you run them from:
```
orbital-mechanics-engine/build
```
Executables are located in:
```
build/bin/orbit-sim
build/bin/orbit-viewer
```
Use:
```
./bin/orbit-sim <command> [options]
```
------------------------------------------------------------------------

## 1. HELP & USAGE

### Global help
```
./bin/orbit-sim help
./bin/orbit-sim --help
```
### Command-specific help
```
./bin/orbit-sim help run
./bin/orbit-sim help fetch
./bin/orbit-sim help validate
./bin/orbit-sim help info
./bin/orbit-sim help list
```
------------------------------------------------------------------------

## 2. LIST SYSTEMS
```
./bin/orbit-sim list
```
------------------------------------------------------------------------

## 3. PRINT SYSTEM INFO
```
./bin/orbit-sim info --system ../systems/earth_moon.json
./bin/orbit-sim info --system ../systems/sun_earth.json
```
------------------------------------------------------------------------

## 4. VALIDATE SYSTEM FILE
```
./bin/orbit-sim validate --system ../systems/earth_moon.json
./bin/orbit-sim validate --system ../systems/broken.json
```
------------------------------------------------------------------------

## 5. RUN SIMULATION

### Default output:
```
./bin/orbit-sim run --system ../systems/earth_moon.json
```
### Custom output:
```
./bin/orbit-sim run   --system ../systems/earth_moon.json   --steps 500   --dt 3600   --output orbit.csv
```
### 1-year simulation:
```
./bin/orbit-sim run   --system ../systems/earth_moon.json   --steps 8760   --dt 3600   --output orbit_year.csv
```

```
./bin/orbit-sim run --system ../systems/solar_system.json --dt 3600 --steps 100000
```

------------------------------------------------------------------------

## 6. FETCH NASA HORIZONS EPHEMERIS --- GET MODE
```
./bin/orbit-sim fetch   --body 399   --start 2025-01-01   --stop 2025-01-02   --step "6 h"   --output earth_raw.txt
```
------------------------------------------------------------------------

## 7. FETCH NASA HORIZONS EPHEMERIS --- POST MODE
```
./bin/orbit-sim fetch   --body 399   --center @0   --start 2025-01-01   --stop 2025-01-02   --step "6 h"   --output earth_post.txt   --post   --verbose
```
------------------------------------------------------------------------

## 8. UNKNOWN COMMAND TEST
```
./bin/orbit-sim somethingInvalid
```
------------------------------------------------------------------------

## 9. VIEW SIMULATION IN OPENGL VIEWER
```
./bin/orbit-viewer
```