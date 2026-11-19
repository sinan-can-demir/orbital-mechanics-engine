/****************
 * Author: Sinan Demir
 * File: constants.h
 * Date: 10/31/2025
 * Updated: 2026-06-13 — renamed from utils.h, removed dead constants
 * Purpose: Physical constants used by the simulation engine.
 *
 * Sources:
 *   G        — CODATA 2018 (NIST)
 *   R_SUN    — IAU 2015 nominal solar radius
 *   R_EARTH  — IAU 2015 nominal Earth equatorial radius
 *   R_MOON   — NASA fact sheet mean radius
 *****************/
#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <cmath>

namespace physics
{
namespace constants
{

// ==============================
// Fundamental constants
// ==============================
constexpr double G = 6.67430e-11;       // gravitational constant (m^3 kg^-1 s^-2)
constexpr double C_LIGHT = 299792458.0; // speed of light (m/s, exact SI definition)

// Reference values for system files and notebooks (JSON cannot include this header).
// AU: IAU 2012 Resolution B2 (exact). GM_SUN: JPL DE440 (gm_de431.tpc BODY10_GM);
// the IAU 2015 nominal value is 1.3271244e20. M_SUN follows from the engine's G so that
// G * M_SUN reproduces DE440 exactly (systems/*.json are checked against this).
constexpr double AU = 149597870700.0;            // m
constexpr double GM_SUN = 1.3271244004193938e20; // m^3 s^-2
constexpr double M_SUN = GM_SUN / G;             // kg (= 1.988409871326421e30 for G = 6.67430e-11)

// ==============================
// Body radii  (used by eclipse.cpp)
// ==============================
constexpr double R_SUN = 6.957e8;   // m
constexpr double R_EARTH = 6.371e6; // m
constexpr double R_MOON = 1.737e6;  // m

} // namespace constants
} // namespace physics

#endif // CONSTANTS_H
