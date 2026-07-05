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
#include <functional>

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
        std::cerr << "⚠️  bodies '" << a.name << "' and '" << b.name << "' are within 1 m (r²=" << r2
                  << ") — gravitational force skipped.\n";
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
 * applyGRCorrection
 * @brief: Applies Schwarzschild 1PN post-Newtonian correction to all non-central bodies.
 *
 * Δa_i = (GM/c²r³) [ (4GM/r − v²) r + 4(r·v) v ]
 *
 * where r and v are measured relative to the most massive body (Sun).
 * Must be called AFTER Newtonian accelerations are computed.
 ***********************/
void applyGRCorrection(std::vector<CelestialBody>& bodies)
{
    // Find most massive body (Sun in solar system simulations)
    std::size_t sun_idx = 0;
    for (std::size_t k = 1; k < bodies.size(); ++k)
        if (bodies[k].mass > bodies[sun_idx].mass)
            sun_idx = k;

    const CelestialBody& sun = bodies[sun_idx];
    const double GM = physics::constants::G * sun.mass;
    constexpr double C2 = physics::constants::C_LIGHT * physics::constants::C_LIGHT;

    for (std::size_t i = 0; i < bodies.size(); ++i)
    {
        if (i == sun_idx)
            continue;

        vec3 r = bodies[i].position - sun.position;
        vec3 v = bodies[i].velocity - sun.velocity;
        double r_mag = r.length();

        if (r_mag == 0.0)
            continue;

        double r3 = r_mag * r_mag * r_mag;
        double v2 = v.length_squared();
        double rdv = dot(r, v);

        // Schwarzschild 1PN: Δa = (GM/c²r³)[(4GM/r − v²)r + 4(r·v)v]
        double factor = GM / (C2 * r3);
        bodies[i].acceleration += factor * ((4.0 * GM / r_mag - v2) * r + 4.0 * rdv * v);
    }
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
 *        If use_gr is true, applies Schwarzschild 1PN correction after Newtonian forces.
 ***********************/
void updateAccelerations(std::vector<CelestialBody>& bodies, bool use_gr)
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

    if (use_gr)
        applyGRCorrection(bodies);
}

/***********************
 * evaluateDerivatives
 * @brief: Produces derivatives for RK4 from the current state.
 * @param: bodies - current state
 * @return: vector of StateDerivative (dpos, dvel) for each body
 ***********************/
