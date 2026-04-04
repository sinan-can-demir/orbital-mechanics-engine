#!/usr/bin/env python3
"""
File: energy_conservation.py
Author: Sinan Demir
Date: 04/04/2026
Purpose: Energy Conservation Plotter for N-Body Simulation

    Reads *.csv and plots:
    - Total energy over time
    - Kinetic and potential components
    - Relative energy drift dE/E0
"""

from utils import pick_csv, plot_multi, plot_conservation
import pandas as pd

data_file = pick_csv("Select simulation CSV:")
df = pd.read_csv(data_file, comment='#')

# Energy components
plot_multi(df,
    series=[
        ("KE",      "Kinetic Energy",   "blue"),
        ("PE",      "Potential Energy", "orange"),
        ("E_total", "Total Energy",     "black"),
    ],
    title="Energy Conservation — Sun–Earth–Moon",
    ylabel="Energy (J)",
    out_path="results/conservation-graphs/energy.png"
)

# Drift
plot_conservation(df,
    y_col    = "dE_rel",
    title    = "Relative Energy Drift",
    ylabel   = "ΔE / E₀",
    out_path = "results/conservation-graphs/energy_drift.png",
    color    = "red"
)