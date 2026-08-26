#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "velographx/incremental/bfs.hpp"
#include "velographx/incremental/connected_components.hpp"
#include "velographx/incremental/kcore.hpp"
#include "velographx/incremental/pagerank.hpp"
#include "velographx/incremental/triangles.hpp"
#include "velographx/incremental/weighted_sssp.hpp"
#include "velographx/storage/dynamic_graph.hpp"
#include "velographx/storage/weighted_dynamic_graph.hpp"

namespace py = pybind11;
namespace {
velographx::DynamicGraph graph_from_numpy_edges(const py::array_t<std::uint32_t, py::array::c_style | py::array::forcecast>& edges, bool directed) {
  const auto info = edges.request(); if (info.ndim != 2 || info.shape[1] != 2) throw std::invalid_argument("edge array must have shape (N, 2)");
  velographx::DynamicGraph graph(0, directed); velographx::UpdateBatch batch; batch.updates.reserve(static_cast<std::size_t>(info.shape[0]));
  const auto* data = static_cast<const std::uint32_t*>(info.ptr); for (py::ssize_t i=0;i<info.shape[0];++i) batch.add(data[2*i],data[2*i+1]); graph.apply(batch); return graph;
}
velographx::DynamicGraph graph_from_scipy_csr(const py::object& csr, bool directed) {
  if (!py::hasattr(csr,"indptr") || !py::hasattr(csr,"indices") || !py::hasattr(csr,"shape")) throw std::invalid_argument("expected a SciPy CSR-like object with indptr, indices, and shape");
  const auto shape=csr.attr("shape").cast<py::tuple>(); if(shape.size()!=2 || shape[0].cast<py::ssize_t>()!=shape[1].cast<py::ssize_t>()) throw std::invalid_argument("CSR graph matrix must be square");
  const auto vertices=static_cast<std::size_t>(shape[0].cast<py::ssize_t>()); auto indptr=py::array_t<std::int64_t,py::array::c_style|py::array::forcecast>(csr.attr("indptr")); auto indices=py::array_t<std::int64_t,py::array::c_style|py::array::forcecast>(csr.attr("indices")); const auto p=indptr.unchecked<1>(); const auto idx=indices.unchecked<1>();
  if(static_cast<std::size_t>(p.shape(0))!=vertices+1) throw std::invalid_argument("invalid CSR indptr length"); velographx::DynamicGraph graph(vertices,directed); velographx::UpdateBatch batch;
  for(std::size_t u=0;u<vertices;++u){const auto begin=p(static_cast<py::ssize_t>(u)),end=p(static_cast<py::ssize_t>(u+1)); if(begin<0||end<begin||end>idx.shape(0)) throw std::invalid_argument("invalid CSR offsets"); for(auto pos=begin;pos<end;++pos){const auto v=idx(static_cast<py::ssize_t>(pos)); if(v<0||static_cast<std::size_t>(v)>=vertices) throw std::invalid_argument("CSR contains out-of-range vertex id"); if(!directed&&static_cast<std::size_t>(v)<u) continue; batch.add(static_cast<velographx::VertexId>(u),static_cast<velographx::VertexId>(v));}} graph.apply(batch); return graph;
}
py::array arrow_column_to_numpy(const py::object& column){if(!py::hasattr(column,"to_numpy")) throw std::invalid_argument("expected an Arrow-like column exposing to_numpy()"); return py::array::ensure(column.attr("to_numpy")(py::arg("zero_copy_only")=false));}
velographx::DynamicGraph graph_from_arrow_columns(const py::object& src_column,const py::object& dst_column,bool directed){auto src=py::array_t<std::uint32_t,py::array::c_style|py::array::forcecast>(arrow_column_to_numpy(src_column)); auto dst=py::array_t<std::uint32_t,py::array::c_style|py::array::forcecast>(arrow_column_to_numpy(dst_column)); const auto s=src.unchecked<1>(),d=dst.unchecked<1>(); if(s.shape(0)!=d.shape(0)) throw std::invalid_argument("Arrow source and destination columns must have equal length"); velographx::DynamicGraph graph(0,directed); velographx::UpdateBatch batch; for(py::ssize_t i=0;i<s.shape(0);++i) batch.add(s(i),d(i)); graph.apply(batch); return graph;}
velographx::DynamicGraph graph_from_arrow_table(const py::object& table,const std::string& src_name,const std::string& dst_name,bool directed){if(!py::hasattr(table,"column")) throw std::invalid_argument("expected an Arrow-like table exposing column(name)"); return graph_from_arrow_columns(table.attr("column")(src_name),table.attr("column")(dst_name),directed);}
}

