"""
angular_momentum_plot.py
------------------------
Plots angular momentum conservation diagnostics for any simulation CSV.

Usage:
    python3 python/angular_momentum_plot.py
"""

import sys
import os
sys.path.insert(0, os.path.dirname(__file__))

import numpy as np
from utils import pick_csv, load_csv, plot_conservation, plot_multi

# ── Load ──────────────────────────────────────────────────────────────────────

data_file = pick_csv("Select conservation CSV:", conservation=True)
df        = load_csv(data_file)

stem    = data_file.split("/")[-1].replace("_conservation.csv", "").replace("_", " ").title()
out_dir = "results/conservation-graphs"

# ── Plot 1: Angular momentum magnitude ────────────────────────────────────────

plot_conservation(df,
    y_col    = "Lmag",
    title    = f"Total Angular Momentum  |L| — {stem}",
    ylabel   = "|L|  (kg·m²/s)",
    out_path = f"{out_dir}/angular_momentum.png",
    color    = "#1f77b4"
)

# ── Plot 2: Relative drift ────────────────────────────────────────────────────

plot_conservation(df,
    y_col    = "dL_rel",
    title    = f"Relative Angular Momentum Drift — {stem}",
    ylabel   = "ΔL / L₀",
    out_path = f"{out_dir}/angular_momentum_drift.png",
    color    = "#9467bd"
)

# ── Plot 3: All three components ──────────────────────────────────────────────

plot_multi(df,
    series=[
        ("Lx", "Lx", "#1f77b4"),
        ("Ly", "Ly", "#ff7f0e"),
        ("Lz", "Lz", "#2ca02c"),
    ],
    title    = f"Angular Momentum Components — {stem}",
    ylabel   = "L  (kg·m²/s)",
    out_path = f"{out_dir}/angular_momentum_components.png"
)
