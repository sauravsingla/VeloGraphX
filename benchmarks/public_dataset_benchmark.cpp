#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <numeric>
#include <string>
#include <unordered_set>

#include "velographx/algorithms.hpp"
#include "velographx/io.hpp"

int main(int argc, char** argv) {
  if (argc < 2 || argc > 3) {
    std::cerr << "usage: velographx_public_dataset_benchmark <edge-list> [source]\n";
    return 2;
  }

  const std::filesystem::path dataset = argv[1];
  const velographx::VertexId source = argc == 3
      ? static_cast<velographx::VertexId>(std::stoul(argv[2]))
      : 0;
  using clock = std::chrono::steady_clock;

  const auto load_begin = clock::now();
  const auto graph = velographx::load_edge_list(dataset, false);
  const auto load_end = clock::now();

  const auto bfs_begin = clock::now();
  const auto distances = velographx::bfs_distances(graph, source);
  const auto bfs_end = clock::now();
  const auto reachable = std::count_if(distances.begin(), distances.end(), [](std::uint32_t d) {
    return d != std::numeric_limits<std::uint32_t>::max();
  });

  const auto cc_begin = clock::now();
  const auto components = velographx::connected_components(graph);
  const auto cc_end = clock::now();
  const std::unordered_set<velographx::VertexId> component_labels(components.begin(), components.end());
  const auto component_count = component_labels.size();

  const auto triangle_begin = clock::now();
  const auto triangles = velographx::triangle_count(graph);
  const auto triangle_end = clock::now();

  const auto pagerank_begin = clock::now();
  const auto ranks = velographx::pagerank(graph);
  const auto pagerank_end = clock::now();
  const double rank_sum = std::accumulate(ranks.begin(), ranks.end(), 0.0);

  const auto us = [](auto begin, auto end) {
    return std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
  };

  std::cout
      << "dataset,source,vertices,edge_entries,load_us,bfs_us,reachable_vertices,components_us,component_count,triangle_us,triangles,pagerank_us,pagerank_sum\n"
      << dataset.string() << ',' << source << ',' << graph.vertex_count() << ',' << graph.edge_entry_count() << ','
      << us(load_begin, load_end) << ',' << us(bfs_begin, bfs_end) << ',' << reachable << ','
      << us(cc_begin, cc_end) << ',' << component_count << ','
      << us(triangle_begin, triangle_end) << ',' << triangles << ','
      << us(pagerank_begin, pagerank_end) << ',' << rank_sum << '\n';
  return 0;
}
