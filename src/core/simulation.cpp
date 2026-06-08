/****************
 * Author: Sinan Demir
 * File: simulation.cpp
 * Date: 10/31/2025
 * Purpose: Implementation file of simulation
 *****************/

#include "simulation.h"
#include "eclipse.h"
#include "vec3.h"
#include <algorithm>
#include <filesystem>

/***********************
 * computeGravitationalForce
 * @brief: Computes mutual gravitational acceleration between two celestial
 * bodies.
 * @param: a - first body (acceleration will be updated)
 * @param: b - second body (acceleration will be updated)
 * @exception: none
 * @return: none
 * @note: Applies Newton's law of universal gravitation.
 *        This function is intended to be called with i < j in an outer loop
 *        to preserve momentum symmetry and avoid double-counting.
 ***********************/
void computeGravitationalForce(CelestialBody& a, CelestialBody& b)
{
    // Vector from a to b
    vec3 r_vec = b.position - a.position;
    double r2 = r_vec.length_squared();

    if (r2 < 1.0)
    {
        // Avoid singularities or extremely close approaches
        return;
    }

    double r = std::sqrt(r2);
    double invr = 1.0 / r;
    double invr3 = invr / r2; // 1 / r^3

    // Acceleration directions:
    // a.acc =  G * m_b / r^3 * r_vec
    // b.acc = -G * m_a / r^3 * r_vec
    vec3 acc_dir = r_vec;

    vec3 acc_a = (physics::constants::G * b.mass * invr3) * acc_dir;
    vec3 acc_b = (physics::constants::G * a.mass * invr3) * (-acc_dir);

    a.acceleration += acc_a;
    b.acceleration += acc_b;
}

/***********************
 * eulerStep
 * @brief: Simple Euler integration step (unused in main loop but kept for
 * reference).
 * @param: body - CelestialBody reference
 * @param: dt   - time step
 * @exception none
 * @return none
 ***********************/
void eulerStep(CelestialBody& body, double dt)
{
    body.velocity += body.acceleration * dt;
    body.position += body.velocity * dt;
}

/***********************
 * resetAccelerations
 * @brief: Sets acceleration vectors to zero for every body in the collection.
 ***********************/
void resetAccelerations(std::vector<CelestialBody>& bodies)
{
    for (auto& b : bodies)
    {
        b.acceleration = vec3(0.0, 0.0, 0.0);
    }
}

/***********************
 * updateAccelerations
 * @brief: Recomputes gravitational accelerations for the entire system.
 * @note: Uses computeGravitationalForce pairwise with i < j
 *        to ensure Newton's 3rd law and avoid double-counting.
 ***********************/
void updateAccelerations(std::vector<CelestialBody>& bodies)
{
    resetAccelerations(bodies);

    const std::size_t N = bodies.size();
    for (std::size_t i = 0; i < N; ++i)
    {
        for (std::size_t j = i + 1; j < N; ++j)
        {
            computeGravitationalForce(bodies[i], bodies[j]);
        }
    }
}

/***********************
 * evaluateDerivatives
 * @brief: Produces derivatives for RK4 from the current state.
 * @param: bodies - current state
 * @return: vector of StateDerivative (dpos, dvel) for each body
 ***********************/
std::vector<StateDerivative> evaluateDerivatives(std::vector<CelestialBody>& bodies)
{
    updateAccelerations(bodies);
    std::vector<StateDerivative> d(bodies.size());

    for (std::size_t i = 0; i < bodies.size(); ++i)
    {
        d[i].dpos = bodies[i].velocity;
        d[i].dvel = bodies[i].acceleration;
    }
    return d;
}

/***********************
 * buildIntermediateState
 * @brief: Generates an intermediate RK4 state from base state and derivatives.
 * @param: bodies - base state
 * @param: d      - derivatives at this stage
 * @param: scale  - scaling factor (e.g. dt/2, dt)
 * @return: new vector<CelestialBody> representing intermediate state
 ***********************/
