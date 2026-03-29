/**********
 * barycenter.h
 * Author: Sinan Demir
 * Date: 11/20/2025
 * Purpose: Declares barycenter normalization function
 ***********/

#pragma once
#include "body.h"
#include "vec3.h"
#include <vector>

namespace physics
{

void normalizeToBarycenter(std::vector<CelestialBody>& bodies);

} // end namespace physics
