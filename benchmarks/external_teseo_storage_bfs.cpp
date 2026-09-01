#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "teseo.hpp"
#include "velographx/csr_graph.hpp"
#include "velographx/incremental/bfs.hpp"
#include "velographx/storage/dynamic_graph.hpp"

namespace teseo_adapter {

using velographx::UpdateBatch;
using velographx::VertexId;

class Graph {
 public:
  Graph(std::size_t vertices, const std::vector<std::pair<VertexId, VertexId>>& edges)
      : vertices_(vertices) {
    auto tx = database_.start_transaction();
    for (std::size_t v = 0; v < vertices_; ++v) tx.insert_vertex(v);
    for (const auto& [u, v] : edges) {
      tx.insert_edge(u, v, 1.0);
      if (u != v) tx.insert_edge(v, u, 1.0);
    }
    tx.commit();
  }

  Graph(const Graph&) = delete;
  Graph& operator=(const Graph&) = delete;
  ~Graph() { close_snapshot(); }

  void close_snapshot() const {
    if (iterator_) { iterator_->close(); iterator_.reset(); }
    if (snapshot_) { snapshot_->commit(); snapshot_.reset(); }
  }

  void ensure_snapshot() const {
    if (snapshot_) return;
    snapshot_ = std::make_unique<teseo::Transaction>(database_.start_transaction(true));
    iterator_ = std::make_unique<teseo::Iterator>(snapshot_->iterator());
  }

  void apply(const UpdateBatch& batch) {
    close_snapshot();
    auto tx = database_.start_transaction();
    for (const auto& op : batch.updates) {
      if (op.add) {
        tx.insert_edge(op.src, op.dst, 1.0);
        if (op.src != op.dst) tx.insert_edge(op.dst, op.src, 1.0);
      } else {
        tx.remove_edge(op.src, op.dst);
        if (op.src != op.dst) tx.remove_edge(op.dst, op.src);
      }
    }
    tx.commit();
    if (!batch.empty()) ++version_;
  }

  mutable teseo::Teseo database_;
  std::size_t vertices_{0};
  std::uint64_t version_{0};
  mutable std::unique_ptr<teseo::Transaction> snapshot_;
  mutable std::unique_ptr<teseo::Iterator> iterator_;
};

std::size_t vx_vertex_count(const Graph& graph) { return graph.vertices_; }
bool vx_is_directed(const Graph&) { return false; }
std::uint64_t vx_version(const Graph& graph) { return graph.version_; }

template <class Fn>
void vx_for_each_neighbor(const Graph& graph, VertexId u, Fn&& fn) {
  graph.ensure_snapshot();
  graph.iterator_->edges(static_cast<std::uint64_t>(u), false, [&](std::uint64_t destination) {
    fn(static_cast<VertexId>(destination));
  });
}

template <class Fn>
void vx_for_each_in_neighbor(const Graph& graph, VertexId u, Fn&& fn) {
  vx_for_each_neighbor(graph, u, std::forward<Fn>(fn));
}

bool vx_has_edge(const Graph& graph, VertexId u, VertexId v) {
  graph.ensure_snapshot();
  return graph.snapshot_->has_edge(u, v);
}

void vx_apply_updates(Graph& graph, const UpdateBatch& batch) { graph.apply(batch); }

}  // namespace teseo_adapter

namespace {
using velographx::BasicIncrementalBFS;
using velographx::CsrGraph;
using velographx::DynamicGraph;
using velographx::UpdateBatch;
using velographx::VertexId;

struct InputGraph { std::size_t vertices{0}; std::vector<std::pair<VertexId, VertexId>> edges; };

InputGraph make_synthetic(std::size_t vertices) {
  std::set<std::pair<VertexId, VertexId>> unique;
  constexpr VertexId offsets[] = {1, 7, 31, 127};
  for (VertexId u = 0; u < vertices; ++u) for (auto offset : offsets) {
    VertexId v = static_cast<VertexId>((static_cast<std::size_t>(u) + offset) % vertices);
    if (u == v) continue;
    auto e = std::minmax(u, v); unique.insert({e.first, e.second});
  }
  return {vertices, {unique.begin(), unique.end()}};
}

InputGraph load_undirected_edge_list(const std::string& path) {
  std::ifstream in(path); if (!in) throw std::runtime_error("cannot open edge list: " + path);
  std::set<std::pair<VertexId, VertexId>> unique; std::size_t vertices = 0; std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#' || line[0] == '%') continue;
    std::istringstream iss(line); std::uint64_t a=0,b=0; if (!(iss>>a>>b) || a==b) continue;
    auto u=static_cast<VertexId>(a), v=static_cast<VertexId>(b); auto e=std::minmax(u,v);
    unique.insert({e.first,e.second}); vertices=std::max(vertices,static_cast<std::size_t>(std::max(u,v))+1);
  }
  return {vertices,{unique.begin(),unique.end()}};
}