std::vector<CelestialBody> buildIntermediateState(const std::vector<CelestialBody>& bodies,
                                                  const std::vector<StateDerivative>& d,
                                                  double scale)
{

    std::vector<CelestialBody> next = bodies;

    for (std::size_t i = 0; i < bodies.size(); ++i)
    {
        next[i].position += scale * d[i].dpos;
        next[i].velocity += scale * d[i].dvel;
    }
    return next;
}

/***********************
 * rk4Step
 * @brief: Classical RK4 solver for N-body system.
 * @param: bodies - state to be advanced in time
 * @param: dt     - time step
 * @exception: none
 * @return: none
 ***********************/
void rk4Step(std::vector<CelestialBody>& bodies, double dt)
{
    if (bodies.empty())
        return;

    auto k1 = evaluateDerivatives(bodies);
    auto s2 = buildIntermediateState(bodies, k1, dt * 0.5);
    auto k2 = evaluateDerivatives(s2);

    auto s3 = buildIntermediateState(bodies, k2, dt * 0.5);
    auto k3 = evaluateDerivatives(s3);

    auto s4 = buildIntermediateState(bodies, k3, dt);
    auto k4 = evaluateDerivatives(s4);

    const double sixth = dt / 6.0;
    for (std::size_t i = 0; i < bodies.size(); ++i)
    {
        bodies[i].position +=
            sixth * (k1[i].dpos + 2.0 * k2[i].dpos + 2.0 * k3[i].dpos + k4[i].dpos);
        bodies[i].velocity +=
            sixth * (k1[i].dvel + 2.0 * k2[i].dvel + 2.0 * k3[i].dvel + k4[i].dvel);
    }
}

/**
 * leapfrogStep
 *
 * @brief: Symplectic leapfrog integrator for N-body system. Advances positions and
 * velocities in a staggered manner to improve energy conservation over long timescales.
 * @param bodies
 * @param dt
 * @exception none
 * @return none
 * @note: Requires that accelerations are already computed before the first call.
 */
void leapfrogStep(std::vector<CelestialBody>& bodies, double dt)
{
    // Step 1: half-kick velocity using current accelerations
    // (accelerations must already be computed before first call)
    for (auto& b : bodies)
        b.velocity += b.acceleration * (dt * 0.5);

    // Step 2: full position update using half-kicked velocity
    for (auto& b : bodies)
        b.position += b.velocity * dt;

    // Step 3: recompute accelerations at new positions
    updateAccelerations(bodies);

    // Step 4: second half-kick with new accelerations
    for (auto& b : bodies)
        b.velocity += b.acceleration * (dt * 0.5);
}

/********************
 * detectSEM
 * @brief: Detects indices of Sun, Earth, and Moon in the bodies vector.
 * @param bodies   - vector of CelestialBody objects
 * @param idxSun   - output index of Sun
 * @param idxEarth - output index of Earth
 * @param idxMoon  - output index of Moon
 * @return true if all three bodies are found, false otherwise
 *********************/
bool detectSEM(const std::vector<CelestialBody>& bodies, int& idxSun, int& idxEarth, int& idxMoon)
{
    idxSun = idxEarth = idxMoon = -1;

    for (int i = 0; i < (int)bodies.size(); i++)
    {
        if (bodies[i].name == "Sun")
            idxSun = i;
        if (bodies[i].name == "Earth")
            idxEarth = i;
        if (bodies[i].name == "Moon")
            idxMoon = i;
    }

    return (idxSun >= 0 && idxEarth >= 0 && idxMoon >= 0);
}

ConservationSnapshot computeSnapshot(const physics::Conservations& C,
                                     double E0, double L0, double P0)
{
    ConservationSnapshot s;
    s.Lmag = std::sqrt(C.L[0]*C.L[0] + C.L[1]*C.L[1] + C.L[2]*C.L[2]);
    s.Pmag = std::sqrt(C.P[0]*C.P[0] + C.P[1]*C.P[1] + C.P[2]*C.P[2]);
    s.dE   = (C.total_energy - E0) / std::abs(E0);
    s.dL   = (s.Lmag - L0) / (L0 == 0.0 ? 1.0 : L0);
    s.dP   = (s.Pmag - P0) / (P0  == 0.0 ? 1.0 : P0);
    return s;
}

