/******************
 * Author: Sinan Demir
 * File: conservations.cpp
 * Purpose: Implementation of conservation-law diagnostics for the 3-body
 * simulator Date: 11/16/2025
 *********************/

#include "conservations.h"

namespace physics
{

// RECALL constants::G = 6.67430e-11 (m^3 kg^-1 s^-2)

// ---- helper: Euclidean distance (vec3) ---- //
static inline double distance(const CelestialBody& a, const CelestialBody& b)
{
    /*****************
     * Distance between two bodies
     * @param: a - first body
     * @param: b - second body
     * @return: Euclidean distance |a.position - b.position|
     * @exception: none
     * @note: Uses vec3::length() method
     *****************/
    return (a.position - b.position).length();
}

// ---- helper: r × (m v) ---- //
static inline std::array<double, 3> angular_term(const CelestialBody& b)
{
    /*****************
     * Computes angular momentum term r × (m v) for a body
     * @param: b - celestial body
     * @return: 3D vector of angular momentum contribution
     * @exception: none
     * @note: uses std::array from <array> and vec3 cross product
     *****************/
    // m v
    vec3 p = b.mass * b.velocity;
    // r × (m v)
    vec3 L = cross(b.position, p);

    return {L.x(), L.y(), L.z()};
}

/***********************
 * compute (N-body overload)
 * @brief: Computes conservation laws for an arbitrary N-body system.
 *
 *  - Kinetic energy  : sum_i (1/2 m_i |v_i|^2)
 *  - Potential energy: - sum_{i<j} G m_i m_j / r_ij
 *  - Linear momentum : sum_i m_i v_i
 *  - Angular momentum: sum_i r_i × (m_i v_i)
 ***********************/
Conservations compute(const std::vector<CelestialBody>& bodies)
{
    Conservations C;

    if (bodies.empty())
    {
        return C;
    }

    // ---- Kinetic energy and linear momentum ---- //
    for (const auto& b : bodies)
    {
        const double v2 = b.velocity.length_squared();

        // 1/2 m v^2
        C.kinetic_energy += 0.5 * b.mass * v2;

        // p = m v
        vec3 p = b.mass * b.velocity;
        C.P[0] += p.x();
        C.P[1] += p.y();
        C.P[2] += p.z();
    }

    // ---- Potential energy (pairwise: i < j) ---- //
    for (std::size_t i = 0; i < bodies.size(); ++i)
    {
        for (std::size_t j = i + 1; j < bodies.size(); ++j)
        {
            const auto& a = bodies[i];
            const auto& b = bodies[j];

            double r = distance(a, b);
            if (r == 0.0)
                continue; // avoid singularity

            C.potential_energy -= constants::G * a.mass * b.mass / r;
        }
    }

    // ---- Angular momentum ---- //
    for (const auto& b : bodies)
    {
        // r × (m v)
        vec3 p = b.mass * b.velocity;
        vec3 Lv = cross(b.position, p);

        C.L[0] += Lv.x();
        C.L[1] += Lv.y();
        C.L[2] += Lv.z();
    }

    // ---- Total energy ---- //
    C.total_energy = C.kinetic_energy + C.potential_energy;
    return C;
}

} // namespace physics
