/****************
 * File: eclipse.h
 * Author: Sinan Demir
 * Date: 11/16/2025
 *
 * Purpose:
 *    Header file defining solar eclipse shadow geometry structures
 *    and related analytic functions using 3D vector math.
 *
 *    Provides:
 *      - EclipseResult struct
 *      - computeSolarEclipse function
 *
 * Note:
 *    EclipseResult describes the umbra, penumbra, and antumbra
 *    geometry produced by the Moon's shadow on the Earth.
 *****************/

#ifndef ECLIPSE_H
#define ECLIPSE_H

#include "utils.h"
#include "vec3.h"
#include <cmath>

/****************
 * struct EclipseResult
 * Purpose: Stores solar eclipse shadow geometry for Earth.
 *****************/
struct EclipseResult {
  vec3 shadowCenter;     // shadow center on Earth's surface
  double umbraRadius;    // umbra radius at Earth (m)
  double penumbraRadius; // penumbra radius at Earth (m)
  int eclipseType;       // 0 = none, 1 = total, 2 = annular, 3 = partial
};

/****************
 * computeSolarEclipse
 * @brief: Computes Moon's shadow (umbra, penumbra, antumbra) on Earth.
 * @param: S - Sun position vector
 * @param: E - Earth position vector
 * @param: M - Moon position vector
 * @exception: none
 * @return: EclipseResult struct with shadow center, radii, and eclipse type.
 * @note: Uses analytic cone geometry; eclipseType encodes:
 *        0 = no eclipse, 1 = total, 2 = annular, 3 = partial.
 *****************/
EclipseResult computeSolarEclipse(const vec3 &S, const vec3 &E, const vec3 &M);

#endif // ECLIPSE_H