/********************
 * runSimulation
 * @brief: Generic N-body simulation runner using RK4 integrator.
 * @param bodies     - vector of CelestialBody objects (from JSON)
 * @param steps      - number of steps to simulate
 * @param dt         - timestep in seconds
 * @param outputPath - CSV output file path
 * @param integrator  - integration method to use (default: RK4)
 * @exception none
 * @return none
 *********************/
void runSimulation(std::vector<CelestialBody>& bodies, int steps, double dt,
                   const std::string& outputPath, Integrator integrator, int stride)
{
    if (bodies.empty())
    {
        std::cerr << "❌ No bodies to simulate.\n";
        return;
    }

    // ============================
    // Initial conservation checks
    // ============================
    physics::Conservations C0 = physics::compute(bodies);
    double E0 = C0.total_energy;

    double L0 = std::sqrt(C0.L[0] * C0.L[0] + C0.L[1] * C0.L[1] + C0.L[2] * C0.L[2]);

    double P0mag = std::sqrt(C0.P[0] * C0.P[0] + C0.P[1] * C0.P[1] + C0.P[2] * C0.P[2]);

    // ---------------------------------------------
    // Optional eclipse logging for Sun–Earth–Moon
    // ---------------------------------------------
    int idxSun, idxEarth, idxMoon;
    bool isSEM = detectSEM(bodies, idxSun, idxEarth, idxMoon);

    std::ofstream eclipseFile;
    if (isSEM)
    {
        // Derive eclipse log path from the output path
        // results/earth_moon.csv → results/earth_moon_eclipse.csv
        std::filesystem::path p(outputPath);
        std::string eclipsePath = (p.parent_path() / (p.stem().string() + "_eclipse.csv")).string();
        eclipseFile.open(eclipsePath);

        if (!eclipseFile)
        {
            std::cerr << "⚠️ Could not open eclipse log file: " << eclipsePath << "\n";
            isSEM = false; // disable logging if file failed
        }
        else
        {
            eclipseFile << "step,"
                        << "shadow_x,shadow_y,shadow_z,"
                        << "umbraRadius,penumbraRadius,eclipseType\n";

            std::cout << "🌓 Eclipse logging enabled → " << eclipsePath << "\n";
        }
    }

    // ============================
    // Auto-create output directory
    // ============================
    std::filesystem::path outPath(outputPath);
    if (outPath.has_parent_path())
        std::filesystem::create_directories(outPath.parent_path());

    // ============================
    // Open positions file (viewer reads this)
    // ============================
    std::ofstream file(outputPath);
    if (!file)
    {
        std::cerr << "❌ Could not open output file: " << outputPath << "\n";
        return;
    }

    // ============================
    // Open conservation file (Python reads this)
    // ============================
    std::string conservPath =
        (outPath.parent_path() / (outPath.stem().string() + "_conservation.csv")).string();

    std::ofstream conservFile(conservPath);
    if (!conservFile)
        std::cerr << "⚠️ Could not open conservation file: " << conservPath << "\n";

    // ============================
    // Metadata comment — positions file only
    // ============================
    file << "# stride=" << stride << " dt=" << dt << " bodies=";
    for (std::size_t i = 0; i < bodies.size(); ++i)
    {
        file << bodies[i].name << ":" << bodies[i].mass;
        if (i + 1 < bodies.size())
            file << ",";
    }
    file << "\n";

    // ============================
    // Positions header — no conservation columns
    // ============================
    file << "step";
    for (const auto& b : bodies)
        file << ",x_" << b.name << ",y_" << b.name << ",z_" << b.name;
    file << "\n";

    // ============================
    // Conservation header — separate file
    // ============================
    if (conservFile)
    {
        conservFile << "step"
                    << ",E_total,KE,PE"
                    << ",Lx,Ly,Lz,Lmag"
                    << ",Px,Py,Pz,Pmag"
                    << ",dE_rel,dL_rel,dP_rel"
                    << "\n";
    }

    // ============================
    // Seed accelerations for leapfrog before the loop
    // ============================
    if (integrator == Integrator::Leapfrog)
        updateAccelerations(bodies);

    // ============================
    // Main Integration Loop
    // ============================
    for (int i = 0; i < steps; ++i)
    {
        // --- Single integration step ---
        if (integrator == Integrator::Leapfrog)
            leapfrogStep(bodies, dt);
        else
            rk4Step(bodies, dt);

        // Only write to CSV every `stride` steps
        if (i % stride != 0)
            continue;

        // --- Compute updated conservation values ---
        physics::Conservations C = physics::compute(bodies);
        ConservationSnapshot snap = computeSnapshot(C, E0, L0, P0mag);

        // ---------------------------------------------
        // Eclipse logging (Sun–Earth–Moon only)
        // ---------------------------------------------
        if (isSEM)
        {
            const vec3& S = bodies[idxSun].position;
            const vec3& E = bodies[idxEarth].position;
            const vec3& M = bodies[idxMoon].position;

            EclipseResult e = computeSolarEclipse(S, E, M);

            eclipseFile << i << "," << e.shadowCenter.x() << "," << e.shadowCenter.y() << ","
                        << e.shadowCenter.z() << "," << e.umbraRadius << "," << e.penumbraRadius
                        << "," << e.eclipseType << "\n";
        }

        // ============================
        // CSV ROW (main orbit data)
        // ============================
        // Positions row
        file << i;
        for (const auto& b : bodies)
            file << "," << b.position.x() << "," << b.position.y() << "," << b.position.z();
        file << "\n";

        // Conservation row — write less frequently
        if (conservFile && i % (stride * 10) == 0)
        {
            conservFile << i << "," << C.total_energy << "," << C.kinetic_energy << ","
                        << C.potential_energy << "," << C.L[0] << "," << C.L[1] << "," << C.L[2]
                        << "," << snap.Lmag << "," << C.P[0] << "," << C.P[1] << "," << C.P[2] << ","
                        << snap.Pmag << "," << snap.dE << "," << snap.dL << "," << snap.dP << "\n";
        }
    }

    file.close();
    if (conservFile)
        conservFile.close();

    std::cout << "✅ Positions:     " << outputPath << "\n";
    std::cout << "✅ Conservation:  " << conservPath << "\n";
    if (isSEM)
    {
        eclipseFile.close();
    }

    std::cout << "✅ Simulation complete: " << outputPath << "\n";
}
// ============================================================================
// RK45 Dormand-Prince Adaptive Integrator
// ============================================================================

