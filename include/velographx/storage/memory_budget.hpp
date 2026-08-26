#pragma once
#include <algorithm>
#include <cstddef>
#include <string>
namespace velographx {
class MemoryBudget {
 public:
  explicit MemoryBudget(std::size_t bytes):bytes_(bytes){}
  [[nodiscard]] std::size_t bytes()const noexcept{return bytes_;}
  [[nodiscard]] std::size_t resident_limit(std::size_t graph_bytes,std::size_t algorithm_state_bytes)const noexcept{return bytes_>algorithm_state_bytes?std::min(graph_bytes,bytes_-algorithm_state_bytes):0;}
 private:std::size_t bytes_;
};
inline std::size_t parse_memory_budget_gib(std::size_t gib){return gib*1024ULL*1024ULL*1024ULL;}
}
