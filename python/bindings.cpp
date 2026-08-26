#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "velographx/incremental/triangles.hpp"
#include "velographx/runtime/execution_plan.hpp"
#include "velographx/storage/dynamic_graph.hpp"

namespace py = pybind11;

namespace {

velographx::DynamicGraph graph_from_numpy_edges(
    const py::array_t<std::uint32_t, py::array::c_style | py::array::forcecast>& edges,
    bool directed) {
  const auto info = edges.request();
  if (info.ndim != 2 || info.shape[1] != 2) {
    throw std::invalid_argument("edge array must have shape (N, 2)");
  }

  velographx::DynamicGraph graph(0, directed);
  velographx::UpdateBatch batch;
  batch.updates.reserve(static_cast<std::size_t>(info.shape[0]));

  const auto* data = static_cast<const std::uint32_t*>(info.ptr);
  for (py::ssize_t i = 0; i < info.shape[0]; ++i) {
    batch.add(data[2 * i], data[2 * i + 1]);
  }
  graph.apply(batch);
  return graph;
}

velographx::DynamicGraph graph_from_scipy_csr(const py::object& csr, bool directed) {
  if (!py::hasattr(csr, "indptr") || !py::hasattr(csr, "indices") ||
      !py::hasattr(csr, "shape")) {
    throw std::invalid_argument("expected a SciPy CSR-like object with indptr, indices, and shape");
  }

  const auto shape = csr.attr("shape").cast<py::tuple>();
  if (shape.size() != 2 || shape[0].cast<py::ssize_t>() != shape[1].cast<py::ssize_t>()) {
    throw std::invalid_argument("CSR graph matrix must be square");
  }

  const auto vertices = static_cast<std::size_t>(shape[0].cast<py::ssize_t>());
  auto indptr = py::array_t<std::int64_t, py::array::c_style | py::array::forcecast>(csr.attr("indptr"));
  auto indices = py::array_t<std::int64_t, py::array::c_style | py::array::forcecast>(csr.attr("indices"));
  const auto p = indptr.unchecked<1>();
  const auto idx = indices.unchecked<1>();

  if (static_cast<std::size_t>(p.shape(0)) != vertices + 1) {
    throw std::invalid_argument("invalid CSR indptr length");
  }

  velographx::DynamicGraph graph(vertices, directed);
  velographx::UpdateBatch batch;
  batch.updates.reserve(static_cast<std::size_t>(idx.shape(0)));

  for (std::size_t u = 0; u < vertices; ++u) {
    const auto begin = p(static_cast<py::ssize_t>(u));
    const auto end = p(static_cast<py::ssize_t>(u + 1));
    if (begin < 0 || end < begin || end > idx.shape(0)) {
      throw std::invalid_argument("invalid CSR offsets");
    }
    for (auto pos = begin; pos < end; ++pos) {
      const auto v = idx(static_cast<py::ssize_t>(pos));
      if (v < 0 || static_cast<std::size_t>(v) >= vertices) {
        throw std::invalid_argument("CSR contains out-of-range vertex id");
      }
      if (!directed && static_cast<std::size_t>(v) < u) continue;
      batch.add(static_cast<velographx::VertexId>(u), static_cast<velographx::VertexId>(v));
    }
  }
  graph.apply(batch);
  return graph;
}

py::array arrow_column_to_numpy(const py::object& column) {
  if (!py::hasattr(column, "to_numpy")) {
    throw std::invalid_argument("expected an Arrow-like column exposing to_numpy()");
  }
  py::object array = column.attr("to_numpy")(py::arg("zero_copy_only") = false);
  return py::array::ensure(array);
}

velographx::DynamicGraph graph_from_arrow_columns(const py::object& src_column,
                                                   const py::object& dst_column,
                                                   bool directed) {
  auto src = py::array_t<std::uint32_t, py::array::c_style | py::array::forcecast>(
      arrow_column_to_numpy(src_column));
  auto dst = py::array_t<std::uint32_t, py::array::c_style | py::array::forcecast>(
      arrow_column_to_numpy(dst_column));
  const auto s = src.unchecked<1>();
  const auto d = dst.unchecked<1>();
  if (s.shape(0) != d.shape(0)) {
    throw std::invalid_argument("Arrow source and destination columns must have equal length");
  }

  velographx::DynamicGraph graph(0, directed);
  velographx::UpdateBatch batch;
  batch.updates.reserve(static_cast<std::size_t>(s.shape(0)));
  for (py::ssize_t i = 0; i < s.shape(0); ++i) {
    batch.add(s(i), d(i));
  }
  graph.apply(batch);
  return graph;
}

velographx::DynamicGraph graph_from_arrow_table(const py::object& table,
                                                 const std::string& src_name,
                                                 const std::string& dst_name,
                                                 bool directed) {
  if (!py::hasattr(table, "column")) {
    throw std::invalid_argument("expected an Arrow-like table exposing column(name)");
  }
  return graph_from_arrow_columns(table.attr("column")(src_name), table.attr("column")(dst_name), directed);
}

}  // namespace

