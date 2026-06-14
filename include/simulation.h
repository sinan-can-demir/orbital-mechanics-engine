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
#include "constants.h"
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
    euler,
    Yoshida4,
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

// Build intermediate state: base + scale * d  (single derivative)
std::vector<CelestialBody> buildIntermediateState(const std::vector<CelestialBody>& bodies,
                                                  const std::vector<StateDerivative>& d,
                                                  double scale);

// Build intermediate state: base + dt * sum(coeff_i * k_i)
// Used by RK45 stages 3-6 which combine multiple previous derivatives.
std::vector<CelestialBody> buildIntermediateStateMulti(
    const std::vector<CelestialBody>& base,
    const std::vector<std::pair<double, const std::vector<StateDerivative>*>>& weighted, double dt);

// ── Conservation snapshot ─────────────────────────────────────────────────────
struct ConservationSnapshot
{
    double dE;   // relative energy drift
    double dL;   // relative angular momentum drift
    double dP;   // relative linear momentum drift
    double Lmag; // angular momentum magnitude
    double Pmag; // linear momentum magnitude
    double total_energy;
    double kinetic_energy;
    double potential_energy;
    double Lx, Ly, Lz;
    double Px, Py, Pz;
};

struct SimulationSnapshot
{
    int step;
    double time_s;
    double dt_used;
    std::vector<vec3> positions;
    ConservationSnapshot conservation;
};

struct SimulationResult
{
    std::vector<std::string> body_names;
    std::vector<double> body_masses;
    std::vector<SimulationSnapshot> snapshots;
    double dt = 0.0;
    int stride = 1;
    bool is_adaptive = false;
};

ConservationSnapshot computeSnapshot(const physics::Conservations& C, double E0, double L0,
                                     double P0);

void exportCSV(const SimulationResult& result, const std::string& outputPath);

// Pure physics — no file I/O. Used by Python bindings and internally by runSimulation().
SimulationResult runSimulationCore(std::vector<CelestialBody>& bodies, int steps, double dt,
                                   Integrator integrator = Integrator::RK4, int stride = 1);

SimulationResult runSimulationAdaptiveCore(std::vector<CelestialBody>& bodies, double duration_s,
                                           double dt_initial, double output_interval_s = 3600.0,
                                           double atol = 1e-9, double rtol = 1e-9,
                                           double dt_min = 1.0, double dt_max = 86400.0);

// ── Fixed-step integrators ────────────────────────────────────────────────────
void eulerStep(CelestialBody& body, double dt);
void rk4Step(std::vector<CelestialBody>& bodies, double dt);
void leapfrogStep(std::vector<CelestialBody>& bodies, double dt);
void yoshida4Step(std::vector<CelestialBody>& bodies, double dt);
void updateAccelerations(std::vector<CelestialBody>& bodies);

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
    bool accepted = false;
    double dt_used = 0.0;
    double dt_next = 0.0;
    double error_norm = 0.0;
    std::vector<StateDerivative> k7; // FSAL: pass back as k1_fsal on next accepted step
};

// Compute scaled RMS error norm for step acceptance decision.
// Returns < 1.0 if step is within tolerance, >= 1.0 if step should be rejected.
double computeErrorNorm(const std::vector<CelestialBody>& y_old,
                        const std::vector<CelestialBody>& y_new,
                        const std::vector<CelestialBody>& error, double atol, double rtol);

// Attempt one adaptive RK45 step.
// If error_norm < 1: accepts step, advances bodies, returns dt_next > dt, saves k7 for FSAL
// If error_norm >= 1: rejects step, bodies unchanged, returns dt_next < dt
// k1_fsal: if non-null, skip recomputing k1 (reuse k7 from previous accepted step)
RK45StepResult rk45Step(std::vector<CelestialBody>& bodies, double dt, double atol = 1e-9,
                        double rtol = 1e-9, const std::vector<StateDerivative>* k1_fsal = nullptr);

// ── Simulation runner ─────────────────────────────────────────────────────────
void runSimulation(std::vector<CelestialBody>& bodies, int steps, double dt,
                   const std::string& outputPath, Integrator integrator = Integrator::RK4,
                   int stride = 1);

// RK45 overload — time-based instead of step-based
void runSimulationAdaptive(std::vector<CelestialBody>& bodies, double duration_s, double dt_initial,
                           const std::string& outputPath, double output_interval_s = 3600.0,
                           double atol = 1e-9, double rtol = 1e-9, double dt_min = 1.0,
                           double dt_max = 86400.0);

#endif // SIMULATION_H
