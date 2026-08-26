#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "velographx/storage/dynamic_graph.hpp"
#include "velographx/incremental/triangles.hpp"
#include "velographx/runtime/execution_plan.hpp"
namespace py=pybind11;
PYBIND11_MODULE(velographx,m){
  py::class_<velographx::UpdateBatch>(m,"UpdateBatch").def(py::init<>()).def("add",&velographx::UpdateBatch::add,py::arg("u"),py::arg("v"),py::arg("timestamp")=0).def("remove",&velographx::UpdateBatch::remove,py::arg("u"),py::arg("v"),py::arg("timestamp")=0);
  py::class_<velographx::DynamicGraph>(m,"Graph").def(py::init<std::size_t,bool>(),py::arg("vertices")=0,py::arg("directed")=false).def("add_edge",&velographx::DynamicGraph::add_edge).def("remove_edge",&velographx::DynamicGraph::remove_edge).def("apply",&velographx::DynamicGraph::apply).def("neighbors",&velographx::DynamicGraph::neighbors).def_property_readonly("version",&velographx::DynamicGraph::version).def_property_readonly("vertex_count",&velographx::DynamicGraph::vertex_count).def("compact",&velographx::DynamicGraph::compact);
  py::class_<velographx::IncrementalTriangleCount>(m,"IncrementalTriangleCount").def(py::init<velographx::DynamicGraph&>(),py::keep_alive<1,2>()).def("value",&velographx::IncrementalTriangleCount::value).def("apply",&velographx::IncrementalTriangleCount::apply);
}
