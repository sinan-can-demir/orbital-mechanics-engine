/****************
 * Author: Sinan Demir
 * File: body.h
 * Date: 10/31/2025
 * Purpose: Defines CelestialBody ADT
 *****************/

#ifndef BODY_H
#define BODY_H

#include "vec3.h"
#include <string>

/***********************
 * struct CelestialBody
 * @brief: Represents a gravitational body in the simulation.
 *         Stores mass, name, position, velocity, acceleration.
 ***********************/
struct CelestialBody
{
    std::string name; ///< Body name (Sun, Earth, Moon, etc.)
    double mass;      ///< Mass (kg)

    vec3 position;     ///< Position vector (m)
    vec3 velocity;     ///< Velocity vector (m/s)
    vec3 acceleration; ///< Acceleration vector (m/s^2)

    CelestialBody(const std::string& name_, double mass_, double x, double y, double z, double vx,
                  double vy, double vz, double ax = 0.0, double ay = 0.0, double az = 0.0)
        : name(name_), mass(mass_), position(x, y, z), velocity(vx, vy, vz),
          acceleration(ax, ay, az)
    {
    }
};

#endif // BODY_H