#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <numeric>
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>
#ifdef __linux__
#include <malloc.h>
#endif

#include "velographx/storage/dynamic_graph.hpp"

namespace {
using velographx::EdgeUpdate;
using velographx::UpdateBatch;
using velographx::VertexId;
using Clock = std::chrono::steady_clock;

std::size_t rss_kb() {
#ifdef __linux__
  std::ifstream in("/proc/self/status");
  std::string key;
  while (in >> key) {
    if (key == "VmRSS:") {
      std::size_t value = 0;
      std::string unit;
      in >> value >> unit;
      return value;
    }
    std::string rest;
    std::getline(in, rest);
  }
#endif
  return 0;
}

void trim_allocator() {
#ifdef __linux__
  malloc_trim(0);
#endif
}

double elapsed_ms(Clock::time_point begin, Clock::time_point end) {
  return std::chrono::duration<double, std::milli>(end - begin).count();
}

std::vector<std::pair<VertexId, VertexId>> make_edges(std::uint64_t target_edges,
                                                       std::uint32_t degree) {
  if (degree == 0 || target_edges < degree) throw std::invalid_argument("invalid graph size");
  const std::uint64_t vertices64 = (target_edges + degree - 1) / degree;
  if (vertices64 > std::numeric_limits<VertexId>::max()) throw std::invalid_argument("too many vertices");
  const auto vertices = static_cast<VertexId>(vertices64);
  std::vector<std::pair<VertexId, VertexId>> edges;
  edges.reserve(static_cast<std::size_t>(target_edges));
  for (VertexId u = 0; u < vertices && edges.size() < target_edges; ++u) {
    for (std::uint32_t d = 1; d <= degree && edges.size() < target_edges; ++d) {
      const auto v = static_cast<VertexId>((static_cast<std::uint64_t>(u) + d) % vertices);
      if (u != v) edges.emplace_back(u, v);
    }
  }
  return edges;
}

UpdateBatch make_updates(std::size_t vertices, std::uint32_t degree, std::size_t count) {
  UpdateBatch batch;
  batch.updates.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    const auto u = static_cast<VertexId>((i * 2654435761ULL) % vertices);
    if ((i & 1U) == 0) {
      const auto d = static_cast<std::uint32_t>(1 + (i % degree));
      const auto v = static_cast<VertexId>((static_cast<std::uint64_t>(u) + d) % vertices);
      batch.remove(u, v);
    } else {
      const auto d = static_cast<std::uint32_t>(degree + 1 + (i % degree));
      const auto v = static_cast<VertexId>((static_cast<std::uint64_t>(u) + d) % vertices);
      batch.add(u, v);
    }
  }
  return batch;
}

std::vector<VertexId> probe_vertices(std::size_t vertices, std::size_t count) {
  std::vector<VertexId> probes;
  probes.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    probes.push_back(static_cast<VertexId>((i * 11400714819323198485ULL) % vertices));
  }
  return probes;
}

std::uint64_t mix_row(std::uint64_t seed, VertexId u, std::span<const VertexId> row) {
  std::uint64_t h = seed ^ (static_cast<std::uint64_t>(u) + 0x9e3779b97f4a7c15ULL);
  h ^= static_cast<std::uint64_t>(row.size()) + (h << 6) + (h >> 2);
  for (auto v : row) h ^= static_cast<std::uint64_t>(v) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
  return h;
}

class LegacyDynamicGraph {
 public:
  explicit LegacyDynamicGraph(std::size_t vertices = 0)
      : base_(vertices), delta_add_(vertices), delta_del_(vertices) {}

  std::size_t vertex_count() const noexcept { return base_.size(); }

  void ensure_vertex(VertexId v) {
    const auto n = static_cast<std::size_t>(v) + 1;
    if (n > base_.size()) {
      base_.resize(n);
      delta_add_.resize(n);
      delta_del_.resize(n);
    }
  }

  void bulk_load_edges(const std::vector<std::pair<VertexId, VertexId>>& edges) {
    VertexId max_vertex = 0;
    bool saw = false;
    for (const auto& [u, v] : edges) {
      if (u == v) continue;
      max_vertex = std::max(max_vertex, std::max(u, v));
      saw = true;
    }
    if (saw) ensure_vertex(max_vertex);
    for (auto& row : base_) row.clear();
    for (auto& row : delta_add_) row.clear();
    for (auto& row : delta_del_) row.clear();
    std::vector<std::size_t> degrees(base_.size(), 0);
    for (const auto& [u, v] : edges) if (u != v) ++degrees[u];
    for (std::size_t u = 0; u < base_.size(); ++u) base_[u].reserve(degrees[u]);
    for (const auto& [u, v] : edges) if (u != v) base_[u].push_back(v);
    for (auto& row : base_) {
      std::sort(row.begin(), row.end());
      row.erase(std::unique(row.begin(), row.end()), row.end());
    }
  }

