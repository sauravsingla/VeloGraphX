#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

#include "velographx/incremental/bfs.hpp"
#include "velographx/storage/dynamic_graph.hpp"

using velographx::DynamicGraph;
using velographx::IncrementalBFS;
using velographx::UpdateBatch;
using velographx::VertexId;

static std::vector<std::pair<std::uint64_t,std::uint64_t>> read_edges(const char* path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("cannot open input");
  std::vector<std::pair<std::uint64_t,std::uint64_t>> edges;
  std::uint64_t u, v;
  while (in.read(reinterpret_cast<char*>(&u), sizeof(u))) {
    if (!in.read(reinterpret_cast<char*>(&v), sizeof(v))) throw std::runtime_error("truncated pair");
    edges.emplace_back(u, v);
  }
  return edges;
}

static std::uint64_t digest(const std::vector<std::uint32_t>& d) {
  std::uint64_t h = 1469598103934665603ULL;
  for (std::size_t i=0;i<d.size();++i) {
    h ^= (static_cast<std::uint64_t>(d[i]) << 32) ^ i;
    h *= 1099511628211ULL;
  }
  return h;
}

int main(int argc, char** argv) {
  if (argc != 5) {
    std::cerr << "usage: " << argv[0] << " graph root imported_rate batch_size\n";
    return 2;
  }
  const auto raw = read_edges(argv[1]);
  const auto root = static_cast<VertexId>(std::stoull(argv[2]));
  const double imported_rate = std::stod(argv[3]);
  const std::size_t batch = std::stoull(argv[4]);
  std::uint64_t maxv = 0;
  for (auto [u,v] : raw) maxv = std::max(maxv, std::max(u,v));
  if (maxv > std::numeric_limits<VertexId>::max()) throw std::runtime_error("vertex id exceeds VeloGraphX domain");
  const std::size_t imported = static_cast<std::size_t>(raw.size() * imported_rate);

  std::vector<std::pair<VertexId,VertexId>> initial;
  initial.reserve(imported);
  for (std::size_t i=0;i<imported;++i) initial.emplace_back(static_cast<VertexId>(raw[i].first), static_cast<VertexId>(raw[i].second));
  DynamicGraph graph(static_cast<std::size_t>(maxv)+1, true);
  graph.bulk_load_edges(initial);
  IncrementalBFS bfs(graph, root);

  std::size_t batches = 0;
  const auto start = std::chrono::steady_clock::now();
  for (std::size_t begin=imported; begin<raw.size(); begin+=batch) {
    const auto end = std::min(begin+batch, raw.size());
    UpdateBatch add;
    for (std::size_t i=begin;i<end;++i) add.add(static_cast<VertexId>(raw[i].first), static_cast<VertexId>(raw[i].second));
    if (!add.empty()) bfs.apply(add);

    UpdateBatch del;
    for (std::size_t i=begin;i<end;++i) {
      const std::size_t old = i - imported;
      if (old >= raw.size()) break;
      del.remove(static_cast<VertexId>(raw[old].first), static_cast<VertexId>(raw[old].second));
    }
    if (!del.empty()) bfs.apply(del);
    ++batches;
  }
  const auto stop = std::chrono::steady_clock::now();

  IncrementalBFS reference(graph, root);
  const bool correct = bfs.distances() == reference.distances();
  std::vector<std::size_t> layers;
  for (auto x : bfs.distances()) {
    if (x == IncrementalBFS::unreachable) continue;
    if (layers.size() <= x) layers.resize(static_cast<std::size_t>(x)+1);
    ++layers[x];
  }
  const auto wall_us = std::chrono::duration_cast<std::chrono::microseconds>(stop-start).count();
  std::cout << "{\"system\":\"VeloGraphX\",\"correct\":" << (correct?"true":"false")
            << ",\"batches\":" << batches << ",\"wall_us\":" << wall_us
            << ",\"mean_batch_us\":" << (batches ? static_cast<double>(wall_us)/batches : 0.0)
            << ",\"distance_digest\":" << digest(bfs.distances()) << ",\"layers\":[";
  for (std::size_t i=0;i<layers.size();++i) { if (i) std::cout << ','; std::cout << layers[i]; }
  std::cout << "]}\n";
  return correct ? 0 : 3;
}
