"""
compare_integrators.py
----------------------
Plots energy, angular momentum, and linear momentum drift
for two simulation CSVs side by side.

Designed for comparing RK4 vs Leapfrog, but works for any two runs.

Usage:
    python3 python/compare_integrators.py
"""

import sys
import os
sys.path.insert(0, os.path.dirname(__file__))

import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
from utils import pick_csv, load_csv, get_x_axis

# ── Load ──────────────────────────────────────────────────────────────────────

print("Select the FIRST integrator results (e.g. RK4):")
path_a = pick_csv("First CSV (conservation):", conservation=True)
df_a   = load_csv(path_a)
label_a = os.path.basename(path_a).replace(".csv", "")

print("Select the SECOND integrator results (e.g. Leapfrog):")
path_b = pick_csv("Second CSV (conservation):", conservation=True)
df_b   = load_csv(path_b)
label_b = os.path.basename(path_b).replace(".csv", "")

# ── Axis ──────────────────────────────────────────────────────────────────────

x_a, xlabel = get_x_axis(df_a)
x_b, _      = get_x_axis(df_b)

# ── Plot ──────────────────────────────────────────────────────────────────────

fig, axes = plt.subplots(3, 1, figsize=(12, 10), sharex=False)
fig.suptitle(f"Integrator Comparison\n{label_a}  vs  {label_b}",
             fontsize=13, fontweight="bold")

COLOR_A = "#E24B4A"
COLOR_B = "#1D9E75"

panels = [
    ("dE_rel", "Relative Energy Drift  |ΔE/E₀|",          "Energy conservation"),
    ("dL_rel", "Relative Angular Momentum Drift  |ΔL/L₀|", "Angular momentum conservation"),
    ("dP_rel", "Relative Momentum Drift  |ΔP/P₀|",         "Linear momentum conservation"),
]

for ax, (col, ylabel, title) in zip(axes, panels):
    if col in df_a.columns:
        ax.plot(x_a, df_a[col], color=COLOR_A, linewidth=0.8,
                label=label_a, alpha=0.9)
    if col in df_b.columns:
        ax.plot(x_b, df_b[col], color=COLOR_B, linewidth=0.8,
                label=label_b, alpha=0.9)

    ax.set_ylabel(ylabel, fontsize=10)
    ax.set_title(title, fontsize=11)
    ax.legend(fontsize=10)
    ax.yaxis.set_major_formatter(ticker.ScalarFormatter(useMathText=True))
    ax.ticklabel_format(style="sci", axis="y", scilimits=(0, 0))
    ax.grid(True, alpha=0.3)

axes[-1].set_xlabel(xlabel, fontsize=10)

# ── Summary ───────────────────────────────────────────────────────────────────

print("\n── Energy drift summary ──────────────────────────────")
for label, df in [(label_a, df_a), (label_b, df_b)]:
    if "dE_rel" in df.columns:
        print(f"  {label:<30} final |dE/E₀| : {df['dE_rel'].iloc[-1]:.6e}")
        print(f"  {label:<30} max   |dE/E₀| : {df['dE_rel'].abs().max():.6e}")

print("\n── Angular momentum drift summary ────────────────────")
for label, df in [(label_a, df_a), (label_b, df_b)]:
    if "dL_rel" in df.columns:
        print(f"  {label:<30} final |dL/L₀| : {df['dL_rel'].iloc[-1]:.6e}")

# ── Save ──────────────────────────────────────────────────────────────────────

out_dir  = "results/conservation-graphs"
out_path = f"{out_dir}/integrator_comparison.png"
os.makedirs(out_dir, exist_ok=True)

plt.tight_layout()
plt.savefig(out_path, dpi=150, bbox_inches="tight")
print(f"\n✅ Saved → {out_path}")
plt.show()