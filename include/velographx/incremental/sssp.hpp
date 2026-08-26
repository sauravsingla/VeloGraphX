#pragma once
#include <cstdint>
#include <limits>
#include <queue>
#include <utility>
#include <vector>
#include "velographx/storage/dynamic_graph.hpp"
namespace velographx {
class IncrementalSSSP {
 public:
  IncrementalSSSP(DynamicGraph& g, VertexId source):g_(g),source_(source){recompute();}
  [[nodiscard]] const std::vector<std::uint64_t>& distances() const noexcept{return dist_;}
  void apply(const UpdateBatch& b){ bool deletion=false;for(const auto&e:b.updates)deletion|=!e.add;g_.apply(b); if(deletion)recompute(); else relax_from_updates(b); }
  void recompute(){const auto inf=std::numeric_limits<std::uint64_t>::max()/4;dist_.assign(g_.vertex_count(),inf);if(source_>=g_.vertex_count())return;using P=std::pair<std::uint64_t,VertexId>;std::priority_queue<P,std::vector<P>,std::greater<P>>q;dist_[source_]=0;q.push({0,source_});while(!q.empty()){auto[d,u]=q.top();q.pop();if(d!=dist_[u])continue;for(auto v:g_.neighbors(u))if(d+1<dist_[v]){dist_[v]=d+1;q.push({dist_[v],v});}}}
 private:
  void relax_from_updates(const UpdateBatch& b){if(dist_.size()<g_.vertex_count())dist_.resize(g_.vertex_count(),std::numeric_limits<std::uint64_t>::max()/4);using P=std::pair<std::uint64_t,VertexId>;std::priority_queue<P,std::vector<P>,std::greater<P>>q;for(const auto&e:b.updates)if(e.add&&dist_[e.src]+1<dist_[e.dst]){dist_[e.dst]=dist_[e.src]+1;q.push({dist_[e.dst],e.dst});}while(!q.empty()){auto[d,u]=q.top();q.pop();if(d!=dist_[u])continue;for(auto v:g_.neighbors(u))if(d+1<dist_[v]){dist_[v]=d+1;q.push({dist_[v],v});}}}
  DynamicGraph&g_;VertexId source_;std::vector<std::uint64_t>dist_;
};
}
