"""
File: energy_conservation.py
Author: Sinan Demir
Date: 11/17/2025
Purpose: Utility functions for data analysis

    pick_csv: a helper for file picking
"""
"""
utils.py
--------
Shared utilities for orbital mechanics plotting scripts.
"""

import glob
import os
import sys

import matplotlib.pyplot as plt
import pandas as pd


# ── File Picker ───────────────────────────────────────────────────────────────

def pick_csv(prompt: str, conservation: bool = False) -> str:
    """
    Pick a CSV from results/.
    
    Args:
        conservation: if True, show _conservation.csv files
                      if False, show position files only
    """
    all_files = sorted(glob.glob("results/**/*.csv", recursive=True)
                      + glob.glob("results/*.csv"))

    seen = set()
    filtered = []
    for f in all_files:
        if f in seen:
            continue
        seen.add(f)

        # Always skip eclipse logs
        if "_eclipse" in f:
            continue

        # Filter based on what caller wants
        is_conservation = "_conservation" in f
        if conservation and not is_conservation:
            continue
        if not conservation and is_conservation:
            continue

        filtered.append(f)

    if not filtered:
        kind = "conservation" if conservation else "position"
        print(f"❌ No {kind} CSV files found in results/")
        sys.exit(1)

    print(f"\n{prompt}")
    print("─" * 50)
    for i, f in enumerate(filtered):
        size_kb = os.path.getsize(f) / 1024
        print(f"  [{i + 1}] {os.path.basename(f):<45} {size_kb:.1f} KB")
    print()

    while True:
        choice = input(f"Select [1-{len(filtered)}]: ").strip()
        if choice.isdigit() and 1 <= int(choice) <= len(filtered):
            selected = filtered[int(choice) - 1]
            print(f"📂 Loaded: {selected}\n")
            return selected
        print("   Invalid choice, try again.")


def load_csv(path: str) -> pd.DataFrame:
    """
    Load a simulation CSV, skipping the metadata comment line.

    Args:
        path: Path to the CSV file.

    Returns:
        pandas DataFrame with simulation data.
    """
    return pd.read_csv(path, comment='#')


def get_x_axis(df: pd.DataFrame):
    """
    Returns (x_values, x_label) for plotting.
    Uses simulated days if time_s column exists, otherwise step count.
    """
    if "time_s" in df.columns:
        return df["time_s"] / 86400.0, "Simulated Time (days)"
    return df["step"], "Step"


# ── Plotters ──────────────────────────────────────────────────────────────────

def plot_conservation(df: pd.DataFrame,
                      y_col: str,
                      title: str,
                      ylabel: str,
                      out_path: str,
                      color: str = "steelblue") -> None:
    """
    Plot a single conservation quantity over time.

    Args:
        df       : Simulation DataFrame.
        y_col    : Column name to plot.
        title    : Plot title.
        ylabel   : Y-axis label.
        out_path : Path to save the PNG.
        color    : Line color.
    """
    x, xlabel = get_x_axis(df)

    os.makedirs(os.path.dirname(out_path), exist_ok=True)

    plt.figure(figsize=(10, 5))
    plt.plot(x, df[y_col], color=color, linewidth=0.8)
    plt.title(title)
    plt.xlabel(xlabel)
    plt.ylabel(ylabel)
    plt.grid(True, linestyle="--", alpha=0.4)
    plt.tight_layout()
    plt.savefig(out_path, dpi=300)
    plt.close()
    print(f"✅ Saved: {out_path}")


def plot_multi(df: pd.DataFrame,
               series: list,
               title: str,
               ylabel: str,
               out_path: str) -> None:
    """
    Plot multiple columns on the same axes.

    Args:
        df       : Simulation DataFrame.
        series   : List of (column_name, label, color) tuples.
        title    : Plot title.
        ylabel   : Y-axis label.
        out_path : Path to save the PNG.

    Example:
        plot_multi(df,
            series=[
                ("KE",      "Kinetic",   "blue"),
                ("PE",      "Potential", "red"),
                ("E_total", "Total",     "black"),
            ],
            title="Energy Conservation",
            ylabel="Energy (J)",
            out_path="results/conservation-graphs/energy.png"
        )
    """
    x, xlabel = get_x_axis(df)

    os.makedirs(os.path.dirname(out_path), exist_ok=True)

    plt.figure(figsize=(10, 5))
    for col, label, color in series:
        if col in df.columns:
            plt.plot(x, df[col], label=label, color=color,
                     alpha=0.85, linewidth=0.9)
        else:
            print(f"⚠️  Column '{col}' not found in CSV — skipping.")

    plt.title(title)
    plt.xlabel(xlabel)
    plt.ylabel(ylabel)
    plt.legend()
    plt.grid(True, linestyle="--", alpha=0.4)
    plt.tight_layout()
    plt.savefig(out_path, dpi=300)
    plt.close()
    print(f"✅ Saved: {out_path}")