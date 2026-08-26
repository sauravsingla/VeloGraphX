#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>
#include <queue>
#include <vector>
#include "velographx/storage/dynamic_graph.hpp"

namespace velographx {
class IncrementalBFS {
 public:
  static constexpr std::uint32_t unreachable = std::numeric_limits<std::uint32_t>::max();
  IncrementalBFS(DynamicGraph& g, VertexId source) : g_(g), source_(source) { recompute(); }
  [[nodiscard]] const std::vector<std::uint32_t>& distances() const noexcept { return dist_; }
  void apply(const UpdateBatch& batch) {
    bool deletion=false; for(const auto&e:batch.updates) deletion|=!e.add;
    g_.apply(batch);
    if(deletion){ recompute(); return; }
    if(dist_.size()<g_.vertex_count()) dist_.resize(g_.vertex_count(),unreachable);
    std::queue<VertexId> q;
    for(const auto&e:batch.updates){
      if(dist_[e.src]!=unreachable && dist_[e.src]+1<dist_[e.dst]){ dist_[e.dst]=dist_[e.src]+1; q.push(e.dst); }
      if(!g_.directed() && dist_[e.dst]!=unreachable && dist_[e.dst]+1<dist_[e.src]){ dist_[e.src]=dist_[e.dst]+1; q.push(e.src); }
    }
    while(!q.empty()){ auto u=q.front(); q.pop(); for(auto v:g_.neighbors(u)) if(dist_[u]+1<dist_[v]){dist_[v]=dist_[u]+1;q.push(v);} }
  }
  void recompute(){ dist_.assign(g_.vertex_count(),unreachable); if(source_>=g_.vertex_count()) return; std::queue<VertexId>q; dist_[source_]=0;q.push(source_);while(!q.empty()){auto u=q.front();q.pop();for(auto v:g_.neighbors(u))if(dist_[v]==unreachable){dist_[v]=dist_[u]+1;q.push(v);}} }
 private: DynamicGraph& g_; VertexId source_; std::vector<std::uint32_t> dist_;
};
} // namespace velographx
