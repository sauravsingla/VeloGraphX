#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <thread>

#include "velographx/incremental/triangles.hpp"
#include "velographx/storage/dynamic_graph.hpp"

int main() {
  using clock = std::chrono::steady_clock;
  constexpr std::uint32_t vertices = 20000;
  constexpr std::array<double, 7> fractions = {
      0.000001, 0.00001, 0.0001, 0.001, 0.01, 0.05, 0.10};

  std::cout << "algorithm,vertices,base_edges,update_fraction,changed_edges,incremental_us,full_recompute_us,speedup,triangles,threads\n";

  for (double fraction : fractions) {
    velographx::DynamicGraph g(vertices, false);
    velographx::UpdateBatch seed;
    for (std::uint32_t i = 0; i < vertices; ++i) {
      seed.add(i, (i + 1) % vertices);
      seed.add(i, (i + 7) % vertices);
      seed.add(i, (i + 31) % vertices);
    }
    g.apply(seed);
    g.compact();

    const std::size_t base_edges = seed.updates.size();
    const std::size_t changed_edges = std::max<std::size_t>(
        1, static_cast<std::size_t>(std::ceil(base_edges * fraction)));

    velographx::IncrementalTriangleCount tc(g);
    velographx::UpdateBatch update;
    for (std::size_t i = 0; i < changed_edges; ++i) {
      const auto src = static_cast<std::uint32_t>((i * 97 + 13) % vertices);
      const auto dst = static_cast<std::uint32_t>((src + 53 + (i % 101)) % vertices);
      if (src != dst && !g.has_edge(src, dst)) update.add(src, dst);
    }

    const auto t0 = clock::now();
    tc.apply(update);
    const auto t1 = clock::now();
    const auto incremental_us =
        std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

    const auto t2 = clock::now();
    tc.recompute();
    const auto t3 = clock::now();
    const auto full_us =
        std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count();

    const double speedup = incremental_us > 0
        ? static_cast<double>(full_us) / static_cast<double>(incremental_us)
        : 0.0;

    std::cout << "triangle_count," << vertices << ',' << base_edges << ','
              << fraction << ',' << update.updates.size() << ','
              << incremental_us << ',' << full_us << ',' << speedup << ','
              << tc.value() << ',' << std::thread::hardware_concurrency() << '\n';
  }
}
