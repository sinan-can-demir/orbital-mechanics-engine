# Physics and Numerical Methods

*Comprehensive documentation of the physical model, governing equations, and numerical integration methods used in the orbital dynamics simulation.*

---

## Introduction

This document describes the theoretical foundation of the Earth-Moon orbital dynamics simulation. The simulation implements a Newtonian N-body gravitational system with fourth-order Runge-Kutta integration, designed for high-accuracy orbital propagation and analysis.

The target audience includes advanced undergraduate and early graduate students in aerospace engineering, physics, and computational science who require a rigorous understanding of the underlying physical and numerical methods.

---

## Governing Equations

### Newtonian N-Body Gravity

The fundamental equation governing the system is Newton's law of universal gravitation:

$$
\vec{F}_{ij} = -G \frac{m_i m_j}{|\vec{r}_{ij}|^3} \vec{r}_{ij}
$$

where:
- $\vec{F}_{ij}$ is the gravitational force on body $i$ due to body $j$
- $G = 6.67430 \times 10^{-11}$ m³ kg⁻¹ s⁻² is the gravitational constant
- $m_i, m_j$ are the masses of bodies $i$ and $j$
- $\vec{r}_{ij} = \vec{r}_i - \vec{r}_j$ is the position vector from body $j$ to body $i$

The total acceleration of body $i$ is the sum of contributions from all other bodies:

$$
\vec{a}_i = \sum_{j \neq i} -G \frac{m_j}{|\vec{r}_{ij}|^3} \vec{r}_{ij}
$$

### Equations of Motion

For each body in the system, we solve the coupled second-order differential equations:

$$
\frac{d^2\vec{r}_i}{dt^2} = \vec{a}_i(\vec{r}_1, \vec{r}_2, \ldots, \vec{r}_N)
$$

These are reformulated as a system of first-order equations for numerical integration:

$$
\frac{d\vec{r}_i}{dt} = \vec{v}_i
$$
$$
\frac{d\vec{v}_i}{dt} = \vec{a}_i
$$

---

## Reference Frames and Coordinates

### Inertial Reference Frame

The simulation uses an inertial, barycentric reference frame with the system's center of mass as the origin. This choice ensures:

- Conservation of linear momentum in the absence of external forces
- Symmetric treatment of all bodies
- Simplified transformation to other reference frames

### Barycenter Definition

The system barycenter (center of mass) is computed as:

$$
\vec{r}_{CM} = \frac{\sum_{i=1}^{N} m_i \vec{r}_i}{\sum_{i=1}^{N} m_i}
$$

The barycenter velocity is:

$$
\vec{v}_{CM} = \frac{\sum_{i=1}^{N} m_i \vec{v}_i}{\sum_{i=1}^{N} m_i}
$$

For an isolated system, both $\vec{r}_{CM}$ and $\vec{v}_{CM}$ remain constant.

### Coordinate System

The simulation uses a right-handed Cartesian coordinate system:
- **X-axis**: Points toward the reference direction (typically vernal equinox)
- **Y-axis**: Completes right-handed system in orbital plane
- **Z-axis**: Normal to orbital plane (positive following right-hand rule)

Units are standardized to SI:
- Position: meters (m)
- Velocity: meters per second (m/s)
- Mass: kilograms (kg)
- Time: seconds (s)

---

## State Vector Formulation

### Complete System State

The complete state of the N-body system at time $t$ is represented by the concatenated state vector:

$$
\mathbf{y}(t) = \begin{bmatrix}
\vec{r}_1(t) \\
\vec{v}_1(t) \\
\vec{r}_2(t) \\
\vec{v}_2(t) \\
\vdots \\
\vec{r}_N(t) \\
\vec{v}_N(t)
\end{bmatrix} \in \mathbb{R}^{6N}
$$

### State Space Equations

The system evolution follows:

$$
\frac{d\mathbf{y}}{dt} = \mathbf{f}(\mathbf{y}, t)
$$

where the function $\mathbf{f}$ computes all accelerations based on current positions:

$$
\mathbf{f}(\mathbf{y}, t) = \begin{bmatrix}
\vec{v}_1 \\
\vec{a}_1(\vec{r}_1, \ldots, \vec{r}_N) \\
\vec{v}_2 \\
\vec{a}_2(\vec{r}_1, \ldots, \vec{r}_N) \\
\vdots \\
\vec{v}_N \\
\vec{a}_N(\vec{r}_1, \ldots, \vec{r}_N)
\end{bmatrix}
$$

---

## Numerical Integration (RK4)

### Fourth-Order Runge-Kutta Method

The simulation employs the classic fourth-order Runge-Kutta (RK4) method for time integration. For a state vector $\mathbf{y}$ at time $t_n$ with time step $\Delta t$:

$$
\mathbf{y}_{n+1} = \mathbf{y}_n + \frac{\Delta t}{6}(k_1 + 2k_2 + 2k_3 + k_4)
$$

