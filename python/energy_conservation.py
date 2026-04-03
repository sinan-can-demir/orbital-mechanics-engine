#!/usr/bin/env python3
"""
File: energy_conservation.py
Author: Sinan Demir
Date: 11/17/2025
Purpose: Energy Conservation Plotter for Earth–Moon–Sun Simulation

    Reads orbit_three_body.csv and plots:
    - Total energy over time
    - Kinetic and potential components
    - Relative energy drift dE/E0
"""

import pandas as pd
import matplotlib.pyplot as plt

# ==============================
# Load simulation data
# ==============================
df = pd.read_csv("build/orbit_three_body.csv")

# Extract columns
steps = df["step"]
E_total = df["E_total"]
KE = df["KE"]
PE = df["PE"]
dE = df["dE_rel"]

# ==============================
# Plot Energy Components
# ==============================
plt.figure(figsize=(10, 6))
plt.plot(steps, KE, label="Kinetic Energy", alpha=0.8)
plt.plot(steps, PE, label="Potential Energy", alpha=0.8)
plt.plot(steps, E_total, label="Total Energy", linewidth=2.0)

plt.title("Energy Conservation in Sun–Earth–Moon Simulation")
plt.xlabel("Simulation Step")
plt.ylabel("Energy (J)")
plt.grid(True, linestyle="--", alpha=0.4)
plt.legend()
plt.tight_layout()
plt.savefig("energy_conservation.png", dpi=300)
plt.show()

# ==============================
# Plot Relative Energy Drift
# ==============================
plt.figure(figsize=(10, 6))
plt.plot(steps, dE, label="Relative Energy Drift (dE/E0)", color="red")

plt.title("Relative Energy Drift")
plt.xlabel("Simulation Step")
plt.ylabel("dE / E0")
plt.grid(True, linestyle="--", alpha=0.4)
plt.legend()
plt.tight_layout()
plt.savefig("energy_drift.png", dpi=300)
plt.show()
