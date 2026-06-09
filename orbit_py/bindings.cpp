#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>

#include "simulation.h"
#include "json_loader.h"

namespace py = pybind11;

SimulationResult simulate(const std::string& path, int steps, double dt)
{
    auto bodies = loadSystemFromJSON(path);
    return runSimulationCore(bodies, steps, dt);
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
    m.attr("__version__") = "2.0.0-dev";

    py::class_<SimulationSnapshot>(m, "SimulationSnapshot")
        .def_readonly("step", &SimulationSnapshot::step)
        .def_readonly("time_s", &SimulationSnapshot::time_s)
        .def_readonly("positions", &SimulationSnapshot::positions);

    py::class_<SimulationResult>(m, "SimulationResult")
        .def_readonly("body_names", &SimulationResult::body_names)
        .def_readonly("body_masses", &SimulationResult::body_masses)
        .def_readonly("snapshots", &SimulationResult::snapshots)
        .def_readonly("dt", &SimulationResult::dt)
        .def("positions_numpy", &positions_numpy)
        .def("energies_numpy", &energies_numpy);

    m.def("simulate", &simulate, py::arg("path"), py::arg("steps"), py::arg("dt"),
          "Run a simulation from a JSON system file");
}