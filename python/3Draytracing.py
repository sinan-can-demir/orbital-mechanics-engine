"""
3D Raytracing Prototype (Earth–Moon Zoom)
Author: Sinan Demir
Date: 11/16/2025

Upgraded Version:
- Loads BOTH orbit and eclipse CSVs
- Merges them on step
- Renders a 3D Earth sphere
- Projects umbra / penumbra onto Earth's surface
- EARTH RADIUS IS EXAGGERATED FOR VISUAL CLARITY
"""

import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from mpl_toolkits.mplot3d import Axes3D  # noqa: F401
import numpy as np
import shutil
from pathlib import Path

# ============================
# Load orbit and eclipse data
# ============================
orbit = pd.read_csv("build/orbit_three_body.csv")
eclipse = pd.read_csv("build/eclipse_log.csv")

df = orbit.merge(eclipse, on="step", how="left")
steps = len(df)

# ----------------------------------------------------------
# Real Earth radius
R_EARTH = 6.371e6
# EXAGGERATION FACTOR (visualization only!)
EARTH_SCALE = 4.0
R_EARTH_DRAW = R_EARTH * EARTH_SCALE
# ----------------------------------------------------------

# ---- Earth-Moon zoom scale (tightly around Earth) ----
XR = 1.2 * R_EARTH_DRAW
ZR = 1.2 * R_EARTH_DRAW

# Exaggerate shadow radius for visibility (visualization only!)
SHADOW_SCALE = 3.0

def eclipse_type_to_str(et: int) -> str:
    return {
        1: "Total eclipse (umbra)",
        2: "Annular eclipse (antumbra)",
        3: "Partial eclipse (penumbra)",
    }.get(et, "No eclipse")


def circle_on_plane(center, axis, radius, n=120):
    axis = np.asarray(axis, float)
    norm = np.linalg.norm(axis)
    if norm == 0 or radius <= 0:
        return np.array([]), np.array([]), np.array([])

    axis /= norm

    if abs(axis[0]) < 0.9:
        ref = np.array([1, 0, 0])
    else:
        ref = np.array([0, 1, 0])

    v1 = np.cross(axis, ref)
    v1 /= np.linalg.norm(v1)
    v2 = np.cross(axis, v1)

    theta = np.linspace(0, 2 * np.pi, n)
    pts = (
        center.reshape(3, 1)
        + radius
        * (
            v1.reshape(3, 1) * np.cos(theta)
            + v2.reshape(3, 1) * np.sin(theta)
        )
    )
    return pts[0], pts[1], pts[2]


# ---------------------- Figure Setup ----------------------
fig = plt.figure(figsize=(8, 8))
ax = fig.add_subplot(111, projection="3d")

ax.set_title("Earth–Moon System (Exaggerated Earth) with Solar Ray Tracing")
ax.set_xlabel("x (m)")
ax.set_ylabel("y (m)")
ax.set_zlabel("z (m)")

# ---- 3D Earth Sphere (EXAGGERATED) ----
u_steps = 40
v_steps = 20
u = np.linspace(0, 2 * np.pi, u_steps)
v = np.linspace(0, np.pi, v_steps)
uu, vv = np.meshgrid(u, v)

X_sphere = R_EARTH_DRAW * np.cos(uu) * np.sin(vv)
Y_sphere = R_EARTH_DRAW * np.sin(uu) * np.sin(vv)
Z_sphere = R_EARTH_DRAW * np.cos(vv)

base_color = np.array([0.2, 0.5, 1.0, 1.0])
facecolors = np.tile(
    base_color,
    (X_sphere.shape[0] - 1) * (X_sphere.shape[1] - 1)
).reshape(X_sphere.shape[0] - 1, X_sphere.shape[1] - 1, 4)

earth_surf = ax.plot_surface(
    X_sphere, Y_sphere, Z_sphere,
    rstride=1, cstride=1,
    facecolors=facecolors,
    linewidth=0,
    antialiased=True,
)

earth_dot, = ax.plot([], [], [], "bo", markersize=3, label="Earth center")
moon_dot, = ax.plot([], [], [], "ko", markersize=4, label="Moon")
shadow_dot, = ax.plot([], [], [], "ro", markersize=5)

ray_sm_line, = ax.plot([], [], [], color="orange", lw=1, alpha=0.7)
shadow_axis_line, = ax.plot([], [], [], color="black", lw=1, alpha=0.7)
umbra_line, = ax.plot([], [], [], color="black", lw=1.0)
penumbra_line, = ax.plot([], [], [], color="red", lw=0.9)

eclipse_text = ax.text2D(0.05, 0.95, "", transform=ax.transAxes)


