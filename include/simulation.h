/****************
 * Author: Sinan Demir
 * File: simulation.h
 * Date: 10/31/2025
 * Purpose: Header file for simulation.cpp
 * Updated: 04/06/2026 — added RK45 adaptive integrator
 *****************/

#ifndef SIMULATION_H
#define SIMULATION_H

#include "body.h"
#include "conservations.h"
#include "eclipse.h"
#include "rk45_coefficients.h"
#include "utils.h"
#include "vec3.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <vector>

// ── Integrator selection ──────────────────────────────────────────────────────
enum class Integrator
{
    RK4,
    Leapfrog,
    RK45,
    euler
};

// ── Internal derivative type ──────────────────────────────────────────────────
// Exposed here so RK45 implementation can use it alongside rk4Step internals.
struct StateDerivative
{
    vec3 dpos; ///< velocity (dr/dt)
    vec3 dvel; ///< acceleration (dv/dt)
};

// ── Core force / acceleration ─────────────────────────────────────────────────
void computeAcceleration(CelestialBody& earth, const CelestialBody& sun);
void computeGravitationalForce(CelestialBody& a, CelestialBody& b);

// ── Integration primitives ────────────────────────────────────────────────────
// These are used internally by rk4Step and rk45Step.
// Exposed so tests can call them directly if needed.

// Evaluate derivatives at the current state (runs full force calculation)
std::vector<StateDerivative> evaluateStateDerivatives(std::vector<CelestialBody>& bodies);

// Build intermediate state: base + scale * d  (single derivative)
std::vector<CelestialBody> buildIntermediateState(
    const std::vector<CelestialBody>& bodies,
    const std::vector<StateDerivative>& d,
    double scale);

// Build intermediate state: base + dt * sum(coeff_i * k_i)
// Used by RK45 stages 3-6 which combine multiple previous derivatives.
std::vector<CelestialBody> buildIntermediateStateMulti(
    const std::vector<CelestialBody>& base,
    const std::vector<std::pair<double, const std::vector<StateDerivative>*>>& weighted,
    double dt);

// ── Fixed-step integrators ────────────────────────────────────────────────────
void eulerStep(CelestialBody& body, double dt);
void rk4Step(std::vector<CelestialBody>& bodies, double dt);
void leapfrogStep(std::vector<CelestialBody>& bodies, double dt);

// ── RK45 adaptive integrator ──────────────────────────────────────────────────

/***********************
 * struct RK45StepResult
 * @brief: Result of one adaptive RK45 step attempt.
 *
 * If accepted == true:
 *   bodies has been advanced by dt_used seconds
 *   dt_next is the suggested timestep for the next step
 *
 * If accepted == false:
 *   bodies is unchanged
 *   dt_next is a smaller timestep — caller should retry with it
 ***********************/
struct RK45StepResult
{
    bool   accepted   = false;
    double dt_used    = 0.0;
    double dt_next    = 0.0;
    double error_norm = 0.0;
};

// Compute scaled RMS error norm for step acceptance decision.
// Returns < 1.0 if step is within tolerance, >= 1.0 if step should be rejected.
double computeErrorNorm(
    const std::vector<CelestialBody>& y_old,
    const std::vector<CelestialBody>& y_new,
    const std::vector<CelestialBody>& error,
    double atol,
    double rtol);

// Attempt one adaptive RK45 step.
// If error_norm < 1: accepts step, advances bodies, returns dt_next > dt
// If error_norm >= 1: rejects step, bodies unchanged, returns dt_next < dt
RK45StepResult rk45Step(
    std::vector<CelestialBody>& bodies,
    double dt,
    double atol = 1e-9,
    double rtol = 1e-9);

// ── Simulation runner ─────────────────────────────────────────────────────────
void runSimulation(std::vector<CelestialBody>& bodies,
                   int steps,
                   double dt,
                   const std::string& outputPath,
                   Integrator integrator = Integrator::RK4,
                   int stride = 1);

// RK45 overload — time-based instead of step-based
void runSimulationAdaptive(std::vector<CelestialBody>& bodies,
                           double duration_s,
                           double dt_initial,
                           const std::string& outputPath,
                           double output_interval_s = 3600.0,
                           double atol = 1e-9,
                           double rtol = 1e-9,
                           double dt_min = 1.0,
                           double dt_max = 86400.0);

#endif // SIMULATION_H