/***********************
 * buildIntermediateStateMulti
 * @brief: Builds an intermediate RK state from a linear combination of
 *         multiple derivative stages.
 *
 * Computes: result = base + dt * sum_i(weight_i * k_i)
 *
 * This is the generalization of buildIntermediateState() needed for
 * RK45 stages 3-6, which each depend on multiple previous k vectors.
 *
 * Physical meaning: we're estimating where the system will be at some
 * intermediate time by combining multiple derivative estimates, each
 * weighted by the Butcher tableau coefficients.
 *
 * @param base     Starting state (y_n)
 * @param weighted List of (coefficient, derivative_vector) pairs
 * @param dt       Timestep
 * @return         Intermediate state
 ***********************/
std::vector<CelestialBody> buildIntermediateStateMulti(
    const std::vector<CelestialBody>& base,
    const std::vector<std::pair<double, const std::vector<StateDerivative>*>>& weighted,
    double dt)
{
    std::vector<CelestialBody> result = base;

    for (std::size_t i = 0; i < base.size(); ++i)
    {
        vec3 dpos(0, 0, 0);
        vec3 dvel(0, 0, 0);

        for (const auto& [coeff, k] : weighted)
        {
            dpos += coeff * (*k)[i].dpos;
            dvel += coeff * (*k)[i].dvel;
        }

        result[i].position += dt * dpos;
        result[i].velocity += dt * dvel;
    }

    return result;
}