PYBIND11_MODULE(velographx,m){
 m.doc()="VeloGraphX CPU-native dynamic graph analytics bindings";
 py::class_<velographx::UpdateBatch>(m,"UpdateBatch").def(py::init<>()).def("add",&velographx::UpdateBatch::add,py::arg("u"),py::arg("v"),py::arg("timestamp")=0).def("remove",&velographx::UpdateBatch::remove,py::arg("u"),py::arg("v"),py::arg("timestamp")=0);
 py::class_<velographx::DynamicGraph>(m,"Graph").def(py::init<std::size_t,bool>(),py::arg("vertices")=0,py::arg("directed")=false).def("add_edge",&velographx::DynamicGraph::add_edge).def("remove_edge",&velographx::DynamicGraph::remove_edge).def("apply",&velographx::DynamicGraph::apply).def("neighbors",&velographx::DynamicGraph::neighbors).def_property_readonly("version",&velographx::DynamicGraph::version).def_property_readonly("vertex_count",&velographx::DynamicGraph::vertex_count).def("compact",&velographx::DynamicGraph::compact);
 py::class_<velographx::IncrementalTriangleCount>(m,"IncrementalTriangleCount").def(py::init<velographx::DynamicGraph&>(),py::keep_alive<1,2>()).def("value",&velographx::IncrementalTriangleCount::value).def("apply",&velographx::IncrementalTriangleCount::apply);
 py::class_<velographx::IncrementalBFS>(m,"IncrementalBFS").def(py::init<velographx::DynamicGraph&,velographx::VertexId>(),py::arg("graph"),py::arg("source"),py::keep_alive<1,2>()).def_property_readonly("distances",[](const velographx::IncrementalBFS& x){return x.distances();}).def("apply",&velographx::IncrementalBFS::apply).def("recompute",&velographx::IncrementalBFS::recompute).def_property_readonly_static("unreachable",[](py::object){return velographx::IncrementalBFS::unreachable;});
 py::class_<velographx::IncrementalComponents>(m,"IncrementalComponents").def(py::init<velographx::DynamicGraph&>(),py::arg("graph"),py::keep_alive<1,2>()).def("component",&velographx::IncrementalComponents::component).def("apply",&velographx::IncrementalComponents::apply).def_property_readonly("last_repaired_vertices",&velographx::IncrementalComponents::last_repaired_vertices);
 py::class_<velographx::IncrementalKCore>(m,"IncrementalKCore").def(py::init<velographx::DynamicGraph&>(),py::arg("graph"),py::keep_alive<1,2>()).def_property_readonly("core",[](const velographx::IncrementalKCore& x){return x.core();}).def("apply",&velographx::IncrementalKCore::apply).def("recompute",&velographx::IncrementalKCore::recompute).def_property_readonly("last_repaired_vertices",&velographx::IncrementalKCore::last_repaired_vertices);
 py::class_<velographx::IncrementalPageRank>(m,"IncrementalPageRank").def(py::init<velographx::DynamicGraph&,double>(),py::arg("graph"),py::arg("damping")=0.85,py::keep_alive<1,2>()).def_property_readonly("values",[](const velographx::IncrementalPageRank& x){return x.values();}).def("apply",&velographx::IncrementalPageRank::apply,py::arg("batch"),py::arg("local_iterations")=24,py::arg("tol")=1e-9,py::arg("full_fallback_fraction")=0.60).def("recompute",&velographx::IncrementalPageRank::recompute,py::arg("iterations")=30).def_property_readonly("last_repaired_vertices",&velographx::IncrementalPageRank::last_repaired_vertices).def_property_readonly("last_repair_iterations",&velographx::IncrementalPageRank::last_repair_iterations);
 py::class_<velographx::WeightedUpdateBatch>(m,"WeightedUpdateBatch").def(py::init<>()).def("add",&velographx::WeightedUpdateBatch::add,py::arg("u"),py::arg("v"),py::arg("weight"),py::arg("timestamp")=0).def("remove",&velographx::WeightedUpdateBatch::remove,py::arg("u"),py::arg("v"),py::arg("timestamp")=0).def("update",&velographx::WeightedUpdateBatch::update,py::arg("u"),py::arg("v"),py::arg("weight"),py::arg("timestamp")=0);
 py::class_<velographx::WeightedDynamicGraph>(m,"WeightedGraph").def(py::init<std::size_t,bool>(),py::arg("vertices")=0,py::arg("directed")=false).def("apply",&velographx::WeightedDynamicGraph::apply).def("weight",&velographx::WeightedDynamicGraph::weight).def("neighbors",&velographx::WeightedDynamicGraph::neighbors).def_property_readonly("version",&velographx::WeightedDynamicGraph::version).def_property_readonly("vertex_count",&velographx::WeightedDynamicGraph::vertex_count).def_property_readonly("directed",&velographx::WeightedDynamicGraph::directed);
 py::class_<velographx::IncrementalWeightedSSSP>(m,"IncrementalWeightedSSSP").def(py::init<velographx::WeightedDynamicGraph&,velographx::VertexId>(),py::arg("graph"),py::arg("source"),py::keep_alive<1,2>()).def_property_readonly("distances",[](const velographx::IncrementalWeightedSSSP& x){return x.distances();}).def("apply",&velographx::IncrementalWeightedSSSP::apply).def("recompute",&velographx::IncrementalWeightedSSSP::recompute).def_property_readonly_static("unreachable",[](py::object){return velographx::IncrementalWeightedSSSP::kInf;});
 m.def("from_numpy_edges",&graph_from_numpy_edges,py::arg("edges"),py::arg("directed")=false); m.def("from_scipy_csr",&graph_from_scipy_csr,py::arg("csr"),py::arg("directed")=false); m.def("from_arrow_columns",&graph_from_arrow_columns,py::arg("src"),py::arg("dst"),py::arg("directed")=false); m.def("from_arrow_table",&graph_from_arrow_table,py::arg("table"),py::arg("src")="src",py::arg("dst")="dst",py::arg("directed")=false);
}
