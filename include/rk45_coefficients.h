/****************
 * File: rk45_coefficients.h
 * Author: Sinan Demir
 * Date: 04/06/2026
 * Purpose: Dormand-Prince RK45 Butcher tableau constants.
 *
 * Source: Dormand & Prince (1980),
 *         "A family of embedded Runge-Kutta formulae",
 *         J. Computational and Applied Mathematics, 6(1), 19-26.
 *
 * These are exact rational fractions — do not approximate them.
 * Using exact fractions ensures the tableau matches the published
 * paper exactly and is reproducible across implementations.
 ****************/

#ifndef RK45_COEFFICIENTS_H
#define RK45_COEFFICIENTS_H

namespace dp45
{

// ── Time offsets (c coefficients) ────────────────────────────────────────────
// These tell RK45 where in [t, t+dt] to evaluate each stage.
// c1=0 means evaluate at current time, c6=1 means at t+dt, etc.
constexpr double C2 = 1.0 / 5.0;
constexpr double C3 = 3.0 / 10.0;
constexpr double C4 = 4.0 / 5.0;
constexpr double C5 = 8.0 / 9.0;
constexpr double C6 = 1.0;
constexpr double C7 = 1.0;

// ── Stage combination coefficients (a coefficients) ──────────────────────────
// Stage 2: y_n + dt * A21*k1
constexpr double A21 = 1.0 / 5.0;

// Stage 3: y_n + dt * (A31*k1 + A32*k2)
constexpr double A31 = 3.0 / 40.0;
constexpr double A32 = 9.0 / 40.0;

// Stage 4: y_n + dt * (A41*k1 + A42*k2 + A43*k3)
constexpr double A41 = 44.0 / 45.0;
constexpr double A42 = -56.0 / 15.0;
constexpr double A43 = 32.0 / 9.0;

// Stage 5: y_n + dt * (A51*k1 + ... + A54*k4)
constexpr double A51 = 19372.0 / 6561.0;
constexpr double A52 = -25360.0 / 2187.0;
constexpr double A53 = 64448.0 / 6561.0;
constexpr double A54 = -212.0 / 729.0;

// Stage 6: y_n + dt * (A61*k1 + ... + A65*k5)
constexpr double A61 = 9017.0 / 3168.0;
constexpr double A62 = -355.0 / 33.0;
constexpr double A63 = 46732.0 / 5247.0;
constexpr double A64 = 49.0 / 176.0;
constexpr double A65 = -5103.0 / 18656.0;

// ── 5th-order solution weights (b coefficients) ───────────────────────────────
// y5 = y_n + dt*(B1*k1 + B3*k3 + B4*k4 + B5*k5 + B6*k6)
// Note: B2 = 0 so k2 does not appear in the solution
constexpr double B1 = 35.0 / 384.0;
// B2 = 0
constexpr double B3 = 500.0 / 1113.0;
constexpr double B4 = 125.0 / 192.0;
constexpr double B5 = -2187.0 / 6784.0;
constexpr double B6 = 11.0 / 84.0;
// B7 = 0

// ── Error estimate weights (e coefficients) ───────────────────────────────────
// These are (b5_i - b4_i): the difference between 5th and 4th order weights.
// err = dt * (E1*k1 + E3*k3 + E4*k4 + E5*k5 + E6*k6 + E7*k7)
// You never need b4 explicitly — only this difference matters.
constexpr double E1 = 71.0 / 57600.0;
// E2 = 0
constexpr double E3 = -71.0 / 16695.0;
constexpr double E4 = 71.0 / 1920.0;
constexpr double E5 = -17253.0 / 339200.0;
constexpr double E6 = 22.0 / 525.0;
constexpr double E7 = -1.0 / 40.0;

// ── Step size control ─────────────────────────────────────────────────────────
// safety:    conservative scaling — don't step right to the error limit
// MIN_SCALE: never shrink dt by more than 10x in one step
// MAX_SCALE: never grow dt by more than 10x in one step
// EXP:       error order exponent for 5th-order method = 1/5
constexpr double SAFETY = 0.9;
constexpr double MIN_SCALE = 0.1;
constexpr double MAX_SCALE = 10.0;
constexpr double EXP = 1.0 / 5.0;

} // namespace dp45

#endif // RK45_COEFFICIENTS_H