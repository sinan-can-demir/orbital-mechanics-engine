"""
File: 3Dplot_earth_moon.py
Purpose: 3D visualization of Earth–Moon system (Earth-centered frame)
Author: Sinan Demir
Date: 11/15/2025
"""

import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from mpl_toolkits.mplot3d import Axes3D
from pathlib import Path
import shutil
import numpy as np

# --- Load simulation data ---
df = pd.read_csv("build/orbit_three_body.csv")
steps = len(df)

# Convert to Earth-centered coordinates
mx = df["x_moon"] - df["x_earth"]
my = df["y_moon"] - df["y_earth"]
mz = df["z_moon"] - df["z_earth"]

# --- Create 3D figure ---
fig = plt.figure(figsize=(8, 8))
ax = fig.add_subplot(111, projection="3d")
ax.set_title("Moon Orbiting the Earth (3D, Earth-Centered)")
ax.set_xlabel("x (m)")
ax.set_ylabel("y (m)")
ax.set_zlabel("z (m)")
ax.grid(True, linestyle="--", alpha=0.4)

# --- Static Moon orbit trail ---
ax.plot(mx, my, mz, color="gray", alpha=0.4, lw=1.0, label="Moon orbit")

# --- Earth (always at origin) ---
earth_dot = ax.scatter([0], [0], [0], color="blue", s=80, label="Earth")

# --- Initial Moon marker ---
moon_dot, = ax.plot([], [], [], "o", color="black", markersize=5)

ax.legend()

# --- Set equal aspect ratio ---
max_radius = np.max(np.sqrt(mx**2 + my**2 + mz**2))
ax.set_xlim(-max_radius, max_radius)
ax.set_ylim(-max_radius, max_radius)
ax.set_zlim(-max_radius, max_radius)
ax.set_box_aspect([1, 1, 1])

# --- Update function ---
def update(frame):
    moon_dot.set_data([mx[frame]], [my[frame]])
    moon_dot.set_3d_properties([mz[frame]])
    return moon_dot,

# --- Animation ---
ani = FuncAnimation(
    fig,
    update,
    frames=np.arange(0, steps, 5),
    interval=20,
    blit=False,
    repeat=True,
)

# --- Save animation to results folder ---
results_dir = Path(__file__).resolve().parent / "results/orbits"
results_dir.mkdir(exist_ok=True)

path = "earth_moon_3d"
mp4_path = results_dir / f"{path}.mp4"
gif_path = results_dir / f"{path}.gif"

try:
    if shutil.which("ffmpeg"):
        ani.save(mp4_path, writer="ffmpeg", fps=30)
        print(f"✅ Animation saved as {mp4_path}")
    elif shutil.which("magick") or shutil.which("convert"):
        ani.save(gif_path, writer="imagemagick", fps=30)
        print(f"✅ Animation saved as {gif_path}")
    else:
        print("⚠️ ffmpeg or ImageMagick not found — showing animation only.")
except Exception as e:
    print(f"⚠️ Could not save animation: {e}")

plt.show()
