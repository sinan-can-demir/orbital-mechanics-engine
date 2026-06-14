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
constexpr double G = 6.67430e-11; // gravitational constant (m^3 kg^-1 s^-2)

// ==============================
// Body radii  (used by eclipse.cpp)
// ==============================
constexpr double R_SUN   = 6.957e8;  // m
constexpr double R_EARTH = 6.371e6;  // m
constexpr double R_MOON  = 1.737e6;  // m

} // namespace constants
} // namespace physics

#endif // CONSTANTS_H
