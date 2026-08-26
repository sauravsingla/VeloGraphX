#pragma once
#include <cstdint>
#include <vector>

namespace velographx {
using VertexId = std::uint32_t;
class Frontier {
 public:
  explicit Frontier(std::size_t n = 0) : bitmap_(n, 0) {}
  void reset(std::size_t n) { sparse_.clear(); bitmap_.assign(n, 0); }
  void add(VertexId v) { if (!bitmap_[v]) { bitmap_[v] = 1; sparse_.push_back(v); } }
  [[nodiscard]] bool contains(VertexId v) const { return bitmap_[v] != 0; }
  [[nodiscard]] const std::vector<VertexId>& sparse() const noexcept { return sparse_; }
  [[nodiscard]] double density() const { return bitmap_.empty() ? 0.0 : static_cast<double>(sparse_.size())/bitmap_.size(); }
  [[nodiscard]] bool prefer_dense() const { return density() > 0.08; }
 private:
  std::vector<VertexId> sparse_;
  std::vector<std::uint8_t> bitmap_;
};
} // namespace velographx
