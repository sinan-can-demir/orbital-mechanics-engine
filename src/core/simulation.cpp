/****************
 * Author: Sinan Demir
 * File: simulation.cpp
 * Date: 10/31/2025
 * Purpose: Implementation file of simulation
 *****************/

#include "simulation.h"
#include "eclipse.h"
#include "vec3.h"
#include <filesystem>

/****************
 * struct StateDerivative
 * Purpose: Captures instantaneous derivatives for position and velocity
 * components.
 *****************/
struct StateDerivative
{
    vec3 dpos; ///< Time derivative of position (velocity)
    vec3 dvel; ///< Time derivative of velocity (acceleration)
};

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
    std::string conservPath = (outPath.parent_path()
                              / (outPath.stem().string() + "_conservation.csv")).string();

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
        file << ",x_" << b.name
             << ",y_" << b.name
             << ",z_" << b.name;
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

        double Lmag = std::sqrt(C.L[0] * C.L[0] + C.L[1] * C.L[1] + C.L[2] * C.L[2]);

        double Pmag = std::sqrt(C.P[0] * C.P[0] + C.P[1] * C.P[1] + C.P[2] * C.P[2]);

        double dE = (C.total_energy - E0) / std::abs(E0);
        double dL = (Lmag - L0) / L0;
        double dP = (Pmag - P0mag) / (P0mag == 0 ? 1.0 : P0mag);

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
            file << "," << b.position.x()
                 << "," << b.position.y()
                 << "," << b.position.z();
        file << "\n";

        // Conservation row — write less frequently
        if (conservFile && i % (stride * 10) == 0)
        {
            conservFile << i
                        << "," << C.total_energy
                        << "," << C.kinetic_energy
                        << "," << C.potential_energy
                        << "," << C.L[0]
                        << "," << C.L[1]
                        << "," << C.L[2]
                        << "," << Lmag
                        << "," << C.P[0]
                        << "," << C.P[1]
                        << "," << C.P[2]
                        << "," << Pmag
                        << "," << dE
                        << "," << dL
                        << "," << dP
                        << "\n";
        }
    }

    file.close();
    if (conservFile)
        conservFile.close();
    
    std::cout << "✅ Positions:     " << outputPath  << "\n";
    std::cout << "✅ Conservation:  " << conservPath << "\n";    if (isSEM)
    {
        eclipseFile.close();
    }

    std::cout << "✅ Simulation complete: " << outputPath << "\n";
}