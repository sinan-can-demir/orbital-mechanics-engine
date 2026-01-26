/***************
 * barycenter.cpp
 * Author: Sinan Demir
 * Date: 11/20/2025
 * Purpose: Implements barycenter normalization function
 ***************/

#include "barycenter.h"

namespace physics {

void normalizeToBarycenter(std::vector<CelestialBody> &bodies) {
  double totalMass = 0.0;

  // Compute total mass
  for (const auto &b : bodies)
    totalMass += b.mass;

  if (totalMass == 0.0)
    return;

  // Compute COM position & velocity
  vec3 R_cm{0, 0, 0};
  vec3 V_cm{0, 0, 0};

  for (const auto &b : bodies) {
    R_cm += b.position * b.mass;
    V_cm += b.velocity * b.mass;
  }

  R_cm /= totalMass;
  V_cm /= totalMass;

  // Shift all bodies
  for (auto &b : bodies) {
    b.position = b.position - R_cm;
    b.velocity = b.velocity - V_cm;
  }
}

} // namespace physics
