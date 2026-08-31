#pragma once

#include <cstdint>
#include <functional>
#include <limits>
#include <queue>
#include <utility>
#include <vector>

#include "velographx/types.hpp"

namespace velographx::incremental_detail {

inline constexpr std::uint64_t kDijkstraInf = std::numeric_limits<std::uint64_t>::max() / 4;

template <class NeighborEnumerator>
void propagate_dijkstra(std::vector<std::uint64_t>& dist,
                        std::priority_queue<std::pair<std::uint64_t, VertexId>,
                                            std::vector<std::pair<std::uint64_t, VertexId>>,
                                            std::greater<std::pair<std::uint64_t, VertexId>>>& queue,
                        NeighborEnumerator&& enumerate) {
  while (!queue.empty()) {
    const auto [distance, u] = queue.top();
    queue.pop();
    if (u >= dist.size() || distance != dist[u]) continue;
    enumerate(u, [&](VertexId v, std::uint64_t weight) {
      if (v >= dist.size() || weight > kDijkstraInf - distance) return;
      const auto candidate = distance + weight;
      if (candidate < dist[v]) {
        dist[v] = candidate;
        queue.push({candidate, v});
      }
    });
  }
}

template <class NeighborEnumerator>
void recompute_dijkstra(std::size_t vertex_count,
                        VertexId source,
                        std::vector<std::uint64_t>& dist,
                        NeighborEnumerator&& enumerate) {
  dist.assign(vertex_count, kDijkstraInf);
  if (source >= vertex_count) return;
  using Item = std::pair<std::uint64_t, VertexId>;
  std::priority_queue<Item, std::vector<Item>, std::greater<Item>> queue;
  dist[source] = 0;
  queue.push({0, source});
  propagate_dijkstra(dist, queue, std::forward<NeighborEnumerator>(enumerate));
}

}  // namespace velographx::incremental_detail
