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

data_file = pick_csv("Select simulation CSV to analyze:")
df        = load_csv(data_file)

out_dir = "results/conservation-graphs"

# ── Plot 1: Angular momentum magnitude ────────────────────────────────────────

plot_conservation(df,
    y_col    = "Lmag",
    title    = "Total Angular Momentum  |L|",
    ylabel   = "|L|  (kg·m²/s)",
    out_path = f"{out_dir}/angular_momentum.png",
    color    = "#1f77b4"
)

# ── Plot 2: Relative drift ────────────────────────────────────────────────────

plot_conservation(df,
    y_col    = "dL_rel",
    title    = "Relative Angular Momentum Drift  |ΔL / L₀|",
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
    title    = "Angular Momentum Components",
    ylabel   = "L  (kg·m²/s)",
    out_path = f"{out_dir}/angular_momentum_components.png"
)