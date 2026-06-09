"""
File: 3Dexaggerated_plot.py
Purpose: 3D visualization with exaggerated z-axis to show Moon's orbital tilt
Author: Sinan Demir
Date: 11/14/2025
"""

import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from mpl_toolkits.mplot3d import Axes3D
import numpy as np
from pathlib import Path
import shutil

# Load data
df = pd.read_csv("build/orbit_three_body.csv")
steps = len(df)

# Exaggeration factor for Z axis
Z_SCALE = 1000  # multiply z by this for visibility

def set_axes_equal(ax):
    """
    @brief: Equal 3D axis scaling.
    @param: axis
    """
    x_limits = ax.get_xlim3d()
    y_limits = ax.get_ylim3d()
    z_limits = ax.get_zlim3d()

    x_range = abs(x_limits[1] - x_limits[0])
    y_range = abs(y_limits[1] - y_limits[0])
    z_range = abs(z_limits[1] - z_limits[0])

    max_range = max([x_range, y_range, z_range]) / 2.0

    mid_x = np.mean(x_limits)
    mid_y = np.mean(y_limits)
    mid_z = np.mean(z_limits)

    ax.set_xlim3d(mid_x - max_range, mid_x + max_range)
    ax.set_ylim3d(mid_y - max_range, mid_y + max_range)
    ax.set_zlim3d(mid_z - max_range, mid_z + max_range)

# Create 3D figure
fig = plt.figure(figsize=(8, 8))
ax = fig.add_subplot(111, projection="3d")
ax.set_title("Sun–Earth–Moon (Z Exaggerated)")
ax.set_xlabel("x (m)")
ax.set_ylabel("y (m)")
ax.set_zlabel("z (exaggerated m)")

# Static trails (z multiplied)
ax.plot(df["x_earth"], df["y_earth"], df["z_earth"] * Z_SCALE,
        color="blue", alpha=0.4)
ax.plot(df["x_moon"], df["y_moon"], df["z_moon"] * Z_SCALE,
        color="gray", alpha=0.4)

# Sun (no z scaling needed)
ax.scatter(df["x_sun"][0], df["y_sun"][0], df["z_sun"][0],
           color="gold", marker="*", s=120)

# Set limits
R = 1.6e11
ax.set_xlim(-R, R)
ax.set_ylim(-R, R)
ax.set_zlim(-R/6, R/6)

set_axes_equal(ax)

# Moving points
earth_dot, = ax.plot([], [], [], "bo", markersize=6)
moon_dot,  = ax.plot([], [], [], "o", color="gray", markersize=4)

# Update function
def update(frame):
    xe, ye, ze = df.loc[frame, ["x_earth", "y_earth", "z_earth"]]
    xm, ym, zm = df.loc[frame, ["x_moon", "y_moon", "z_moon"]]

    earth_dot.set_data([xe], [ye])
    earth_dot.set_3d_properties([ze * Z_SCALE])

    moon_dot.set_data([xm], [ym])
    moon_dot.set_3d_properties([zm * Z_SCALE])

    return earth_dot, moon_dot

# Animate
ani = FuncAnimation(fig, update, frames=np.arange(0, steps, 20),
                    interval=20, repeat=True)

# Save
results_dir = Path(__file__).resolve().parent / "results/orbits"
results_dir.mkdir(exist_ok=True)

path = "orbit_3d_exaggerated"
mp4_path = results_dir / f"{path}.mp4"
gif_path = results_dir / f"{path}.gif"

try:
    if shutil.which("ffmpeg"):
        ani.save(mp4_path, writer="ffmpeg", fps=30)
        print(f"✅ Animation saved as {mp4_path}")
    if shutil.which("magick") or shutil.which("convert"):
        ani.save(gif_path, writer="imagemagick", fps=30)
        print(f"✅ Animation saved as {gif_path}")
    else:
        print("⚠️ ffmpeg or ImageMagick not found — showing animation only.")
except Exception as e:
    print(f"⚠️ Could not save animation: {e}")

plt.show()
