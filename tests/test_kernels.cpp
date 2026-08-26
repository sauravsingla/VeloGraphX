#include <cassert>
#include <cstdint>
#include <vector>

#include "velographx/kernels/cpu_features.hpp"
#include "velographx/kernels/intersection.hpp"
#include "velographx/runtime/push_pull.hpp"
#include "velographx/storage/compressed_adjacency.hpp"

int main() {
  using velographx::kernels::IntersectionKernel;
  using velographx::kernels::VertexId;

  std::vector<VertexId> a{1, 3, 5, 7};
  std::vector<VertexId> b{0, 3, 4, 7};
  assert(velographx::kernels::adaptive_intersection(a, b) == 2);

  std::vector<VertexId> large_a;
  std::vector<VertexId> large_b;
  for (VertexId i = 0; i < 512; ++i) {
    large_a.push_back(i * 2);
    large_b.push_back(i * 3);
  }

  const auto expected = velographx::kernels::scalar_intersection(large_a, large_b);
  assert(velographx::kernels::galloping_intersection(large_a, large_b) == expected);
  assert(velographx::kernels::avx2_intersection(large_a, large_b) == expected);
  assert(velographx::kernels::avx512_intersection(large_a, large_b) == expected);
  assert(velographx::kernels::neon_intersection(large_a, large_b) == expected);
  assert(velographx::kernels::adaptive_intersection(large_a, large_b) == expected);

  std::vector<VertexId> dense_a;
  std::vector<VertexId> dense_b;
  for (VertexId i = 0; i < 1024; ++i) {
    dense_a.push_back(i);
    if ((i % 2) == 0) dense_b.push_back(i);
  }
  const auto dense_expected = velographx::kernels::scalar_intersection(dense_a, dense_b);
  assert(velographx::kernels::bitmap_intersection(dense_a, dense_b) == dense_expected);
  assert(velographx::kernels::select_intersection(dense_a, dense_b) ==
         IntersectionKernel::bitmap);
  assert(velographx::kernels::adaptive_intersection(dense_a, dense_b) == dense_expected);

  std::vector<VertexId> disjoint_a;
  std::vector<VertexId> disjoint_b;
  for (VertexId i = 0; i < 128; ++i) {
    disjoint_a.push_back(i * 4);
    disjoint_b.push_back(i * 4 + 1);
  }
  assert(velographx::kernels::adaptive_intersection(disjoint_a, disjoint_b) == 0);

  const auto encoded = velographx::storage::delta_encode(a);
  assert(velographx::storage::delta_decode(encoded) == a);
  assert(velographx::choose_direction(1, 100, 2, 1000) ==
         velographx::TraversalDirection::push);

  const auto features = velographx::kernels::detect_cpu_features();
  (void)features;
}
