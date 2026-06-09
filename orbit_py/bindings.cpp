#include <pybind11/pybind11.h>

namespace py = pybind11;

PYBIND11_MODULE(orbit, m)
{
    m.doc() = "Orbital Mechanics Engine — Python bindings";
    m.attr("__version__") = "2.0.0-dev";
}