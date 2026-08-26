#include <chrono>
#include <iostream>
#include "velographx/storage/dynamic_graph.hpp"
#include "velographx/incremental/triangles.hpp"

int main() {
  using clock = std::chrono::steady_clock;
  velographx::DynamicGraph g(10000, false);
  velographx::UpdateBatch seed;
  for (std::uint32_t i = 1; i < 10000; ++i) seed.add(i-1,i);
  g.apply(seed); g.compact();
  velographx::IncrementalTriangleCount tc(g);
  velographx::UpdateBatch update;
  for (std::uint32_t i = 0; i < 100; ++i) update.add(i, (i+2)%10000);
  const auto t0 = clock::now(); tc.apply(update); const auto t1 = clock::now();
  const auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count();
  std::cout << "{\"algorithm\":\"triangle_count\",\"mode\":\"incremental\",\"vertices\":10000,\"changed_edges\":100,\"elapsed_us\":" << us << ",\"triangles\":" << tc.value() << "}\n";
}