/***********************
 * computeErrorNorm
 * @brief: Computes the scaled RMS error norm for adaptive step control.
 *
 * For each coordinate of each body:
 *   sc_i = atol + rtol * max(|y_old_i|, |y_new_i|)
 *   err_i = error_i / sc_i
 *
 * Returns sqrt(mean(err_i^2))
 *
 * If < 1.0 → step is within tolerance (accept)
 * If >= 1.0 → step exceeds tolerance (reject, shrink dt)
 *
 * The scaling by sc_i makes the norm dimensionless, so position
 * error (in meters ~1e11) and velocity error (m/s ~1e4) are treated
 * fairly relative to their own magnitudes.
 ***********************/
double computeErrorNorm(
    const std::vector<CelestialBody>& y_old,
    const std::vector<CelestialBody>& y_new,
    const std::vector<CelestialBody>& err,
    double atol,
    double rtol)
{
    double sum = 0.0;
    int    n   = 0;

    for (std::size_t i = 0; i < y_old.size(); ++i)
    {
        // Position components
        for (int j = 0; j < 3; ++j)
        {
            double yo = y_old[i].position[j];
            double yn = y_new[i].position[j];
            double e  = err[i].position[j];
            double sc = atol + rtol * std::max(std::abs(yo), std::abs(yn));
            sum += (e / sc) * (e / sc);
            ++n;
        }

        // Velocity components
        for (int j = 0; j < 3; ++j)
        {
            double yo = y_old[i].velocity[j];
            double yn = y_new[i].velocity[j];
            double e  = err[i].velocity[j];
            double sc = atol + rtol * std::max(std::abs(yo), std::abs(yn));
            sum += (e / sc) * (e / sc);
            ++n;
        }
    }

    return std::sqrt(sum / n);
}

/***********************
 * rk45Step
 * @brief: One adaptive Dormand-Prince RK45 step.
 *
 * Computes six derivative stages (k1-k6) using the Dormand-Prince
 * Butcher tableau, then:
 *   - y5: 5th-order solution (accepted state if step passes)
 *   - err: difference between 4th and 5th order solutions (error estimate)
 *
 * The error norm determines whether to accept or reject the step,
 * and suggests the next timestep via the standard scaling formula:
 *   dt_next = dt * safety * (1/error_norm)^(1/5)
 *
 * FSAL property: k6 evaluated at y5 = k1 of the next step.
 * This saves one force evaluation per accepted step.
 * (Not yet exploited here — left as future optimization.)
 *
 * @param bodies  System state (modified in place if step accepted)
 * @param dt      Proposed timestep
 * @param atol    Absolute tolerance (meters / m/s)
 * @param rtol    Relative tolerance (dimensionless)
 * @return        RK45StepResult with acceptance flag and next dt suggestion
 ***********************/
