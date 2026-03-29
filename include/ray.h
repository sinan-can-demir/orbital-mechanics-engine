/************************************************************************************
 * Source: https://raytracing.github.io/books/RayTracingInOneWeekend.html
 * Author: Peter Shirley, Trevor David Black, Steve Hollasch
 * Modified by: Sinan Demir
 * File: ray.h
 * Date modified: 11/16/2025
 *
 * Description:
 *   Header file defining a 3D ray class (`ray`) used in ray tracing.
 *   A ray is defined by an origin point and a direction vector.
 *   Rays are fundamental in ray tracing for calculating intersections
 *   with geometric objects like spheres, planes, and meshes.
 * *************************************/

#ifndef RAY_H
#define RAY_H

#include "vec3.h"

/***********************
 * class ray
 * @brief Represents a 3D ray with an origin point and a direction vector.
 *
 * Rays are defined by the parametric equation:
 *
 *      P(t) = O + t * D
 *
 * Where:
 *   - O is the ray origin (point3)
 *   - D is the ray direction (vec3, not required to be normalized)
 *   - t is a scalar parameter
 *
 * Rays are a fundamental element in ray tracing, used for intersection
 * calculations against geometry such as spheres, planes, and meshes.
 ***********************/
class ray
{
  public:
    /***********************
     * ray (default constructor)
     * @brief Creates an empty ray with default-initialized origin and direction.
     ***********************/
    ray() {}

    /***********************
     * ray (constructor)
     * @brief Creates a ray from a specified origin and direction.
     * @param origin  Reference point where the ray starts.
     * @param direction Direction vector of the ray.
     * @note Direction does not need to be normalized, but some algorithms may
     * assume it.
     ***********************/
    ray(const point3& origin, const vec3& direction) : orig(origin), dir(direction) {}

    /***********************
     * origin
     * @brief Accessor for the ray’s origin.
     * @return const reference to origin point.
     ***********************/
    const point3& origin() const { return orig; }

    /***********************
     * direction
     * @brief Accessor for the ray’s direction vector.
     * @return const reference to direction vector.
     ***********************/
    const vec3& direction() const { return dir; }

    /***********************
     * at
     * @brief Computes the point along the ray at parameter t.
     *
     * Uses the formula: P(t) = O + t * D
     *
     * @param t Distance parameter along the ray.
     * @return A point3 object representing the position at parameter t.
     * @note If D is not normalized, t represents scaled units, not exact
     * distance.
     ***********************/
    point3 at(double t) const { return orig + t * dir; }

  private:
    point3 orig; // Ray origin in 3D space
    vec3 dir;    // Ray direction vector
};

#endif // RAY_H