  void apply(const UpdateBatch& batch) {
    for (const auto& op : batch.updates) {
      ensure_vertex(std::max(op.src, op.dst));
      apply_one(op.src, op.dst, op.add);
    }
  }

  std::vector<VertexId> neighbors(VertexId u) const {
    if (u >= base_.size()) return {};
    std::vector<VertexId> out;
    out.reserve(base_[u].size() + delta_add_[u].size());
    for (auto v : base_[u]) if (!delta_del_[u].contains(v)) out.push_back(v);
    for (auto v : delta_add_[u]) if (!delta_del_[u].contains(v)) out.push_back(v);
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
  }

  void compact() {
    for (VertexId u = 0; u < base_.size(); ++u) {
      if (delta_add_[u].empty() && delta_del_[u].empty()) continue;
      base_[u] = neighbors(u);
      delta_add_[u].clear();
      delta_del_[u].clear();
    }
  }

  std::size_t edge_count() const {
    std::size_t total = 0;
    for (const auto& row : base_) total += row.size();
    return total;
  }

 private:
  void apply_one(VertexId u, VertexId v, bool add) {
    if (add) {
      delta_del_[u].erase(v);
      if (!std::binary_search(base_[u].begin(), base_[u].end(), v)) delta_add_[u].insert(v);
    } else {
      delta_add_[u].erase(v);
      if (std::binary_search(base_[u].begin(), base_[u].end(), v)) delta_del_[u].insert(v);
    }
  }

  std::vector<std::vector<VertexId>> base_;
  std::vector<std::unordered_set<VertexId>> delta_add_;
  std::vector<std::unordered_set<VertexId>> delta_del_;
};

template <class Graph>
std::pair<double, std::uint64_t> neighbor_probe(Graph& graph, const std::vector<VertexId>& probes) {
  std::uint64_t checksum = 1469598103934665603ULL;
  const auto begin = Clock::now();
  for (auto u : probes) {
    const auto row = graph.neighbors(u);
    checksum = mix_row(checksum, u, row);
  }
  const auto end = Clock::now();
  const auto ns = std::chrono::duration<double, std::nano>(end - begin).count();
  return {ns / static_cast<double>(probes.size()), checksum};
}

void print_common(const std::string& design, std::uint64_t target_edges, std::size_t vertices,
                  std::uint32_t degree, double load_ms, std::size_t loaded_rss,
                  std::size_t storage_bytes, std::size_t updates, double update_ms,
                  std::size_t update_rss, double neighbor_ns, std::uint64_t checksum,
                  double compact_ms, std::size_t compact_rss, std::size_t final_edges) {
  const double ups = update_ms > 0.0 ? static_cast<double>(updates) * 1000.0 / update_ms : 0.0;
  std::cout << "{\"design\":\"" << design << "\",\"target_edges\":" << target_edges
            << ",\"vertices\":" << vertices << ",\"degree\":" << degree
            << ",\"bulk_load_ms\":" << load_ms << ",\"loaded_rss_kb\":" << loaded_rss
            << ",\"reported_storage_bytes\":" << storage_bytes
            << ",\"updates\":" << updates << ",\"update_ms\":" << update_ms
            << ",\"updates_per_second\":" << ups << ",\"rss_after_updates_kb\":" << update_rss
            << ",\"neighbor_ns_per_probe\":" << neighbor_ns << ",\"neighbor_checksum\":" << checksum
            << ",\"compaction_ms\":" << compact_ms << ",\"rss_after_compaction_kb\":" << compact_rss
            << ",\"final_edges\":" << final_edges << "}\n";
}

void run_current(std::uint64_t target_edges, std::uint32_t degree, std::size_t update_count,
                 std::size_t probes_count) {
  auto edges = make_edges(target_edges, degree);
  const auto vertices = static_cast<std::size_t>((target_edges + degree - 1) / degree);
  velographx::DynamicGraph graph(vertices, true);
  const auto t0 = Clock::now();
  graph.bulk_load_edges(edges);
  const auto t1 = Clock::now();
  edges.clear(); edges.shrink_to_fit(); trim_allocator();
  const auto loaded_rss = rss_kb();
  const auto storage_bytes = graph.storage_bytes();

  auto updates = make_updates(vertices, degree, update_count);
  const auto u0 = Clock::now();
  graph.apply(updates);
  const auto u1 = Clock::now();
  updates.updates.clear(); updates.updates.shrink_to_fit(); trim_allocator();
  const auto update_rss = rss_kb();
  const auto [neighbor_ns, checksum] = neighbor_probe(graph, probe_vertices(vertices, probes_count));

  const auto c0 = Clock::now();
  graph.compact();
  const auto c1 = Clock::now();
  trim_allocator();
  print_common("segmented-packed-reverse", target_edges, vertices, degree,
               elapsed_ms(t0, t1), loaded_rss, storage_bytes, update_count,
               elapsed_ms(u0, u1), update_rss, neighbor_ns, checksum,
               elapsed_ms(c0, c1), rss_kb(), graph.edge_count_directed());
}

