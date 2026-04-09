#!/usr/bin/env python3
"""
horizons_validation.py
----------------------
Compares simulation output against NASA JPL HORIZONS endpoint positions.

Study design:
    1. Use real HORIZONS initial conditions at epoch T0 (2025-01-01)
    2. Simulate forward for 7, 30, 180 days with RK4 at dt=60s
    3. Fetch HORIZONS positions at each endpoint
    4. Compute position residual |r_sim - r_horizons| in km
    5. Print results table and write to docs/validation_results.md

Usage:
    python3 python/horizons_validation.py

Requirements:
    - orbit-sim binary must be built (build/bin/orbit-sim)
    - Network access to JPL HORIZONS API
    - pip install pandas
"""

import subprocess
import os
import sys
import json
import math
import pandas as pd
from pathlib import Path
from datetime import datetime, timedelta

# ── Paths ─────────────────────────────────────────────────────────────────────
REPO_ROOT  = Path(__file__).resolve().parent.parent
SIM_EXE    = REPO_ROOT / "build/bin/orbit-sim"
SYSTEM     = REPO_ROOT / "systems/earth_moon_horizons_2025.json"
RESULTS    = REPO_ROOT / "results/horizons_validation"
HORIZONS_PARSER = REPO_ROOT / "src/io/horizons_parser.cpp"  # just for reference

EPOCH      = "2025-01-01"
EPOCH_DT   = datetime(2025, 1, 1)

# ── Study parameters ──────────────────────────────────────────────────────────
DURATIONS_DAYS = [7, 30, 180]
DT_SECONDS     = 60.0       # timestep
STRIDE         = 1440       # write once per simulated day (1440 min/day)

# HORIZONS body IDs we care about
BODIES = {
    "Earth": "399",
    "Moon":  "301",
}

# ── Helpers ───────────────────────────────────────────────────────────────────

def run(cmd, cwd=REPO_ROOT, check=True):
    """Run a shell command and return stdout."""
    result = subprocess.run(
        cmd, shell=True, cwd=cwd,
        capture_output=True, text=True
    )
    if check and result.returncode != 0:
        print(f"ERROR running: {cmd}")
        print(result.stderr)
        sys.exit(1)
    return result.stdout.strip()


def date_plus_days(epoch: str, days: int) -> str:
    """Return YYYY-MM-DD string for epoch + days."""
    d = datetime.strptime(epoch, "%Y-%m-%d") + timedelta(days=days)
    return d.strftime("%Y-%m-%d")


def fetch_horizons_position(body_id: str, date: str) -> tuple[float, float, float]:
    """
    Fetch position vector for a body at a given date from HORIZONS.
    Returns (x, y, z) in meters.

    Uses orbit-sim fetch under the hood, then parses the raw text.
    """
    stop_date = date_plus_days(date, 1)
    out_file  = RESULTS / f"horizons_{body_id}_{date}.txt"

    cmd = (
        f"{SIM_EXE} fetch "
        f"--body {body_id} "
        f"--center @0 "
        f"--start {date} "
        f"--stop {stop_date} "
        f"--step '1 d' "
        f"--output {out_file} "
        f"--post"
    )

    print(f"  Fetching body {body_id} at {date}...")
    run(cmd)

    return parse_horizons_xyz(out_file)


def parse_horizons_xyz(path: Path) -> tuple[float, float, float]:
    """
    Parse X Y Z from a HORIZONS vector table text file.
    Converts km → m.

    Expected format after $$SOE:
        timestamp line
        X = ... Y = ... Z = ...
        VX= ... VY= ... VZ= ...
    """
    KM_TO_M = 1000.0

    with open(path) as f:
        lines = f.readlines()

    found_soe = False
    for i, line in enumerate(lines):
        if "$$SOE" in line:
            found_soe = True
            # timestamp is lines[i+1]
            # XYZ      is lines[i+2]
            xyz_line = lines[i + 2]
            break

    if not found_soe:
        print(f"ERROR: $$SOE not found in {path}")
        sys.exit(1)

    # Parse: " X = 1.234E+08 Y =-5.678E+07 Z = 9.012E+03"
    parts = xyz_line.split("=")
    # parts[0] = " X "
    # parts[1] = " 1.234E+08 Y "   <- x value + next label
    # parts[2] = "-5.678E+07 Z "   <- y value + next label
    # parts[3] = " 9.012E+03"      <- z value

    x = float(parts[1].split()[0]) * KM_TO_M
    y = float(parts[2].split()[0]) * KM_TO_M
    z = float(parts[3].split()[0]) * KM_TO_M

    return x, y, z


def load_sim_endpoint(csv_path: Path, body_name: str) -> tuple[float, float, float]:
    """
    Load the last row of a simulation CSV and return
    position of body_name as (x, y, z) in meters.
    """
    df = pd.read_csv(csv_path, comment='#')

    x_col = f"x_{body_name}"
    y_col = f"y_{body_name}"
    z_col = f"z_{body_name}"

    if x_col not in df.columns:
        print(f"ERROR: column {x_col} not found in {csv_path}")
        print(f"Available columns: {list(df.columns)}")
        sys.exit(1)

    last = df.iloc[-1]
    print(f"    Reading step {int(last['step'])} of {len(df)-1} total rows")

    return float(last[x_col]), float(last[y_col]), float(last[z_col])


def distance_km(p1: tuple, p2: tuple) -> float:
    """Euclidean distance between two 3D points, result in km."""
    dx = p1[0] - p2[0]
    dy = p1[1] - p2[1]
    dz = p1[2] - p2[2]
    return math.sqrt(dx*dx + dy*dy + dz*dz) / 1000.0


