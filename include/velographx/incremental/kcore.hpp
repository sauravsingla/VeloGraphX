#pragma once
#include <algorithm>
#include <queue>
#include <vector>
#include "velographx/storage/dynamic_graph.hpp"
namespace velographx {
class IncrementalKCore {
 public:
  explicit IncrementalKCore(DynamicGraph& g):g_(g){recompute();}
  [[nodiscard]] const std::vector<std::uint32_t>& core() const noexcept{return core_;}
  void apply(const UpdateBatch& b){ g_.apply(b); recompute(); }
  void recompute(){
    const auto n=g_.vertex_count(); core_.assign(n,0); std::vector<std::uint32_t> deg(n); std::uint32_t maxd=0;
    for(VertexId u=0;u<n;++u){deg[u]=g_.neighbors(u).size();maxd=std::max(maxd,deg[u]);}
    std::vector<std::vector<VertexId>> bins(maxd+1); for(VertexId u=0;u<n;++u) bins[deg[u]].push_back(u);
    std::vector<char> removed(n,0); for(std::uint32_t k=0;k<=maxd;++k){ std::queue<VertexId> q; for(auto u:bins[k]) if(!removed[u]&&deg[u]<=k) q.push(u); while(!q.empty()){auto u=q.front();q.pop();if(removed[u])continue;removed[u]=1;core_[u]=k;for(auto v:g_.neighbors(u))if(!removed[v]&&deg[v]>k){--deg[v];if(deg[v]<=k)q.push(v);}} }
  }
 private: DynamicGraph& g_; std::vector<std::uint32_t> core_;
};
}