void run_legacy(std::uint64_t target_edges, std::uint32_t degree, std::size_t update_count,
                std::size_t probes_count) {
  auto edges = make_edges(target_edges, degree);
  const auto vertices = static_cast<std::size_t>((target_edges + degree - 1) / degree);
  LegacyDynamicGraph graph(vertices);
  const auto t0 = Clock::now();
  graph.bulk_load_edges(edges);
  const auto t1 = Clock::now();
  edges.clear(); edges.shrink_to_fit(); trim_allocator();
  const auto loaded_rss = rss_kb();

  auto updates = make_updates(vertices, degree, update_count);
  const auto u0 = Clock::now();
  graph.apply(updates);
  const auto u1 = Clock::now();
  updates.updates.clear(); updates.updates.shrink_to_fit(); trim_allocator();
  const auto update_rss = rss_kb();
  const auto [neighbor_ns, checksum] = neighbor_probe(graph, probe_vertices(vertices, probes_count));

  const auto c0 = Clock::now();
  graph.compact();
  const auto c1 = Clock::now();
  trim_allocator();
  print_common("legacy-vector-hash", target_edges, vertices, degree,
               elapsed_ms(t0, t1), loaded_rss, 0, update_count,
               elapsed_ms(u0, u1), update_rss, neighbor_ns, checksum,
               elapsed_ms(c0, c1), rss_kb(), graph.edge_count());
}

void run_reverse(std::uint64_t target_edges, std::uint32_t degree) {
  auto edges = make_edges(target_edges, degree);
  const auto vertices = static_cast<std::size_t>((target_edges + degree - 1) / degree);
  velographx::storage_detail::SegmentedCsr out;
  const auto b0 = Clock::now();
  out.build(vertices, std::move(edges));
  const auto b1 = Clock::now();
  trim_allocator();
  const auto forward_rss = rss_kb();
  const auto forward_bytes = out.storage_bytes();

  velographx::storage_detail::SegmentedCsr in;
  const auto r0 = Clock::now();
  in.build_transpose_from(out);
  const auto r1 = Clock::now();
  trim_allocator();
  const auto both_rss = rss_kb();
  const auto reverse_bytes = in.storage_bytes();
  const auto rss_delta = both_rss >= forward_rss ? both_rss - forward_rss : 0;
  std::cout << "{\"design\":\"reverse-base-overhead\",\"target_edges\":" << target_edges
            << ",\"vertices\":" << vertices << ",\"degree\":" << degree
            << ",\"forward_build_ms\":" << elapsed_ms(b0, b1)
            << ",\"transpose_build_ms\":" << elapsed_ms(r0, r1)
            << ",\"forward_storage_bytes\":" << forward_bytes
            << ",\"reverse_storage_bytes\":" << reverse_bytes
            << ",\"forward_rss_kb\":" << forward_rss
            << ",\"both_rss_kb\":" << both_rss
            << ",\"reverse_rss_delta_kb\":" << rss_delta << "}\n";
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 3 || argc > 6) {
    std::cerr << "usage: velographx_storage_ab_benchmark <current|legacy|reverse> <target-edges> [degree=20] [updates] [probes]\n";
    return 2;
  }
  try {
    const std::string mode = argv[1];
    const auto target_edges = std::stoull(argv[2]);
    const auto degree = argc >= 4 ? static_cast<std::uint32_t>(std::stoul(argv[3])) : 20U;
    const auto updates = argc >= 5 ? static_cast<std::size_t>(std::stoull(argv[4]))
                                   : static_cast<std::size_t>(std::max<std::uint64_t>(1000, target_edges / 1000));
    const auto probes = argc >= 6 ? static_cast<std::size_t>(std::stoull(argv[5])) : 8192U;
    if (mode == "current") run_current(target_edges, degree, updates, probes);
    else if (mode == "legacy") run_legacy(target_edges, degree, updates, probes);
    else if (mode == "reverse") run_reverse(target_edges, degree);
    else throw std::invalid_argument("unknown mode");
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << '\n';
    return 2;
  }
  return 0;
}