# ----------------------------------------------------------
# Update Function
# ----------------------------------------------------------
def update(i):
    xe, ye, ze = df.at[i, "x_Earth"], df.at[i, "y_Earth"], df.at[i, "z_Earth"]
    xm, ym, zm = df.at[i, "x_Moon"], df.at[i, "y_Moon"], df.at[i, "z_Moon"]
    xs, ys, zs = df.at[i, "x_Sun"],  df.at[i, "y_Sun"],  df.at[i, "z_Sun"]

    sx = df.at[i, "shadow_x"]
    sy = df.at[i, "shadow_y"]
    sz = df.at[i, "shadow_z"]

    umbra_r = df.at[i, "umbraRadius"]
    penumbra_r = df.at[i, "penumbraRadius"]
    eclipse_type = int(df.at[i, "eclipseType"])

    # exaggerate shadow radii for visibility
    umbra_r *= SHADOW_SCALE
    penumbra_r *= SHADOW_SCALE

    E = np.array([xe, ye, ze])
    M = np.array([xm, ym, zm]) - E
    S = np.array([xs, ys, zs]) - E
    shadow_center = np.array([sx, sy, sz]) - E

    earth_dot.set_data([0], [0])
    earth_dot.set_3d_properties([0])

    moon_dot.set_data([M[0]], [M[1]])
    moon_dot.set_3d_properties([M[2]])

    shadow_dot.set_data([shadow_center[0]], [shadow_center[1]])
    shadow_dot.set_3d_properties([shadow_center[2]])

    sm = np.vstack([S, M])
    ray_sm_line.set_data(sm[:, 0], sm[:, 1])
    ray_sm_line.set_3d_properties(sm[:, 2])

    me_vec = -M
    L = np.linalg.norm(me_vec)

    if L > 0:
        u_axis = me_vec / L
        ts = np.linspace(0, L + 2 * R_EARTH, 60)
        axis = M.reshape(3, 1) + u_axis.reshape(3, 1) * ts
        shadow_axis_line.set_data(axis[0], axis[1])
        shadow_axis_line.set_3d_properties(axis[2])
    else:
        u_axis = np.array([0, 0, 1])

    # Shadow cross-sections (reference only)
    if L > 0:
        if umbra_r > 0:
            ux, uy, uz = circle_on_plane(shadow_center, me_vec, umbra_r)
            umbra_line.set_data(ux, uy)
            umbra_line.set_3d_properties(uz)
        else:
            umbra_line.set_data([], [])
            umbra_line.set_3d_properties([])

        if penumbra_r > 0:
            px, py, pz = circle_on_plane(shadow_center, me_vec, penumbra_r)
            penumbra_line.set_data(px, py)
            penumbra_line.set_3d_properties(pz)
        else:
            penumbra_line.set_data([], [])
            penumbra_line.set_3d_properties([])

    # Shadow shading on Earth surface
    Xc = 0.25 * (X_sphere[:-1, :-1] + X_sphere[1:, :-1] +
                 X_sphere[:-1, 1:] + X_sphere[1:, 1:])
    Yc = 0.25 * (Y_sphere[:-1, :-1] + Y_sphere[1:, :-1] +
                 Y_sphere[:-1, 1:] + Y_sphere[1:, 1:])
    Zc = 0.25 * (Z_sphere[:-1, :-1] + Z_sphere[1:, :-1] +
                 Z_sphere[:-1, 1:] + Z_sphere[1:, 1:])

    P = np.stack([Xc, Yc, Zc], axis=-1)
    d = P - shadow_center
    dot = np.einsum("ijk,k->ij", d, u_axis)
    d_perp = d - dot[..., None] * u_axis
    dist = np.linalg.norm(d_perp, axis=-1)

    # Simple Lambert lighting: light from Sun direction
    normals = P / np.linalg.norm(P, axis=-1, keepdims=True)
    if np.linalg.norm(S) > 0:
        light_dir = -S / np.linalg.norm(S)
    else:
        light_dir = np.array([0.0, 0.0, 1.0])
    lambert = np.clip(np.einsum("ijk,k->ij", normals, light_dir), 0.2, 1.0)

    colors = np.zeros(P.shape[:2] + (4,))
    # apply lighting to base RGB, alpha stays 1
    colors[..., :3] = base_color[:3] * lambert[..., None]
    colors[..., 3] = 1.0

    in_umbra = dist <= max(umbra_r, 0)
    colors[in_umbra] = np.array([0.05, 0.05, 0.1, 1.0])

    in_penumbra = (dist <= penumbra_r) & (~in_umbra)
    colors[in_penumbra] = np.array([0.1, 0.2, 0.4, 1.0])

    earth_surf.set_facecolors(colors.reshape([-1, 4]))

    ax.set_xlim(-XR, XR)
    ax.set_ylim(-XR, XR)
    ax.set_zlim(-ZR, ZR)

    eclipse_text.set_text(eclipse_type_to_str(eclipse_type))
    return []


ani = FuncAnimation(
    fig, update,
    frames=range(0, steps, 20),
    interval=20,
    blit=False,
    repeat=True
)

# Save
script_dir = Path(__file__).resolve().parent
project_root = script_dir.parent
results_dir = project_root / "results/ray_tracing"
results_dir.mkdir(parents=True, exist_ok=True)

path = "3d_raytracing_EXAGGERATED"
mp4_path = results_dir / f"{path}.mp4"
gif_path = results_dir / f"{path}.gif"

print(f"Saving to: {results_dir}")
try:
#    if shutil.which("ffmpeg"):
#        ani.save(mp4_path, writer="ffmpeg", fps=30)
#        print(f"✅ Animation saved as {mp4_path}")
    if shutil.which("magick") or shutil.which("convert"):
        ani.save(gif_path, writer="imagemagick", fps=30)
        print(f"✅ Animation saved as {gif_path}")
    else:
        print("⚠️ ffmpeg or ImageMagick not found — showing animation only.")
except Exception as e:
    print(f"⚠️ Could not save animation: {e}")

plt.show()