PYBIND11_MODULE(velographx, m) {
  m.doc() = "VeloGraphX CPU-native dynamic graph analytics bindings";

  py::class_<velographx::UpdateBatch>(m, "UpdateBatch")
      .def(py::init<>())
      .def("add", &velographx::UpdateBatch::add, py::arg("u"), py::arg("v"),
           py::arg("timestamp") = 0)
      .def("remove", &velographx::UpdateBatch::remove, py::arg("u"), py::arg("v"),
           py::arg("timestamp") = 0);

  py::class_<velographx::DynamicGraph>(m, "Graph")
      .def(py::init<std::size_t, bool>(), py::arg("vertices") = 0,
           py::arg("directed") = false)
      .def("add_edge", &velographx::DynamicGraph::add_edge)
      .def("remove_edge", &velographx::DynamicGraph::remove_edge)
      .def("apply", &velographx::DynamicGraph::apply)
      .def("neighbors", &velographx::DynamicGraph::neighbors)
      .def_property_readonly("version", &velographx::DynamicGraph::version)
      .def_property_readonly("vertex_count", &velographx::DynamicGraph::vertex_count)
      .def("compact", &velographx::DynamicGraph::compact);

  py::class_<velographx::IncrementalTriangleCount>(m, "IncrementalTriangleCount")
      .def(py::init<velographx::DynamicGraph&>(), py::keep_alive<1, 2>())
      .def("value", &velographx::IncrementalTriangleCount::value)
      .def("apply", &velographx::IncrementalTriangleCount::apply);

  m.def("from_numpy_edges", &graph_from_numpy_edges, py::arg("edges"),
        py::arg("directed") = false,
        "Create a graph from a contiguous NumPy uint32 edge array of shape (N, 2). "
        "The binding reads directly from the NumPy buffer and only copies into graph update storage.");
  m.def("from_scipy_csr", &graph_from_scipy_csr, py::arg("csr"),
        py::arg("directed") = false,
        "Create a graph from a SciPy CSR-like matrix using its indptr/indices buffers without "
        "materializing a Python edge list.");
  m.def("from_arrow_columns", &graph_from_arrow_columns, py::arg("src"), py::arg("dst"),
        py::arg("directed") = false,
        "Create a graph from Arrow-like source and destination columns. The binding asks Arrow for "
        "NumPy-compatible contiguous views where possible; Arrow may copy for unsupported/chunked layouts.");
  m.def("from_arrow_table", &graph_from_arrow_table, py::arg("table"), py::arg("src") = "src",
        py::arg("dst") = "dst", py::arg("directed") = false,
        "Create a graph from two named Arrow table columns. This is low-copy when Arrow can expose "
        "contiguous NumPy views and otherwise performs the minimum conversion required by Arrow.");
}
