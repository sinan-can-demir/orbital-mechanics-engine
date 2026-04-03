#!/usr/bin/env python3
"""
fetch_and_build.py
Convenience script: fetch real ephemeris -> build system JSON -> run simulation -> plot.
"""

import subprocess
from pathlib import Path


def run(cmd, cwd):
    print(">>", " ".join(str(c) for c in cmd))
    subprocess.run(cmd, cwd=cwd, check=True)


def main():
    repo_root = Path(__file__).resolve().parent.parent
    sim_exe = repo_root / "build/bin/orbit-sim"

    bodies = "10,399,301"
    epoch = "2025-01-01"
    system_out = repo_root / "systems/earth_moon_horizons.json"
    csv_out = repo_root / "build/orbit_three_body.csv"

    system_out.parent.mkdir(parents=True, exist_ok=True)
    csv_out.parent.mkdir(parents=True, exist_ok=True)

    run(
        [
            str(sim_exe),
            "build-system",
            "--bodies",
            bodies,
            "--epoch",
            epoch,
            "--output",
            str(system_out),
        ],
        cwd=repo_root,
    )

    run(
        [
            str(sim_exe),
            "run",
            "--system",
            str(system_out),
            "--steps",
            "876000",
            "--dt",
            "60",
            "--output",
            str(csv_out),
        ],
        cwd=repo_root,
    )

    run(["python3", "python/energy_conservation.py"], cwd=repo_root)
    run(["python3", "python/3Dplot_earth_moon.py"], cwd=repo_root)


if __name__ == "__main__":
    main()