where the stage derivatives are:

$$
k_1 = \mathbf{f}(\mathbf{y}_n, t_n)
$$
$$
k_2 = \mathbf{f}\left(\mathbf{y}_n + \frac{\Delta t}{2}k_1, t_n + \frac{\Delta t}{2}\right)
$$
$$
k_3 = \mathbf{f}\left(\mathbf{y}_n + \frac{\Delta t}{2}k_2, t_n + \frac{\Delta t}{2}\right)
$$
$$
k_4 = \mathbf{f}\left(\mathbf{y}_n + \Delta t k_3, t_n + \Delta t\right)
$$

### Accuracy and Stability

**Local truncation error**: $O(\Delta t^5)$  
**Global error**: $O(\Delta t^4)$

The RK4 method offers excellent accuracy for orbital dynamics with appropriate time step selection. For typical Earth-Moon simulations:

- **Recommended time step**: 60-600 seconds
- **Stability constraint**: $\Delta t < 0.1 \times T_{min}$ where $T_{min}$ is the shortest orbital period
- **Energy conservation**: Typically better than $10^{-6}$ relative error per orbit

### Implementation Considerations

The RK4 implementation requires:
- Four complete force evaluations per time step
- Consistent computation of all pairwise interactions
- Careful handling of the singular case when bodies approach closely
- Efficient vectorization for N-body force calculations

---

## Conservation Law Diagnostics

### Energy Conservation

The total mechanical energy of the system is:

$$
E = K + U = \sum_{i=1}^{N} \frac{1}{2}m_i|\vec{v}_i|^2 - \sum_{i=1}^{N}\sum_{j>i}^{N} \frac{Gm_i m_j}{|\vec{r}_{ij}|}
$$

Energy conservation is monitored via the relative error:

$$
\epsilon_E = \left|\frac{E(t) - E(0)}{E(0)}\right|
$$

### Linear Momentum Conservation

Total linear momentum:

$$
\vec{P} = \sum_{i=1}^{N} m_i \vec{v}_i
$$

For an isolated system, $\vec{P}$ should remain constant. Deviations indicate numerical errors or external perturbations.

### Angular Momentum Conservation

Total angular momentum about the origin:

$$
\vec{L} = \sum_{i=1}^{N} m_i (\vec{r}_i \times \vec{v}_i)
$$

Angular momentum conservation is particularly sensitive to integration errors and provides a stringent test of numerical accuracy.

---

## Eclipse Detection Geometry

### Geometric Model

Eclipse detection uses a simplified geometric model based on umbra and penumbra cone intersections. The model assumes:

- Spherical bodies with known radii
- Point light source (the Sun)
- No atmospheric effects
- No light-time corrections

### Umbra and Penumbra Cones

For a body of radius $R_s$ (shadow caster) at distance $d$ from a light source of radius $R_l$:

**Umbra cone angle**:
$$
\theta_u = \arcsin\left(\frac{R_l - R_s}{d}\right)
$$

**Penumbra cone angle**:
$$
\theta_p = \arcsin\left(\frac{R_l + R_s}{d}\right)
$$

### Eclipse Conditions

An eclipse occurs when:
1. The shadowed body lies within the shadow cone
2. The angular separation satisfies geometric constraints
3. The bodies are properly ordered (light source → caster → shadowed)

The detection algorithm tests these conditions using vector dot products and distance comparisons.

---

## Assumptions and Limitations

### Physical Model Assumptions

1. **Newtonian gravity only**: No relativistic corrections
2. **Point masses**: No consideration of body size or shape
3. **No non-gravitational forces**: Solar radiation pressure, atmospheric drag ignored
4. **Vacuum environment**: No medium effects

### Numerical Limitations

1. **Fixed time step**: No adaptive error control
2. **Floating-point precision**: Limited by double-precision arithmetic
3. **Singular behavior**: No special handling for close approaches or collisions
4. **Long-term drift**: Energy and angular momentum slowly degrade over extended integrations

### Eclipse Model Limitations

1. **Simplified geometry**: No atmospheric refraction or scattering
2. **No light-time correction**: Assumes instantaneous light propagation
3. **No penumbral shading**: Binary eclipse detection only
4. **No surface features**: Uniform disk approximation

---

## References

1. **Goldstein, H.** *Classical Mechanics*, 3rd ed. Addison-Wesley, 2002.
2. **Danby, J.M.A.** *Fundamentals of Celestial Mechanics*. Willmann-Bloch, 1992.
3. **Murray, C.D., and Dermott, S.F.** *Solar System Dynamics*. Cambridge University Press, 1999.
4. **Hairer, E., Nørsett, S.P., and Wanner, G.** *Solving Ordinary Differential Equations I*. Springer, 1993.
5. **Vallado, D.A.** *Fundamentals of Astrodynamics and Applications*. 4th ed. Microcosm Press, 2013.

---