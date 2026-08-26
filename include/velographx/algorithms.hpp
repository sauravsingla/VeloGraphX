#pragma once
#include "velographx/csr_graph.hpp"
#include <cstdint>
#include <vector>

namespace velographx {

std::vector<std::uint32_t> bfs_distances(const CsrGraph& graph, VertexId source);
std::vector<VertexId> connected_components(const CsrGraph& graph);
std::vector<double> pagerank(const CsrGraph& graph, double damping = 0.85, std::size_t max_iterations = 100, double tolerance = 1e-10);
std::uint64_t triangle_count(const CsrGraph& graph);
std::uint64_t common_neighbor_count(const CsrGraph& graph, VertexId u, VertexId v);
double jaccard_similarity(const CsrGraph& graph, VertexId u, VertexId v);

}