template<class Graph>
double median_recompute_us(Graph& graph, VertexId source, std::vector<std::uint32_t>& distances) {
  BasicIncrementalBFS<Graph> bfs(graph, source); std::vector<double> samples;
  for(int rep=0;rep<5;++rep){ auto a=std::chrono::steady_clock::now(); bfs.recompute(); auto b=std::chrono::steady_clock::now(); samples.push_back(std::chrono::duration<double,std::micro>(b-a).count()); }
  std::sort(samples.begin(),samples.end()); distances=bfs.distances(); return samples[samples.size()/2];
}

UpdateBatch make_update_batch(std::size_t vertices,const std::vector<std::pair<VertexId,VertexId>>& edges,std::size_t count){
  UpdateBatch batch; std::set<std::pair<VertexId,VertexId>> present(edges.begin(),edges.end());
  const std::size_t removals=std::min(count/2,edges.size()); for(std::size_t i=0;i<removals;++i) batch.remove(edges[i].first,edges[i].second);
  std::size_t added=0; for(VertexId u=0;u<vertices && added<count-removals;++u){ VertexId v=static_cast<VertexId>((static_cast<std::size_t>(u)*97+53)%vertices); if(u==v) continue; auto e=std::minmax(u,v); if(!present.contains({e.first,e.second})){ batch.add(e.first,e.second); present.insert({e.first,e.second}); ++added; }} return batch;
}

template<class Graph>
double apply_batch_us(Graph& graph,VertexId source,const UpdateBatch& batch,std::vector<std::uint32_t>& distances){ BasicIncrementalBFS<Graph> bfs(graph,source); auto a=std::chrono::steady_clock::now(); bfs.apply(batch); auto b=std::chrono::steady_clock::now(); distances=bfs.distances(); return std::chrono::duration<double,std::micro>(b-a).count(); }

std::uint64_t digest(const std::vector<std::uint32_t>& d){ std::uint64_t h=1469598103934665603ULL; for(auto x:d){h^=static_cast<std::uint64_t>(x);h*=1099511628211ULL;} return h; }
}

int main(int argc,char** argv){
  InputGraph input; std::string dataset="synthetic";
  if(argc>2 && std::string(argv[1])=="--edge-list"){dataset=argv[2];input=load_undirected_edge_list(argv[2]);} else {const std::size_t n=argc>1?static_cast<std::size_t>(std::stoull(argv[1])):8192;input=make_synthetic(n);} if(input.vertices==0) return 3; const VertexId source=0;
  DynamicGraph dynamic(input.vertices,false); dynamic.bulk_load_edges(input.edges); CsrGraph csr(input.edges,false); teseo_adapter::Graph teseo(input.vertices,input.edges);
  std::vector<std::uint32_t> dd,cd,td; const double du=median_recompute_us(dynamic,source,dd), cu=median_recompute_us(csr,source,cd), tu=median_recompute_us(teseo,source,td); const bool rexact=dd==cd && dd==td;
  DynamicGraph dm(input.vertices,false); dm.bulk_load_edges(input.edges); teseo_adapter::Graph tm(input.vertices,input.edges); const auto batch=make_update_batch(input.vertices,input.edges,64); std::vector<std::uint32_t> da,ta; const double dmu=apply_batch_us(dm,source,batch,da), tmu=apply_batch_us(tm,source,batch,ta); const bool uexact=da==ta; const bool exact=rexact&&uexact;
  std::cout<<std::fixed<<std::setprecision(3)<<"{\"schema_version\":2,\"benchmark\":\"same-algorithm-storage-bfs\",\"algorithm\":\"BasicIncrementalBFS\",\"dataset\":\""<<dataset<<"\",\"vertices\":"<<input.vertices<<",\"edges\":"<<input.edges.size()<<",\"source\":"<<source<<",\"repetitions\":5,\"update_count\":"<<batch.updates.size()<<",\"recompute_exact\":"<<(rexact?"true":"false")<<",\"update_exact\":"<<(uexact?"true":"false")<<",\"exact\":"<<(exact?"true":"false")<<",\"digest\":"<<digest(dd)<<",\"updated_digest\":"<<digest(da)<<",\"dynamic_graph_median_us\":"<<du<<",\"csr_graph_median_us\":"<<cu<<",\"teseo_median_us\":"<<tu<<",\"dynamic_update_us\":"<<dmu<<",\"teseo_update_us\":"<<tmu<<"}\n";
  return exact?0:2;
}
