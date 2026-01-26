/****************
 * File: eclipse.cpp
 * Author: Sinan Demir
 * Date: 11/16/2025
 *
 * Purpose:
 *    Implements analytic solar eclipse geometry:
 *      - Umbra radius
 *      - Penumbra radius
 *      - Antumbra condition
 *      - Shadow center on Earth's surface
 *****************/

#include "eclipse.h"
#include "utils.h" // for constants::R_SUN etc.

/****************
 * computeSolarEclipse
 * @brief: Computes Moon's shadow geometry using analytic cone equations.
 * @param: S - Sun position vector
 * @param: E - Earth position vector
 * @param: M - Moon position vector
 * @exception none
 * @return: EclipseResult struct
 *****************/
EclipseResult computeSolarEclipse(const vec3 &S, const vec3 &E, const vec3 &M) {

  // --------------------------
  // Compute vectors
  // --------------------------
  vec3 ME = E - M; // Moon -> Earth
  vec3 SM = M - S; // Sun  -> Moon

  double d_em = ME.length(); // Earth->Moon distance
  double d_sm = SM.length(); // Sun->Moon distance

  if (d_em <= 0.0 || d_sm <= 0.0) {
    return {E, 0.0, 0.0, 0};
  }

  // --------------------------
  // Physical constants
  // --------------------------
  const double R_SUN = physics::constants::R_SUN;
  const double R_EARTH = physics::constants::R_EARTH;
  const double R_MOON = physics::constants::R_MOON;

  // --------------------------
  // Umbra & Penumbra lengths
  // --------------------------
  double L_u = (R_MOON * d_sm) / (R_SUN - R_MOON);
  double L_p = (R_MOON * d_sm) / (R_SUN + R_MOON);

  // --------------------------
  // Radii of shadow at Earth
  // --------------------------
  double umbraRadius = R_MOON * (1.0 - d_em / L_u);
  double penumbraRadius = R_MOON * (1.0 + d_em / L_p);

  // --------------------------
  // Shadow center direction
  // --------------------------
  vec3 u = unit_vector(ME);
  vec3 shadowCenter = E - u * R_EARTH;

  // --------------------------
  // Classify eclipse type
  // --------------------------
  int eclipseType = 0;

  if (umbraRadius > R_EARTH) {
    eclipseType = 1; // total
  } else if (umbraRadius < 0.0 && penumbraRadius > R_EARTH) {
    eclipseType = 2; // annular
  } else if (penumbraRadius > 0.0) {
    eclipseType = 3; // partial
  }

  return {shadowCenter, umbraRadius, penumbraRadius, eclipseType};
}
