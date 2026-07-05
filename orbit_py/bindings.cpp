#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>

#include "simulation.h"
#include "json_loader.h"

namespace py = pybind11;

SimulationResult simulate(const std::string& path, int steps, double dt,
                          Integrator integrator = Integrator::RK4, int stride = 1, bool gr = false)
{
    auto bodies = loadSystemFromJSON(path);
    return runSimulationCore(bodies, steps, dt, integrator, stride, gr);
}

SimulationResult simulate_adaptive(const std::string& path, double duration_s, double dt_initial,
                                   double output_interval_s = 3600.0, double atol = 1e-9,
                                   double rtol = 1e-9, double dt_min = 1.0, double dt_max = 86400.0,
                                   bool gr = false)
{
    auto bodies = loadSystemFromJSON(path);
    return runSimulationAdaptiveCore(bodies, duration_s, dt_initial, output_interval_s, atol, rtol,
                                     dt_min, dt_max, gr);
}

py::array_t<double> positions_numpy(const SimulationResult& result)
{
    size_t n_steps = result.snapshots.size();
    size_t n_bodies = result.body_names.size();

    py::array_t<double> arr({n_steps, n_bodies, size_t(3)});
    auto buf = arr.mutable_unchecked<3>();

    for (size_t s = 0; s < n_steps; ++s)
        for (size_t b = 0; b < n_bodies; ++b)
        {
            buf(s, b, 0) = result.snapshots[s].positions[b].x();
            buf(s, b, 1) = result.snapshots[s].positions[b].y();
            buf(s, b, 2) = result.snapshots[s].positions[b].z();
        }

    return arr;
}

py::array_t<double> energies_numpy(const SimulationResult& result)
{
    py::ssize_t n_steps = static_cast<py::ssize_t>(result.snapshots.size());
    py::array_t<double> arr({n_steps});
    auto buf = arr.mutable_unchecked<1>();
    for (size_t s = 0; s < n_steps; ++s)
        buf(s) = result.snapshots[s].conservation.total_energy;
    return arr;
}

PYBIND11_MODULE(orbit, m)
{
    m.doc() = "Orbital Mechanics Engine — Python bindings";
    m.attr("__version__") = "2.0.0";

    py::enum_<Integrator>(m, "Integrator")
        .value("RK4", Integrator::RK4)
        .value("Leapfrog", Integrator::Leapfrog)
        .value("RK45", Integrator::RK45)
        .value("Euler", Integrator::euler)
        .value("Yoshida4", Integrator::Yoshida4)
        .value("Hermite", Integrator::Hermite)
        .export_values();

    py::class_<ConservationSnapshot>(m, "ConservationSnapshot")
        .def_readonly("dE", &ConservationSnapshot::dE)
        .def_readonly("dL", &ConservationSnapshot::dL)
        .def_readonly("dP", &ConservationSnapshot::dP)
        .def_readonly("total_energy", &ConservationSnapshot::total_energy)
        .def_readonly("kinetic_energy", &ConservationSnapshot::kinetic_energy)
        .def_readonly("potential_energy", &ConservationSnapshot::potential_energy)
        .def_readonly("Lmag", &ConservationSnapshot::Lmag)
        .def_readonly("Pmag", &ConservationSnapshot::Pmag)
        .def_readonly("Lx", &ConservationSnapshot::Lx)
        .def_readonly("Ly", &ConservationSnapshot::Ly)
        .def_readonly("Lz", &ConservationSnapshot::Lz)
        .def_readonly("Px", &ConservationSnapshot::Px)
        .def_readonly("Py", &ConservationSnapshot::Py)
        .def_readonly("Pz", &ConservationSnapshot::Pz);

    py::class_<SimulationSnapshot>(m, "SimulationSnapshot")
        .def_readonly("step", &SimulationSnapshot::step)
        .def_readonly("time_s", &SimulationSnapshot::time_s)
        .def_readonly("dt_used", &SimulationSnapshot::dt_used)
        .def_readonly("positions", &SimulationSnapshot::positions)
        .def_readonly("conservation", &SimulationSnapshot::conservation);

    py::class_<SimulationResult>(m, "SimulationResult")
        .def_readonly("body_names", &SimulationResult::body_names)
        .def_readonly("body_masses", &SimulationResult::body_masses)
        .def_readonly("snapshots", &SimulationResult::snapshots)
        .def_readonly("dt", &SimulationResult::dt)
        .def("positions_numpy", &positions_numpy)
        .def("energies_numpy", &energies_numpy);

    m.def("simulate", &simulate, py::arg("path"), py::arg("steps"), py::arg("dt"),
          py::arg("integrator") = Integrator::RK4, py::arg("stride") = 1, py::arg("gr") = false,
          "Run a fixed-step simulation from a JSON system file");

    m.def("simulate_adaptive", &simulate_adaptive, py::arg("path"), py::arg("duration_s"),
          py::arg("dt_initial"), py::arg("output_interval_s") = 3600.0, py::arg("atol") = 1e-9,
          py::arg("rtol") = 1e-9, py::arg("dt_min") = 1.0, py::arg("dt_max") = 86400.0,
          py::arg("gr") = false, "Run an adaptive RK45 simulation from a JSON system file");
}