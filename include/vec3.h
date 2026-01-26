/************************************************************************************
 * Source: https://raytracing.github.io/books/RayTracingInOneWeekend.html
 * Author: Peter Shirley, Trevor David Black, Steve Hollasch
 * Modified by: Sinan Demir
 * File: vec3.h
 * Date modified: 11/16/2025
 *
 * Description:
 *    Header file defining a 3D vector class (`vec3`) with common vector
 * operations. Used for ray tracing, graphics, and geometric computations.
 *
 * Debug Options:
 *    Define VEC3_DEBUG before including this file to enable:
 *      - Out-of-range indexing checks
 *      - NaN checks in length, arithmetic, and normalization
 *
 * Example:
 *      vec3 a(1,2,3);
 *      vec3 b(4,5,6);
 *      vec3 c = a + b;
 *      std::cout << "Result: " << c << std::endl;
 *
 ************************************************************************************/

#ifndef VEC3_H
#define VEC3_H

#include <cmath>
#include <iostream>
#include <stdexcept> // used for debug exceptions

//============================================================
//  vec3 Class Definition
//============================================================

/**
 * @brief Represents a 3-dimensional vector or RGB color.
 *
 * The vec3 class stores three double precision components and
 * provides common vector arithmetic used in ray-tracing.
 */
class vec3 {
public:
  double e[3]; ///< Storage for x, y, z components.

  /// Constructs a zero vector (0,0,0).
  vec3() : e{0, 0, 0} {}

  /// Constructs a vector with components (e0, e1, e2).
  vec3(double e0, double e1, double e2) : e{e0, e1, e2} {}

  /// @return X component
  double x() const { return e[0]; }

  /// @return Y component
  double y() const { return e[1]; }

  /// @return Z component
  double z() const { return e[2]; }

  /// Negation: returns (-x, -y, -z)
  vec3 operator-() const { return vec3(-e[0], -e[1], -e[2]); }

  /// Read-only index operator with optional debug bounds checking.
  double operator[](int i) const {
#ifdef VEC3_DEBUG
    if (i < 0 || i > 2)
      throw std::out_of_range("vec3 index out of range");
#endif
    return e[i];
  }

  /// Writable index operator with optional debug bounds checking.
  double &operator[](int i) {
#ifdef VEC3_DEBUG
    if (i < 0 || i > 2)
      throw std::out_of_range("vec3 index out of range");
#endif
    return e[i];
  }

  /// Adds another vector to this one.
  vec3 &operator+=(const vec3 &v) {
    e[0] += v.e[0];
    e[1] += v.e[1];
    e[2] += v.e[2];
    return *this;
  }

  /// Scales this vector by t.
  vec3 &operator*=(double t) {
    e[0] *= t;
    e[1] *= t;
    e[2] *= t;
    return *this;
  }

  /// Divides this vector by t (multiplies by reciprocal).
  vec3 &operator/=(double t) {
#ifdef VEC3_DEBUG
    if (t == 0)
      throw std::runtime_error("vec3 divide by zero");
#endif
    return *this *= 1 / t;
  }

  /// @return Magnitude (length) of the vector.
  double length() const {
#ifdef VEC3_DEBUG
    if (std::isnan(e[0]) || std::isnan(e[1]) || std::isnan(e[2]))
      throw std::runtime_error("vec3 contains NaN component");
#endif
    return std::sqrt(length_squared());
  }

  /// @return Squared magnitude (avoids sqrt, faster for comparisons).
  double length_squared() const {
    return e[0] * e[0] + e[1] * e[1] + e[2] * e[2];
  }
};

// point3 is a semantic alias for vec3 (e.g., representing a location in space)
using point3 = vec3;

//============================================================
//  Vector Utility Functions
//============================================================

/// Stream output formatting: prints "x y z"
inline std::ostream &operator<<(std::ostream &out, const vec3 &v) {
  return out << v.e[0] << ' ' << v.e[1] << ' ' << v.e[2];
}

/// Vector addition
inline vec3 operator+(const vec3 &u, const vec3 &v) {
  return vec3(u.e[0] + v.e[0], u.e[1] + v.e[1], u.e[2] + v.e[2]);
}

/// Vector subtraction
inline vec3 operator-(const vec3 &u, const vec3 &v) {
  return vec3(u.e[0] - v.e[0], u.e[1] - v.e[1], u.e[2] - v.e[2]);
}

/// Component-wise multiplication
inline vec3 operator*(const vec3 &u, const vec3 &v) {
  return vec3(u.e[0] * v.e[0], u.e[1] * v.e[1], u.e[2] * v.e[2]);
}

/// Scalar multiplication (t * v)
inline vec3 operator*(double t, const vec3 &v) {
  return vec3(t * v.e[0], t * v.e[1], t * v.e[2]);
}

/// Scalar multiplication (v * t)
inline vec3 operator*(const vec3 &v, double t) { return t * v; }

/// Scalar division (v / t)
inline vec3 operator/(const vec3 &v, double t) {
#ifdef VEC3_DEBUG
  if (t == 0)
    throw std::runtime_error("vec3 divide by zero");
#endif
  return (1 / t) * v;
}

/// Dot product
inline double dot(const vec3 &u, const vec3 &v) {
  return u.e[0] * v.e[0] + u.e[1] * v.e[1] + u.e[2] * v.e[2];
}

/// Cross product
inline vec3 cross(const vec3 &u, const vec3 &v) {
  return vec3(u.e[1] * v.e[2] - u.e[2] * v.e[1],
              u.e[2] * v.e[0] - u.e[0] * v.e[2],
              u.e[0] * v.e[1] - u.e[1] * v.e[0]);
}

/// Unit normalization: v / |v|
inline vec3 unit_vector(const vec3 &v) {
#ifdef VEC3_DEBUG
  double len = v.length();
  if (len == 0)
    throw std::runtime_error("normalize zero-length vector");
  return v / len;
#else
  return v / v.length();
#endif
}

#endif // VEC3_H