def run_simulation(duration_days: int) -> Path:
    steps  = int(duration_days * 86400 / DT_SECONDS)
    outdir = RESULTS / f"{duration_days}day"
    outdir.mkdir(parents=True, exist_ok=True)
    out    = outdir / "orbit.csv"

    # stride=1: write every step so iloc[-1] is exactly the endpoint
    # For 7 days at dt=60s this is 10080 rows — manageable
    # For 180 days this is 259200 rows — still fine for validation
    stride = 1

    print(f"\n  Simulating {duration_days} days "
          f"({steps} steps at dt={DT_SECONDS}s, stride=1)...")

    cmd = (
        f"{SIM_EXE} run "
        f"--system {SYSTEM} "
        f"--steps {steps} "
        f"--dt {DT_SECONDS} "
        f"--stride {stride} "
        f"--output {out}"
    )
    run(cmd)
    print(f"  Simulation complete → {out}")
    return out

# ── Main ──────────────────────────────────────────────────────────────────────

def main():
    RESULTS.mkdir(parents=True, exist_ok=True)

    if not SIM_EXE.exists():
        print(f"ERROR: orbit-sim not found at {SIM_EXE}")
        print("Run: cmake --build build --parallel")
        sys.exit(1)

    if not SYSTEM.exists():
        print(f"ERROR: system file not found at {SYSTEM}")
        print("Run: ./build/bin/orbit-sim build-system --bodies 10,399,301 "
              "--epoch 2025-01-01 --output systems/earth_moon_horizons_2025.json --post")
        sys.exit(1)

    print("=" * 60)
    print("HORIZONS Validation Study")
    print(f"Epoch:      {EPOCH}")
    print(f"System:     {SYSTEM.name}")
    print(f"Integrator: RK4")
    print(f"dt:         {DT_SECONDS} s")
    print(f"Durations:  {DURATIONS_DAYS} days")
    print("=" * 60)

    results = []  # list of dicts for the table

    for days in DURATIONS_DAYS:
        print(f"\n{'─'*60}")
        print(f"Duration: {days} days")
        print(f"{'─'*60}")

        endpoint_date = date_plus_days(EPOCH, days)

        # ── Step 1: Run simulation ─────────────────────────────────────────
        csv_path = run_simulation(days)

        # ── Step 2: Fetch HORIZONS endpoint positions ──────────────────────
        print(f"\n  Fetching HORIZONS endpoint positions at {endpoint_date}...")
        horizons_pos = {}
        for body_name, body_id in BODIES.items():
            horizons_pos[body_name] = fetch_horizons_position(body_id, endpoint_date)
            x, y, z = horizons_pos[body_name]
            print(f"  HORIZONS {body_name}: ({x:.4e}, {y:.4e}, {z:.4e}) m")

        # ── Step 3: Load simulation endpoint positions ─────────────────────
        print(f"\n  Loading simulation endpoint...")
        sim_pos = {}
        for body_name in BODIES:
            sim_pos[body_name] = load_sim_endpoint(csv_path, body_name)
            x, y, z = sim_pos[body_name]
            print(f"  Sim      {body_name}: ({x:.4e}, {y:.4e}, {z:.4e}) m")

        # ── Step 4: Compute residuals ──────────────────────────────────────
        print(f"\n  Residuals:")
        for body_name in BODIES:
            err_km = distance_km(sim_pos[body_name], horizons_pos[body_name])
            print(f"  {body_name:6s}: {err_km:,.1f} km")

            results.append({
                "Duration (days)": days,
                "Body":            body_name,
                "Error (km)":      round(err_km, 1),
                "Integrator":      "RK4",
                "dt (s)":          int(DT_SECONDS),
            })

    # ── Results table ──────────────────────────────────────────────────────────
    print(f"\n{'='*60}")
    print("RESULTS SUMMARY")
    print(f"{'='*60}")

    df = pd.DataFrame(results)
    print(df.to_string(index=False))

    # ── Write markdown ────────────────────────────────────────────────────────
    md_path = RESULTS / "validation_results.md"
    write_markdown(df, md_path)
    print(f"\n✅ Results written to {md_path}")
    print("   Copy the relevant section into docs/validation.md")


def write_markdown(df: pd.DataFrame, path: Path):
    """Write results as a markdown section."""
    now = datetime.now().strftime("%Y-%m-%d")

    lines = [
        "# HORIZONS Validation Results",
        "",
        f"**Generated:** {now}  ",
        f"**Epoch:** {EPOCH}  ",
        f"**Integrator:** RK4  ",
        f"**Timestep:** {int(DT_SECONDS)} s  ",
        f"**System:** Earth-Moon from real HORIZONS initial conditions  ",
        "",
        "## Position Residuals vs NASA JPL HORIZONS",
        "",
        "| Duration (days) | Body | Position Error (km) |",
        "|-----------------|------|---------------------|",
    ]

    for _, row in df.iterrows():
        lines.append(
            f"| {row['Duration (days)']:>15} "
            f"| {row['Body']:4} "
            f"| {row['Error (km)']:>19,.1f} |"
        )

    lines += [
        "",
        "## Notes",
        "",
        "- Positions are in the Solar System Barycentric frame (ICRF/J2000)",
        "- HORIZONS uses a full solar system dynamical model including",
        "  relativistic corrections and all planetary perturbations",
        "- This simulation uses Newtonian gravity only (Sun + Earth + Moon)",
        "- Error growth over time reflects both integrator drift and",
        "  missing physics (other planets, relativity)",
        "- Moon error is larger than Earth error because the Moon's orbit",
        "  is more sensitive to perturbations from other planets",
        "",
    ]

    with open(path, "w") as f:
        f.write("\n".join(lines))


if __name__ == "__main__":
    main()
