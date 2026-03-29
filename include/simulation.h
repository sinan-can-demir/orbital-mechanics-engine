/****************
 * Author: Sinan Demir
 * File: simulation.h
 * Date: 10/31/2025
 * Purpose: Header file for simulation.cpp
 *****************/

#ifndef SIMULATION_H
#define SIMULATION_H

#include "body.h"
#include "conservations.h"
#include "eclipse.h"
#include "utils.h"
#include "vec3.h"
#include <cmath>
#include <fstream> // for CSV output
#include <iostream>
#include <vector>

// void computeAcceleration(CelestialBody& earth, const CelestialBody& sun);
void computeGravitationalForce(CelestialBody& a, CelestialBody& b);
void eulerStep(CelestialBody& body, double dt);
void rk4Step(std::vector<CelestialBody>& bodies, double dt);
void runSimulation(std::vector<CelestialBody>& bodies, int steps, double dt,
                   const std::string& outputPath);

#endif // SIMULATION_H