std::vector<StateDerivative> evaluateDerivatives(std::vector<CelestialBody>& bodies, bool use_gr)
{
    updateAccelerations(bodies, use_gr);
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
 * @param: use_gr - if true, apply 1PN GR correction at each derivative evaluation
 * @exception: none
 * @return: none
 ***********************/
void rk4Step(std::vector<CelestialBody>& bodies, double dt, bool use_gr)
{
    if (bodies.empty())
        return;

    auto k1 = evaluateDerivatives(bodies, use_gr);
    auto s2 = buildIntermediateState(bodies, k1, dt * 0.5);
    auto k2 = evaluateDerivatives(s2, use_gr);

    auto s3 = buildIntermediateState(bodies, k2, dt * 0.5);
    auto k3 = evaluateDerivatives(s3, use_gr);

    auto s4 = buildIntermediateState(bodies, k3, dt);
    auto k4 = evaluateDerivatives(s4, use_gr);

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
 * @param use_gr - if true, apply 1PN GR correction when recomputing accelerations
 * @exception none
 * @return none
 * @note: Requires that accelerations are already computed before the first call.
 */
void leapfrogStep(std::vector<CelestialBody>& bodies, double dt, bool use_gr)
{
    // Step 1: half-kick velocity using current accelerations
    // (accelerations must already be computed before first call)
    for (auto& b : bodies)
        b.velocity += b.acceleration * (dt * 0.5);

    // Step 2: full position update using half-kicked velocity
    for (auto& b : bodies)
        b.position += b.velocity * dt;

    // Step 3: recompute accelerations at new positions
    updateAccelerations(bodies, use_gr);

    // Step 4: second half-kick with new accelerations
    for (auto& b : bodies)
        b.velocity += b.acceleration * (dt * 0.5);
}

/**
 * yoshida4Step
 *
 * @brief: 4th-order symplectic integrator via Yoshida (1990) triple-Leapfrog composition.
 * @param use_gr - if true, apply 1PN GR correction when recomputing accelerations
 * @note: Requires accelerations pre-computed before first call (same as leapfrogStep).
 *        3 force evaluations per step vs 1 for Leapfrog, but achieves O(dt^5) error
 *        vs O(dt^3) — a much larger timestep can be used for equivalent accuracy.
 */
void yoshida4Step(std::vector<CelestialBody>& bodies, double dt, bool use_gr)
{
    // Yoshida (1990) coefficients: compose Leapfrog(w1) ∘ Leapfrog(w0) ∘ Leapfrog(w1)
    // w0 is negative — the middle sub-step reverses direction to cancel error terms.
    constexpr double CR2 = 1.2599210498948732; // ∛2
    constexpr double w1 = 1.0 / (2.0 - CR2);   // ≈ +1.3512
    constexpr double w0 = 1.0 - 2.0 * w1;      // ≈ −1.7024
    constexpr double c1 = w1 / 2.0;            // ≈ +0.6756  (merged outer half-kicks)
    constexpr double c2 = (w0 + w1) / 2.0;     // ≈ −0.1756  (merged inner half-kicks)

    // Palindromic KDK sequence: kick c1, drift w1, kick c2, drift w0, kick c2, drift w1, kick c1
    for (auto& b : bodies)
        b.velocity += b.acceleration * (c1 * dt);
    for (auto& b : bodies)
        b.position += b.velocity * (w1 * dt);
    updateAccelerations(bodies, use_gr);

    for (auto& b : bodies)
        b.velocity += b.acceleration * (c2 * dt);
    for (auto& b : bodies)
        b.position += b.velocity * (w0 * dt);
    updateAccelerations(bodies, use_gr);

    for (auto& b : bodies)
        b.velocity += b.acceleration * (c2 * dt);
    for (auto& b : bodies)
        b.position += b.velocity * (w1 * dt);
    updateAccelerations(bodies, use_gr); // leave accelerations current for next step

    for (auto& b : bodies)
        b.velocity += b.acceleration * (c1 * dt);
}

/***********************
 * computeJerks
 * @brief: Computes da/dt (jerk) for every body from pairwise Newtonian gravity.
 *
 * da_i/dt = G * m_j * [ v_ij/r^3 - 3*(r_ij . v_ij)*r_ij/r^5 ],  pairwise i<j (Newton's 3rd law)
 *
 * @note: Required by hermiteStep. Not folded into computeGravitationalForce since no other
 *        integrator needs it, and jerk has no field on CelestialBody.
 *        Does NOT include a jerk contribution from the GR correction (see applyGRCorrection) —
 *        hermiteStep applies GR to acceleration only, as an approximation.
 ***********************/
std::vector<vec3> computeJerks(const std::vector<CelestialBody>& bodies)
{
    std::vector<vec3> jerks(bodies.size(), vec3(0.0, 0.0, 0.0));
    const std::size_t N = bodies.size();

    for (std::size_t i = 0; i < N; ++i)
    {
        for (std::size_t j = i + 1; j < N; ++j)
        {
            vec3 r_vec = bodies[j].position - bodies[i].position;
            vec3 v_vec = bodies[j].velocity - bodies[i].velocity;
            double r2 = r_vec.length_squared();

            if (r2 < 1.0) // mirrors the close-approach guard in computeGravitationalForce
                continue;

            double r = std::sqrt(r2);
            double invr3 = 1.0 / (r * r2);
            double invr5 = invr3 / r2;
            double rdotv = dot(r_vec, v_vec);

            vec3 jerk_dir = v_vec * invr3 - (3.0 * rdotv * invr5) * r_vec;

            jerks[i] += (physics::constants::G * bodies[j].mass) * jerk_dir;
            jerks[j] += (-physics::constants::G * bodies[i].mass) * jerk_dir;
        }
    }
    return jerks;
}

/***********************
 * hermiteStep
 * @brief: 4th-order Hermite predictor-corrector (Makino & Aarseth 1992).
 * @param use_gr - if true, apply 1PN GR correction to acceleration (not jerk — approximation)
 * @note: Requires bodies[i].acceleration valid at the current state before the first call
 *        (same precondition as leapfrogStep). Leaves it valid at the corrected state on return.
 *
 * Predict:  x_p = x + v*dt + a*dt^2/2 + j*dt^3/6
 *           v_p = v + a*dt + j*dt^2/2
 * Evaluate: a_p, j_p at the predicted state
 * Correct:  v1 = v + (a+a_p)/2*dt + (j-j_p)/12*dt^2
 *           x1 = x + (v+v1)/2*dt + (a-a_p)/12*dt^2
 ***********************/
void hermiteStep(std::vector<CelestialBody>& bodies, double dt, bool use_gr)
{
    if (bodies.empty())
        return;

    std::vector<vec3> jerk0 = computeJerks(bodies);

    std::vector<CelestialBody> predicted = bodies;
    for (std::size_t i = 0; i < bodies.size(); ++i)
    {
        const vec3& a0 = bodies[i].acceleration;
        const vec3& j0 = jerk0[i];
        predicted[i].position = bodies[i].position + bodies[i].velocity * dt +
                                a0 * (0.5 * dt * dt) + j0 * (dt * dt * dt / 6.0);
        predicted[i].velocity = bodies[i].velocity + a0 * dt + j0 * (0.5 * dt * dt);
    }

    updateAccelerations(predicted, use_gr);
    std::vector<vec3> jerk1 = computeJerks(predicted);

    const double dt2_12 = dt * dt / 12.0;
    for (std::size_t i = 0; i < bodies.size(); ++i)
    {
        const vec3& a0 = bodies[i].acceleration;
        const vec3& a1 = predicted[i].acceleration;
        const vec3& j0 = jerk0[i];
        const vec3& j1 = jerk1[i];

        vec3 vel_new = bodies[i].velocity + 0.5 * dt * (a0 + a1) + dt2_12 * (j0 - j1);
        vec3 pos_new =
            bodies[i].position + 0.5 * dt * (bodies[i].velocity + vel_new) + dt2_12 * (a0 - a1);

        bodies[i].position = pos_new;
        bodies[i].velocity = vel_new;
    }

    updateAccelerations(bodies, use_gr); // leave acceleration current for next call
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

ConservationSnapshot computeSnapshot(const physics::Conservations& C, double E0, double L0,
                                     double P0)
{
    ConservationSnapshot s;
    s.Lmag = std::sqrt(C.L[0] * C.L[0] + C.L[1] * C.L[1] + C.L[2] * C.L[2]);
    s.Pmag = std::sqrt(C.P[0] * C.P[0] + C.P[1] * C.P[1] + C.P[2] * C.P[2]);
    s.dE = (C.total_energy - E0) / std::abs(E0);
    s.dL = (s.Lmag - L0) / (L0 == 0.0 ? 1.0 : L0);
    s.dP = (s.Pmag - P0) / (P0 == 0.0 ? 1.0 : P0);
    s.total_energy = C.total_energy;
    s.kinetic_energy = C.kinetic_energy;
    s.potential_energy = C.potential_energy;
    s.Lx = C.L[0];
    s.Ly = C.L[1];
    s.Lz = C.L[2];
    s.Px = C.P[0];
    s.Py = C.P[1];
    s.Pz = C.P[2];
    return s;
}

void exportCSV(const SimulationResult& result, const std::string& outputPath)
{
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
    if (!conservFile)
        std::cerr << "⚠️ Could not open conservation file: " << conservPath << "\n";

    // Metadata comment
    file << "# dt=" << result.dt << " stride=" << result.stride << " bodies=";
    for (std::size_t i = 0; i < result.body_names.size(); ++i)
    {
        file << result.body_names[i];
        if (!result.body_masses.empty())
            file << ":" << result.body_masses[i];
        if (i + 1 < result.body_names.size())
            file << ",";
    }
    file << "\n";

    // Headers
    if (result.is_adaptive)
    {
        file << "time_s";
        for (const auto& name : result.body_names)
            file << ",x_" << name << ",y_" << name << ",z_" << name;
        file << ",dt_used\n";

        if (conservFile)
            conservFile << "time_s,E_total,KE,PE,Lmag,Pmag,dE_rel,dL_rel,dP_rel\n";
    }
    else
    {
        file << "step";
        for (const auto& name : result.body_names)
            file << ",x_" << name << ",y_" << name << ",z_" << name;
        file << "\n";

        if (conservFile)
            conservFile << "step,E_total,KE,PE,Lx,Ly,Lz,Lmag,Px,Py,Pz,Pmag,dE_rel,dL_rel,dP_rel\n";
    }

    // Data rows
    for (const auto& snap : result.snapshots)
    {
        const auto& c = snap.conservation;

        // Positions row
        if (result.is_adaptive)
        {
            file << snap.time_s;
            for (const auto& pos : snap.positions)
                file << "," << pos.x() << "," << pos.y() << "," << pos.z();
            file << "," << snap.dt_used << "\n";
        }
        else
        {
            file << snap.step;
            for (const auto& pos : snap.positions)
                file << "," << pos.x() << "," << pos.y() << "," << pos.z();
            file << "\n";
        }

        // Conservation row
        if (conservFile)
        {
            if (result.is_adaptive)
            {
                conservFile << snap.time_s << "," << c.total_energy << "," << c.kinetic_energy
                            << "," << c.potential_energy << "," << c.Lmag << "," << c.Pmag << ","
                            << c.dE << "," << c.dL << "," << c.dP << "\n";
            }
            else
            {
                conservFile << snap.step << "," << c.total_energy << "," << c.kinetic_energy << ","
                            << c.potential_energy << "," << c.Lx << "," << c.Ly << "," << c.Lz
                            << "," << c.Lmag << "," << c.Px << "," << c.Py << "," << c.Pz << ","
                            << c.Pmag << "," << c.dE << "," << c.dL << "," << c.dP << "\n";
            }
        }
    }

    file.close();
    if (conservFile)
        conservFile.close();

    std::cout << "✅ Positions:    " << outputPath << "\n";
    std::cout << "✅ Conservation: " << conservPath << "\n";
}

/********************
 * runSimulationCore
 * @brief: Generic N-body simulation runner.
 * @param bodies     - vector of CelestialBody objects (from JSON)
 * @param steps      - number of steps to simulate
 * @param dt         - timestep in seconds
 * @param integrator  - integration method to use (default: RK4)
 * @param stride     - record every Nth step
 * @param use_gr     - if true, apply 1PN GR correction each step
 * @exception none
 * @return SimulationResult
 *********************/
SimulationResult runSimulationCore(std::vector<CelestialBody>& bodies, int steps, double dt,
                                   Integrator integrator, int stride, bool use_gr)
{
    physics::Conservations C0 = physics::compute(bodies);
    double E0 = C0.total_energy;
    double L0 = std::sqrt(C0.L[0] * C0.L[0] + C0.L[1] * C0.L[1] + C0.L[2] * C0.L[2]);
    double P0mag = std::sqrt(C0.P[0] * C0.P[0] + C0.P[1] * C0.P[1] + C0.P[2] * C0.P[2]);

    if (integrator == Integrator::Leapfrog || integrator == Integrator::Yoshida4 ||
        integrator == Integrator::Hermite)
        updateAccelerations(bodies, use_gr);

    SimulationResult result;
    result.dt = dt;
    result.stride = stride;
    result.body_names.reserve(bodies.size());
    result.body_masses.reserve(bodies.size());
    for (const auto& b : bodies)
    {
        result.body_names.push_back(b.name);
        result.body_masses.push_back(b.mass);
    }

    // Use std::function to capture use_gr without changing the step function signatures
    // visible at the call sites (lambdas forward the flag).
    std::function<void(std::vector<CelestialBody>&, double)> stepFn;
    if (integrator == Integrator::Leapfrog)
        stepFn = [use_gr](std::vector<CelestialBody>& b, double d) { leapfrogStep(b, d, use_gr); };
    else if (integrator == Integrator::Yoshida4)
        stepFn = [use_gr](std::vector<CelestialBody>& b, double d) { yoshida4Step(b, d, use_gr); };
    else if (integrator == Integrator::Hermite)
        stepFn = [use_gr](std::vector<CelestialBody>& b, double d) { hermiteStep(b, d, use_gr); };
    else if (integrator == Integrator::RK4)
        stepFn = [use_gr](std::vector<CelestialBody>& b, double d) { rk4Step(b, d, use_gr); };
    else
        throw std::invalid_argument("Unknown integrator");

    for (int i = 0; i < steps; ++i)
    {
        stepFn(bodies, dt);

        if (i % stride != 0)
            continue;

        physics::Conservations C = physics::compute(bodies);
        ConservationSnapshot snap = computeSnapshot(C, E0, L0, P0mag);

        std::vector<vec3> pos;
        pos.reserve(bodies.size());
        for (const auto& b : bodies)
            pos.push_back(b.position);
        result.snapshots.push_back({i, (i + 1) * dt, dt, pos, snap});
    }

    return result;
}

void runSimulation(std::vector<CelestialBody>& bodies, int steps, double dt,
                   const std::string& outputPath, Integrator integrator, int stride, bool use_gr)
{
    if (bodies.empty())
    {
        std::cerr << "❌ No bodies to simulate.\n";
        return;
    }

    SimulationResult result = runSimulationCore(bodies, steps, dt, integrator, stride, use_gr);

    // ── Eclipse logging from stored positions ─────────────────────────────────
    int idxSun = -1, idxEarth = -1, idxMoon = -1;
    for (int i = 0; i < (int)result.body_names.size(); ++i)
    {
        if (result.body_names[i] == "Sun")
            idxSun = i;
        if (result.body_names[i] == "Earth")
            idxEarth = i;
        if (result.body_names[i] == "Moon")
            idxMoon = i;
    }

    if (idxSun >= 0 && idxEarth >= 0 && idxMoon >= 0)
    {
        std::filesystem::path p(outputPath);
        std::string eclipsePath = (p.parent_path() / (p.stem().string() + "_eclipse.csv")).string();
        std::ofstream eclipseFile(eclipsePath);
        if (eclipseFile)
        {
            std::cout << "🌓 Eclipse logging enabled → " << eclipsePath << "\n";
            eclipseFile
                << "step,shadow_x,shadow_y,shadow_z,umbraRadius,penumbraRadius,eclipseType\n";
            for (const auto& snap : result.snapshots)
            {
                EclipseResult e = computeSolarEclipse(
                    snap.positions[idxSun], snap.positions[idxEarth], snap.positions[idxMoon]);
                eclipseFile << snap.step << "," << e.shadowCenter.x() << "," << e.shadowCenter.y()
                            << "," << e.shadowCenter.z() << "," << e.umbraRadius << ","
                            << e.penumbraRadius << "," << e.eclipseType << "\n";
            }
        }
    }

    exportCSV(result, outputPath);
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
    const std::vector<std::pair<double, const std::vector<StateDerivative>*>>& weighted, double dt)
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
double computeErrorNorm(const std::vector<CelestialBody>& y_old,
                        const std::vector<CelestialBody>& y_new,
                        const std::vector<CelestialBody>& err, double atol, double rtol)
{
    double sum = 0.0;
    int n = 0;

    for (std::size_t i = 0; i < y_old.size(); ++i)
    {
        // Position components
        for (int j = 0; j < 3; ++j)
        {
            double yo = y_old[i].position[j];
            double yn = y_new[i].position[j];
            double e = err[i].position[j];
            double sc = atol + rtol * std::max(std::abs(yo), std::abs(yn));
            sum += (e / sc) * (e / sc);
            ++n;
        }

        // Velocity components
        for (int j = 0; j < 3; ++j)
        {
            double yo = y_old[i].velocity[j];
            double yn = y_new[i].velocity[j];
            double e = err[i].velocity[j];
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
 *
 * @param bodies  System state (modified in place if step accepted)
 * @param dt      Proposed timestep
 * @param atol    Absolute tolerance (meters / m/s)
 * @param rtol    Relative tolerance (dimensionless)
 * @param k1_fsal If non-null, reuse as k1 (FSAL from previous accepted step)
 * @param use_gr  If true, apply 1PN GR correction at each derivative evaluation
 * @return        RK45StepResult with acceptance flag and next dt suggestion
 ***********************/
RK45StepResult rk45Step(std::vector<CelestialBody>& bodies, double dt, double atol, double rtol,
                        const std::vector<StateDerivative>* k1_fsal, bool use_gr)
{
    using namespace dp45;

    // ── Stage 1: k1 at current state (skip if FSAL k1 provided) ─────────────
    auto k1 = k1_fsal ? *k1_fsal : evaluateDerivatives(bodies, use_gr);

    // ── Stage 2: k2 at t + C2*dt ─────────────────────────────────────────────
    auto s2 = buildIntermediateState(bodies, k1, dt * A21);
    auto k2 = evaluateDerivatives(s2, use_gr);

    // ── Stage 3: k3 at t + C3*dt ─────────────────────────────────────────────
    auto s3 = buildIntermediateStateMulti(bodies,
                                          {
                                              {A31, &k1},
                                              {A32, &k2},
                                          },
                                          dt);
    auto k3 = evaluateDerivatives(s3, use_gr);

    // ── Stage 4: k4 at t + C4*dt ─────────────────────────────────────────────
    auto s4 = buildIntermediateStateMulti(bodies,
                                          {
                                              {A41, &k1},
                                              {A42, &k2},
                                              {A43, &k3},
                                          },
                                          dt);
    auto k4 = evaluateDerivatives(s4, use_gr);

    // ── Stage 5: k5 at t + C5*dt ─────────────────────────────────────────────
    auto s5 = buildIntermediateStateMulti(bodies,
                                          {
                                              {A51, &k1},
                                              {A52, &k2},
                                              {A53, &k3},
                                              {A54, &k4},
                                          },
                                          dt);
    auto k5 = evaluateDerivatives(s5, use_gr);

    // ── Stage 6: k6 at t + dt ────────────────────────────────────────────────
    auto s6 = buildIntermediateStateMulti(bodies,
                                          {
                                              {A61, &k1},
                                              {A62, &k2},
                                              {A63, &k3},
                                              {A64, &k4},
                                              {A65, &k5},
                                          },
                                          dt);
    auto k6 = evaluateDerivatives(s6, use_gr);

    // ── 5th-order solution y5 ─────────────────────────────────────────────────
    // y5 = y_n + dt*(B1*k1 + B3*k3 + B4*k4 + B5*k5 + B6*k6)
    // B2 = 0 so k2 does not appear
    auto y5 = buildIntermediateStateMulti(bodies,
                                          {
                                              {B1, &k1},
                                              {B3, &k3},
                                              {B4, &k4},
                                              {B5, &k5},
                                              {B6, &k6},
                                          },
                                          dt);

    // ── k7 at y5 (FSAL: this will be k1 of the next step) ────────────────────
    auto k7 = evaluateDerivatives(y5, use_gr);

    // ── Error estimate: y5 - y4 ───────────────────────────────────────────────
    // err = dt*(E1*k1 + E3*k3 + E4*k4 + E5*k5 + E6*k6 + E7*k7)
    // We store the error as a CelestialBody vector for computeErrorNorm
    auto err_state = buildIntermediateStateMulti(bodies,
                                                 {
                                                     {E1, &k1},
                                                     {E3, &k3},
                                                     {E4, &k4},
                                                     {E5, &k5},
                                                     {E6, &k6},
                                                     {E7, &k7},
                                                 },
                                                 dt);

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
    result.dt_next = dt_next;

    if (error_norm <= 1.0)
    {
        // Accept: advance bodies to y5, save k7 for FSAL reuse next step
        bodies = y5;
        result.accepted = true;
        result.dt_used = dt;
        result.k7 = std::move(k7);
    }
    // If rejected: bodies unchanged, caller retries with dt_next (k7 not reusable)

    return result;
}

/***********************
 * runSimulationAdaptiveCore
 * @brief: Adaptive RK45 simulation runner (pure physics, no file I/O).
 *
 * Unlike runSimulationCore() which uses a fixed step count,
 * this runs until t >= duration_s, writing output every
 * output_interval_s seconds of simulated time.
 *
 * @param bodies            System to simulate (modified in place)
 * @param duration_s        Total simulated time in seconds
 * @param dt_initial        Starting timestep guess
 * @param output_interval_s Write a CSV row every N simulated seconds
 * @param atol              Absolute tolerance (m / m/s)
 * @param rtol              Relative tolerance (dimensionless)
 * @param dt_min            Minimum allowed timestep
 * @param dt_max            Maximum allowed timestep
 * @param use_gr            If true, apply 1PN GR correction each step
 ***********************/
SimulationResult runSimulationAdaptiveCore(std::vector<CelestialBody>& bodies, double duration_s,
                                           double dt_initial, double output_interval_s, double atol,
                                           double rtol, double dt_min, double dt_max, bool use_gr)
{
    if (bodies.empty())
    {
        std::cerr << "❌ No bodies to simulate.\n";
        return {};
    }

    physics::Conservations C0 = physics::compute(bodies);
    double E0 = C0.total_energy;
    double L0 = std::sqrt(C0.L[0] * C0.L[0] + C0.L[1] * C0.L[1] + C0.L[2] * C0.L[2]);
    double P0 = std::sqrt(C0.P[0] * C0.P[0] + C0.P[1] * C0.P[1] + C0.P[2] * C0.P[2]);

    SimulationResult sim_result;
    sim_result.dt = dt_initial;
    sim_result.is_adaptive = true;
    for (const auto& b : bodies)
    {
        sim_result.body_names.push_back(b.name);
        sim_result.body_masses.push_back(b.mass);
    }

    double t = 0.0;
    double dt = dt_initial;
    double next_output = 0.0;
    int n_steps = 0;
    int n_rejected = 0;

    // FSAL: reuse k7 from each accepted step as k1 of the next
    std::vector<StateDerivative> fsal_k1;
    bool has_fsal = false;

    while (t < duration_s)
    {
        if (t + dt > duration_s)
            dt = duration_s - t;

        auto step = rk45Step(bodies, dt, atol, rtol, has_fsal ? &fsal_k1 : nullptr, use_gr);

        if (step.accepted)
        {
            t += step.dt_used;
            ++n_steps;
            fsal_k1 = std::move(step.k7);
            has_fsal = true;

            if (t >= next_output)
            {
                physics::Conservations C = physics::compute(bodies);
                ConservationSnapshot snap = computeSnapshot(C, E0, L0, P0);

                std::vector<vec3> pos;
                pos.reserve(bodies.size());
                for (const auto& b : bodies)
                    pos.push_back(b.position);
                sim_result.snapshots.push_back({0, t, step.dt_used, pos, snap});

                next_output += output_interval_s;
            }
        }
        else
        {
            ++n_rejected;
            has_fsal = false; // rejected step: k7 was at unaccepted y5, must recompute k1
        }

        dt = std::clamp(step.dt_next, dt_min, dt_max);
    }

    double reject_pct = 100.0 * n_rejected / std::max(1, n_steps + n_rejected);
    std::cout << "✅ RK45 complete:\n"
              << "   Steps accepted: " << n_steps << "\n"
              << "   Steps rejected: " << n_rejected << " (" << reject_pct << "%)\n"
              << "   Final dt:       " << dt << " s\n";

    return sim_result;
}

void runSimulationAdaptive(std::vector<CelestialBody>& bodies, double duration_s, double dt_initial,
                           const std::string& outputPath, double output_interval_s, double atol,
                           double rtol, double dt_min, double dt_max, bool use_gr)
{
    SimulationResult result = runSimulationAdaptiveCore(
        bodies, duration_s, dt_initial, output_interval_s, atol, rtol, dt_min, dt_max, use_gr);
    exportCSV(result, outputPath);
}
