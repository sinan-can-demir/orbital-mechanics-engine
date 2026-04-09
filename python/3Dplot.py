"""
File: 3Dplot.py
Purpose: 3D visualization of Sun–Earth–Moon system
Author: Sinan Demir
Date: 11/14/2025
"""

import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from mpl_toolkits.mplot3d import Axes3D  # needed for 3D projection side effects
import shutil
from pathlib import Path
import numpy as np

# --- Load simulation data ---
df = pd.read_csv("build/orbit_three_body.csv")
steps = len(df)

# --- Define set axis equal function ---
def set_axes_equal(ax):
    """
    Make axes of 3D plot have equal scale so that the units are the same
    in x, y, z. This prevents distorted 3D views.
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


# --- Create 3D figure and axes ---
fig = plt.figure(figsize=(8, 8))
ax = fig.add_subplot(111, projection="3d")
ax.set_title("Earth & Moon Orbiting the Sun (3D)")
ax.set_xlabel("x (m)")
ax.set_ylabel("y (m)")
ax.set_zlabel("z (m)")
ax.grid(True, linestyle="--", alpha=0.4)

# --- Static orbital trails (faint background) ---
ax.plot(df["x_earth"], df["y_earth"], df["z_earth"],
        color="blue", alpha=0.3, lw=1, label="Earth orbit")
ax.plot(df["x_moon"], df["y_moon"], df["z_moon"],
        color="gray", alpha=0.3, lw=0.8, label="Moon orbit")

# Sun at origin (in this model)
ax.scatter(df["x_sun"].iloc[0],
           df["y_sun"].iloc[0],
           df["z_sun"].iloc[0],
           color="gold", marker="*", s=120, label="Sun")

ax.legend()

# --- Set 3D bounds (roughly around Earth's orbit) ---
R = 1.6e11  # a bit larger than Earth's orbital radius
ax.set_xlim(-R, R)
ax.set_ylim(-R, R)
ax.set_zlim(-R / 6, R / 6)  # z is much smaller than x,y

# --- Set correct axis limits before equalizing ---
R = 1.6e11    # Earth orbit scale
ZMAX = 3.5e7  # maximum moon inclination (~35,000 km)

ax.set_xlim(-R, R)
ax.set_ylim(-R, R)
ax.set_zlim(-ZMAX, ZMAX)

# --- Call set_axes_equal function ---
set_axes_equal(ax)

# --- Initialize moving points ---
earth_dot, = ax.plot([], [], [], "bo", markersize=6)          # Earth marker
moon_dot,  = ax.plot([], [], [], "o", color="gray", markersize=4)  # Moon marker

# --- Update function ---
def update(frame):
    # Earth position at this frame
    xe = df.at[frame, "x_earth"]
    ye = df.at[frame, "y_earth"]
    ze = df.at[frame, "z_earth"]

    # Moon position at this frame
    xm = df.at[frame, "x_moon"]
    ym = df.at[frame, "y_moon"]
    zm = df.at[frame, "z_moon"]

    # Update Earth marker
    earth_dot.set_data([xe], [ye])          # x, y
    earth_dot.set_3d_properties([ze])      # z

    # Update Moon marker
    moon_dot.set_data([xm], [ym])          # x, y
    moon_dot.set_3d_properties([zm])      # z

    return earth_dot, moon_dot

# --- Run animation ---
ani = FuncAnimation(
    fig,
    update,
    frames=np.arange(0, steps, 20),  # skip frames for speed
    interval=20,
    blit=False,
    repeat=True,
)

# --- Save animation to results folder ---
results_dir = Path(__file__).resolve().parent / "results/orbits"
results_dir.mkdir(exist_ok=True)

path = "orbit_3d"
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