RK45StepResult rk45Step(
    std::vector<CelestialBody>& bodies,
    double dt,
    double atol,
    double rtol)
{
    using namespace dp45;

    // ── Stage 1: k1 at current state ─────────────────────────────────────────
    auto k1 = evaluateDerivatives(bodies);

    // ── Stage 2: k2 at t + C2*dt ─────────────────────────────────────────────
    auto s2 = buildIntermediateState(bodies, k1, dt * A21);
    auto k2 = evaluateDerivatives(s2);

    // ── Stage 3: k3 at t + C3*dt ─────────────────────────────────────────────
    auto s3 = buildIntermediateStateMulti(bodies, {
        {A31, &k1},
        {A32, &k2},
    }, dt);
    auto k3 = evaluateDerivatives(s3);

    // ── Stage 4: k4 at t + C4*dt ─────────────────────────────────────────────
    auto s4 = buildIntermediateStateMulti(bodies, {
        {A41, &k1},
        {A42, &k2},
        {A43, &k3},
    }, dt);
    auto k4 = evaluateDerivatives(s4);

    // ── Stage 5: k5 at t + C5*dt ─────────────────────────────────────────────
    auto s5 = buildIntermediateStateMulti(bodies, {
        {A51, &k1},
        {A52, &k2},
        {A53, &k3},
        {A54, &k4},
    }, dt);
    auto k5 = evaluateDerivatives(s5);

    // ── Stage 6: k6 at t + dt ────────────────────────────────────────────────
    auto s6 = buildIntermediateStateMulti(bodies, {
        {A61, &k1},
        {A62, &k2},
        {A63, &k3},
        {A64, &k4},
        {A65, &k5},
    }, dt);
    auto k6 = evaluateDerivatives(s6);

    // ── 5th-order solution y5 ─────────────────────────────────────────────────
    // y5 = y_n + dt*(B1*k1 + B3*k3 + B4*k4 + B5*k5 + B6*k6)
    // B2 = 0 so k2 does not appear
    auto y5 = buildIntermediateStateMulti(bodies, {
        {B1, &k1},
        {B3, &k3},
        {B4, &k4},
        {B5, &k5},
        {B6, &k6},
    }, dt);

    // ── k7 at y5 (FSAL: this will be k1 of the next step) ────────────────────
    auto k7 = evaluateDerivatives(y5);

    // ── Error estimate: y5 - y4 ───────────────────────────────────────────────
    // err = dt*(E1*k1 + E3*k3 + E4*k4 + E5*k5 + E6*k6 + E7*k7)
    // We store the error as a CelestialBody vector for computeErrorNorm
    auto err_state = buildIntermediateStateMulti(bodies, {
        {E1, &k1},
        {E3, &k3},
        {E4, &k4},
        {E5, &k5},
        {E6, &k6},
        {E7, &k7},
    }, dt);

    // err_state currently holds: bodies + dt*(error terms)
    // We need just the delta: subtract bodies to get the error vector
    std::vector<CelestialBody> err_delta = err_state;
    for (std::size_t i = 0; i < bodies.size(); ++i)
    {
        err_delta[i].position = err_state[i].position - bodies[i].position;
        err_delta[i].velocity = err_state[i].velocity - bodies[i].velocity;
    }

    // ── Error norm and step control ───────────────────────────────────────────
    double error_norm = computeErrorNorm(bodies, y5, err_delta, atol, rtol);

    double scale = SAFETY * std::pow(1.0 / std::max(error_norm, 1e-10), EXP);
    scale = std::clamp(scale, MIN_SCALE, MAX_SCALE);
    double dt_next = dt * scale;

    RK45StepResult result;
    result.error_norm = error_norm;
    result.dt_next    = dt_next;

    if (error_norm <= 1.0)
    {
        // Accept: advance bodies to y5
        bodies        = y5;
        result.accepted = true;
        result.dt_used  = dt;
    }
    // If rejected: bodies unchanged, caller retries with dt_next

    return result;
}

/***********************
 * runSimulationAdaptive
 * @brief: Adaptive RK45 simulation runner.
 *
 * Unlike runSimulation() which uses a fixed step count,
 * this runs until t >= duration_s, writing output every
 * output_interval_s seconds of simulated time.
 *
 * @param bodies            System to simulate (modified in place)
 * @param duration_s        Total simulated time in seconds
 * @param dt_initial        Starting timestep guess
 * @param outputPath        CSV output file path
 * @param output_interval_s Write a CSV row every N simulated seconds
 * @param atol              Absolute tolerance (m / m/s)
 * @param rtol              Relative tolerance (dimensionless)
 * @param dt_min            Minimum allowed timestep
 * @param dt_max            Maximum allowed timestep
 ***********************/
