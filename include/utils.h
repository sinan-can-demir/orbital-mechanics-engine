/****************
 * Author: Sinan Demir
 * File: utils.h
 * Date: 10/31/2025
 * Purpose: Defines physical constants in constants namespace
 *****************/
#ifndef UTILS_H
#define UTILS_H

#include <cmath>

namespace physics {
namespace constants {

// ==============================
// Fundamental constants
// ==============================
constexpr double G = 6.67430e-11; // gravitational constant (m^3 kg^-1 s^-2)
constexpr double DT = 3600;       // one hour timestep (s)

// ==============================
// Masses
// ==============================
constexpr double M_SUN = 1.9891e30;  // kg
constexpr double M_EARTH = 5.972e24; // kg
constexpr double M_MOON = 7.3477e22; // kg

// ==============================
// Radii
// ==============================
constexpr double R_SUN = 6.957e8;   // m
constexpr double R_EARTH = 6.371e6; // m
constexpr double R_MOON = 1.737e6;  // m

// ==============================
// Orbital
// ==============================
constexpr double MOON_INCLINATION = 5.145 * (M_PI / 180.0); // radians
constexpr double AU = 1.495978707e11; // astronomical unit (m)

// ==============================
// Orbital distances
// ==============================
constexpr double EARTH_PERIHELION = 1.47098074e11; // m
constexpr double EARTH_APHELION = 1.521e11;        // m
constexpr double MOON_ORBIT_RADIUS = 384400000.0;  // m

}; // namespace constants
} // namespace physics
#endif // UTILS_H
