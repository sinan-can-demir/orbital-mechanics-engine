"""
compare_integrators.py
----------------------
Plots energy drift (dE_rel) for RK4 vs Leapfrog side by side.
Run from the project root:
    python3 plotting_scripts/compare_integrators.py
"""

import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import sys
import os

# ── Paths ────────────────────────────────────────────────────
RK4_CSV      = "results/rk4_coarse.csv"
LEAPFROG_CSV = "results/leapfrog_coarse.csv"

for path in [RK4_CSV, LEAPFROG_CSV]:
    if not os.path.exists(path):
        print(f"❌ Missing: {path}")
        print("   Run both simulations first.")
        sys.exit(1)

# ── Load ─────────────────────────────────────────────────────
rk4 = pd.read_csv(RK4_CSV)
lf  = pd.read_csv(LEAPFROG_CSV)

# Convert step number to simulated days (dt=60s → 1440 steps/day)
DT_SECONDS   = 60.0
SECONDS_PER_DAY = 86400.0
rk4["days"] = rk4["step"] * DT_SECONDS / SECONDS_PER_DAY
lf["days"]  = lf["step"]  * DT_SECONDS / SECONDS_PER_DAY

# ── Plot ──────────────────────────────────────────────────────
fig, axes = plt.subplots(3, 1, figsize=(12, 10), sharex=True)
fig.suptitle("RK4 vs Leapfrog — Earth–Moon system\n"
             "50 000 steps · dt = 60 s · ~34.7 simulated days",
             fontsize=13, fontweight="bold")

# ── Panel 1: Energy drift ─────────────────────────────────────
ax = axes[0]
ax.plot(rk4["days"], rk4["dE_rel"],  color="#E24B4A", linewidth=0.8,
        label="RK4",      alpha=0.9)
ax.plot(lf["days"],  lf["dE_rel"],   color="#1D9E75", linewidth=0.8,
        label="Leapfrog", alpha=0.9)
ax.set_ylabel("Relative energy drift  |ΔE/E₀|", fontsize=10)
ax.set_title("Energy conservation", fontsize=11)
ax.legend(fontsize=10)
ax.yaxis.set_major_formatter(ticker.ScalarFormatter(useMathText=True))
ax.ticklabel_format(style="sci", axis="y", scilimits=(0, 0))
ax.grid(True, alpha=0.3)

# ── Panel 2: Angular momentum drift ──────────────────────────
ax = axes[1]
ax.plot(rk4["days"], rk4["dL_rel"],  color="#E24B4A", linewidth=0.8,
        label="RK4",      alpha=0.9)
ax.plot(lf["days"],  lf["dL_rel"],   color="#1D9E75", linewidth=0.8,
        label="Leapfrog", alpha=0.9)
ax.set_ylabel("Relative angular momentum drift  |ΔL/L₀|", fontsize=10)
ax.set_title("Angular momentum conservation", fontsize=11)
ax.legend(fontsize=10)
ax.yaxis.set_major_formatter(ticker.ScalarFormatter(useMathText=True))
ax.ticklabel_format(style="sci", axis="y", scilimits=(0, 0))
ax.grid(True, alpha=0.3)

# ── Panel 3: Linear momentum drift ───────────────────────────
ax = axes[2]
ax.plot(rk4["days"], rk4["dP_rel"],  color="#E24B4A", linewidth=0.8,
        label="RK4",      alpha=0.9)
ax.plot(lf["days"],  lf["dP_rel"],   color="#1D9E75", linewidth=0.8,
        label="Leapfrog", alpha=0.9)
ax.set_ylabel("Relative momentum drift  |ΔP/P₀|", fontsize=10)
ax.set_title("Linear momentum conservation", fontsize=11)
ax.set_xlabel("Simulated time (days)", fontsize=10)
ax.legend(fontsize=10)
ax.yaxis.set_major_formatter(ticker.ScalarFormatter(useMathText=True))
ax.ticklabel_format(style="sci", axis="y", scilimits=(0, 0))
ax.grid(True, alpha=0.3)

# ── Summary stats ─────────────────────────────────────────────
print("\n── Energy drift summary ──────────────────────────────")
print(f"  RK4      final |dE/E₀| : {rk4['dE_rel'].iloc[-1]:.6e}")
print(f"  Leapfrog final |dE/E₀| : {lf['dE_rel'].iloc[-1]:.6e}")
print(f"  RK4      max   |dE/E₀| : {rk4['dE_rel'].abs().max():.6e}")
print(f"  Leapfrog max   |dE/E₀| : {lf['dE_rel'].abs().max():.6e}")

print("\n── Angular momentum drift summary ────────────────────")
print(f"  RK4      final |dL/L₀| : {rk4['dL_rel'].iloc[-1]:.6e}")
print(f"  Leapfrog final |dL/L₀| : {lf['dL_rel'].iloc[-1]:.6e}")

plt.tight_layout()

out = "results/integrator_comparison_coarse.png"
plt.savefig(out, dpi=150, bbox_inches="tight")
print(f"\n✅ Saved → {out}")
plt.show()