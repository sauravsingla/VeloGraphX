#pragma once

#include "velographx/runtime/partitioner.hpp"
#include "velographx/runtime/work_stealing_pool.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace velographx {

class NumaLocalScheduler {
 public:
  NumaLocalScheduler(WorkStealingPool& pool, std::vector<NumaVertexPartition> partitions)
      : pool_(pool), partitions_(std::move(partitions)) {
    for (const auto& partition : partitions_) {
      if (!partition.node_id.has_value()) continue;
      if (std::find(node_ids_.begin(), node_ids_.end(), *partition.node_id) == node_ids_.end())
        node_ids_.push_back(*partition.node_id);
    }
    std::sort(node_ids_.begin(), node_ids_.end());
  }

  std::size_t preferred_queue_for_node(std::size_t node_id,
                                       std::size_t sequence = 0) const noexcept {
    if (pool_.size() == 0 || node_ids_.empty()) return sequence % std::max<std::size_t>(1, pool_.size());
    const auto it = std::find(node_ids_.begin(), node_ids_.end(), node_id);
    const std::size_t node_rank = it == node_ids_.end()
        ? node_id % node_ids_.size()
        : static_cast<std::size_t>(it - node_ids_.begin());
    const std::size_t node_count = node_ids_.size();
    const std::size_t local_workers = (pool_.size() + node_count - 1) / node_count;
    return (node_rank + (sequence % local_workers) * node_count) % pool_.size();
  }

  std::size_t preferred_queue_for_vertex(std::size_t vertex,
                                         std::size_t sequence = 0) const noexcept {
    const auto node = numa_node_for_vertex(vertex, partitions_);
    if (!node.has_value()) return sequence % std::max<std::size_t>(1, pool_.size());
    return preferred_queue_for_node(*node, sequence);
  }

  void submit_for_vertex(std::size_t vertex, WorkStealingPool::Task task,
                         std::size_t sequence = 0) {
    pool_.submit(std::move(task), preferred_queue_for_vertex(vertex, sequence));
  }

  template <class Fn>
  void parallel_for_partitions(Fn&& fn) {
    std::size_t sequence = 0;
    for (const auto& partition : partitions_) {
      const auto queue = partition.node_id.has_value()
          ? preferred_queue_for_node(*partition.node_id, sequence++)
          : sequence++ % std::max<std::size_t>(1, pool_.size());
      pool_.submit([partition, &fn] { fn(partition); }, queue);
    }
    pool_.wait_idle();
  }

  const std::vector<NumaVertexPartition>& partitions() const noexcept { return partitions_; }

 private:
  WorkStealingPool& pool_;
  std::vector<NumaVertexPartition> partitions_;
  std::vector<std::size_t> node_ids_;
};

}  // namespace velographx