void runSimulationAdaptive(
    std::vector<CelestialBody>& bodies,
    double duration_s,
    double dt_initial,
    const std::string& outputPath,
    double output_interval_s,
    double atol,
    double rtol,
    double dt_min,
    double dt_max)
{
    if (bodies.empty())
    {
        std::cerr << "❌ No bodies to simulate.\n";
        return;
    }

    // ── Conservation baseline ─────────────────────────────────────────────────
    physics::Conservations C0 = physics::compute(bodies);
    double E0   = C0.total_energy;
    double L0   = std::sqrt(C0.L[0]*C0.L[0] + C0.L[1]*C0.L[1] + C0.L[2]*C0.L[2]);
    double P0   = std::sqrt(C0.P[0]*C0.P[0] + C0.P[1]*C0.P[1] + C0.P[2]*C0.P[2]);

    // ── Output files ──────────────────────────────────────────────────────────
    std::filesystem::path outPath(outputPath);
    if (outPath.has_parent_path())
        std::filesystem::create_directories(outPath.parent_path());

    std::ofstream file(outputPath);
    if (!file)
    {
        std::cerr << "❌ Could not open output file: " << outputPath << "\n";
        return;
    }

    std::string conservPath =
        (outPath.parent_path() / (outPath.stem().string() + "_conservation.csv")).string();
    std::ofstream conservFile(conservPath);

    // ── Headers ───────────────────────────────────────────────────────────────
    file << "# rk45 dt_initial=" << dt_initial
         << " atol=" << atol << " rtol=" << rtol
         << " bodies=";
    for (std::size_t i = 0; i < bodies.size(); ++i)
    {
        file << bodies[i].name << ":" << bodies[i].mass;
        if (i + 1 < bodies.size()) file << ",";
    }
    file << "\n";

    // Use time_s instead of step for adaptive output
    file << "time_s";
    for (const auto& b : bodies)
        file << ",x_" << b.name << ",y_" << b.name << ",z_" << b.name;
    file << ",dt_used\n";

    if (conservFile)
    {
        conservFile << "time_s,E_total,KE,PE,Lmag,Pmag"
                    << ",dE_rel,dL_rel,dP_rel\n";
    }

    // ── Eclipse detection ─────────────────────────────────────────────────────
    int idxSun, idxEarth, idxMoon;
    bool isSEM = detectSEM(bodies, idxSun, idxEarth, idxMoon);

    // ── Adaptive loop ─────────────────────────────────────────────────────────
    double t              = 0.0;
    double dt             = dt_initial;
    double next_output    = 0.0;   // write at t=0 and every output_interval_s
    int    n_steps        = 0;
    int    n_rejected     = 0;

    while (t < duration_s)
    {
        // Don't overshoot the end
        if (t + dt > duration_s)
            dt = duration_s - t;

        auto result = rk45Step(bodies, dt, atol, rtol);

        if (result.accepted)
        {
            t  += result.dt_used;
            ++n_steps;

            // Write output at regular simulated-time intervals
            if (t >= next_output)
            {
                physics::Conservations C = physics::compute(bodies);
                ConservationSnapshot snap = computeSnapshot(C, E0, L0, P0);

                // Positions row
                file << t;
                for (const auto& b : bodies)
                    file << "," << b.position.x()
                         << "," << b.position.y()
                         << "," << b.position.z();
                file << "," << result.dt_used << "\n";

                // Conservation row
                if (conservFile)
                {
                    conservFile << t << ","
                                << C.total_energy << ","
                                << C.kinetic_energy << ","
                                << C.potential_energy << ","
                                << snap.Lmag << "," << snap.Pmag << ","
                                << snap.dE << "," << snap.dL << "," << snap.dP << "\n";
                }

                next_output += output_interval_s;
            }
        }
        else
        {
            ++n_rejected;
        }

        // Clamp next timestep
        dt = std::clamp(result.dt_next, dt_min, dt_max);
    }

    file.close();
    if (conservFile) conservFile.close();

    double reject_pct = 100.0 * n_rejected / std::max(1, n_steps + n_rejected);

    std::cout << "✅ RK45 complete:\n"
              << "   Steps accepted: " << n_steps    << "\n"
              << "   Steps rejected: " << n_rejected
              << " (" << reject_pct << "%)\n"
              << "   Final dt:       " << dt         << " s\n"
              << "   Positions:      " << outputPath  << "\n"
              << "   Conservation:   " << conservPath << "\n";
}
