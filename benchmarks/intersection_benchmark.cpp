#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <string_view>
#include <vector>

#include "velographx/kernels/intersection.hpp"
#include "velographx/kernels/cpu_features.hpp"

using Clock = std::chrono::steady_clock;
using VertexId = velographx::kernels::VertexId;

namespace {
std::vector<VertexId> make_sorted_unique(std::size_t n, std::uint32_t seed, std::uint32_t max_value) {
  std::mt19937 rng(seed);
  std::uniform_int_distribution<std::uint32_t> dist(0, max_value);
  std::vector<VertexId> out;
  out.reserve(n);
  while (out.size() < n) out.push_back(dist(rng));
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
  while (out.size() < n) {
    out.push_back(out.empty() ? 0u : static_cast<VertexId>(out.back() + 1u));
  }
  return out;
}

template <class Fn>
double bench(Fn&& fn, std::span<const VertexId> a, std::span<const VertexId> b, std::size_t reps, std::size_t& checksum) {
  const auto start = Clock::now();
  std::size_t local = 0;
  for (std::size_t i = 0; i < reps; ++i) local += fn(a, b);
  const auto stop = Clock::now();
  checksum ^= local;
  return std::chrono::duration<double, std::micro>(stop - start).count() / static_cast<double>(reps);
}

void emit(std::string_view name, std::size_t a_size, std::size_t b_size, double us, std::size_t count) {
  std::cout << "{\"benchmark\":\"intersection\",\"kernel\":\"" << name
            << "\",\"a_size\":" << a_size << ",\"b_size\":" << b_size
            << ",\"mean_us\":" << us << ",\"intersection_count\":" << count << "}\n";
}
}  // namespace

int main() {
  constexpr std::size_t reps = 1000;
  const std::vector<std::pair<std::size_t, std::size_t>> cases{
      {8, 8}, {32, 32}, {128, 128}, {32, 1024}, {128, 4096}, {1024, 1024}};
  std::size_t checksum = 0;

  std::cout << "{\"cpu_best_isa\":\"" << velographx::kernels::best_isa() << "\"}\n";
  for (const auto [as, bs] : cases) {
    auto a = make_sorted_unique(as, static_cast<std::uint32_t>(as + 17), 1u << 20);
    auto b = make_sorted_unique(bs, static_cast<std::uint32_t>(bs + 29), 1u << 20);
    const auto reference = velographx::kernels::scalar_intersection(a, b);

    emit("scalar", as, bs,
         bench(velographx::kernels::scalar_intersection, a, b, reps, checksum), reference);
    emit("galloping", as, bs,
         bench([&](auto x, auto y) {
           return x.size() <= y.size() ? velographx::kernels::galloping_intersection(x, y)
                                       : velographx::kernels::galloping_intersection(y, x);
         }, a, b, reps, checksum), reference);
    emit("adaptive", as, bs,
         bench(velographx::kernels::adaptive_intersection, a, b, reps, checksum), reference);
  }

  if (checksum == static_cast<std::size_t>(-1)) std::cerr << checksum << '\n';
  return 0;
}
