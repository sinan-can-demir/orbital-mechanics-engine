"""
File: angular_momentum_plot.py
Author: Sinan Demir
Date: 11/17/2025
Purpose: Angular Momentum Plotter for Earth–Moon–Sun Simulation
    Reads orbit_three_body.csv and plots:
    - Total angular momentum magnitude |L| over time
    - Relative angular momentum drift ΔL / L₀
"""


#!/usr/bin/env python3
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

df = pd.read_csv("build/orbit_three_body.csv")

# Extract logged angular momentum columns
Lx = df["Lx"]
Ly = df["Ly"]
Lz = df["Lz"]

# Magnitude
L_mag = np.sqrt(Lx**2 + Ly**2 + Lz**2)

# Drift relative to initial
L0 = L_mag.iloc[0]
dL_rel = (L_mag - L0) / L0

path = "results/conservation-graphs/"

# ---------- PLOT 1: |L|(t) ----------
plt.figure(figsize=(10,5))
plt.plot(df["step"], L_mag)
plt.title("Total Angular Momentum |L| vs Time")
plt.xlabel("Step")
plt.ylabel("|L|  (kg·m²/s)")
plt.grid(True)
plt.tight_layout()
plt.savefig(f"{path}angular_momentum.png")
plt.close()

# ---------- PLOT 2: Relative drift ----------
plt.figure(figsize=(10,5))
plt.plot(df["step"], dL_rel)
plt.title("Relative Angular Momentum Drift")    
plt.xlabel("Step")
plt.ylabel("ΔL / L₀")
plt.grid(True)
plt.tight_layout()
plt.savefig(f"{path}angular_momentum_drift.png")
plt.close()

print(f"Saved: angular_momentum.png, angular_momentum_drift.png to {path}")
