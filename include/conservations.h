/******************
 * Author: Sinan Demir
 * File: conservation.h
 * Purpose: Conservation-law diagnostics (C++)
 * Date: 11/16/2025
 *********************/

#ifndef CONSERVATIONS_H
#define CONSERVATIONS_H

#include "body.h"
#include "utils.h"
#include <array>
#include <cmath>
#include <vector>

namespace physics {

/***********************
 * struct Conservations
 * @brief: Holds total energy, linear momentum, and angular momentum for a
 * system.
 ***********************/
struct Conservations {
  // --- Energy --- //
  double kinetic_energy = 0.0;
  double potential_energy = 0.0;
  double total_energy = 0.0;

  // --- Linear Momentum --- //
  std::array<double, 3> P{0.0, 0.0, 0.0};

  // --- Angular Momentum --- //
  std::array<double, 3> L{0.0, 0.0, 0.0};
};

/***********************
 * compute (3-body overload)
 * @brief: Computes conservation quantities for Sun–Earth–Moon system.
 ***********************/
Conservations compute(const CelestialBody &sun, const CelestialBody &earth,
                      const CelestialBody &moon);

/***********************
 * compute (N-body overload)
 * @brief: Computes conservation quantities for a general N-body system.
 * @param: bodies - collection of CelestialBody instances
 * @return: Conservations struct with energy, linear momentum, and angular
 * momentum
 ***********************/
Conservations compute(const std::vector<CelestialBody> &bodies);

} // namespace physics

#endif // CONSERVATIONS_H
