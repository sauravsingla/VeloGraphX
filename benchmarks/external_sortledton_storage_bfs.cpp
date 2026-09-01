#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "data-structure/TransactionManager.h"
#include "data-structure/VersioningBlockedSkipListAdjacencyList.h"
#include "data-structure/VersionedBlockedPropertyEdgeIterator.h"
#include "data-structure/VersionedBlockedEdgeIterator.h"

#include "velographx/csr_graph.hpp"
#include "velographx/incremental/bfs.hpp"
#include "velographx/storage/dynamic_graph.hpp"

namespace sortledton_adapter {
using velographx::UpdateBatch; using velographx::VertexId;
class Graph {
 public:
  Graph(std::size_t vertices,const std::vector<std::pair<VertexId,VertexId>>& edges):tm_(1),ds_(std::make_unique<VersioningBlockedSkipListAdjacencyList>(256,sizeof(double),tm_)),vertices_(vertices){
    for(VertexId v=0;v<vertices_;++v){SnapshotTransaction tx=tm_.getSnapshotTransaction(ds_.get(),true);tx.insert_vertex(v);tx.execute();tm_.transactionCompleted(tx);} for(const auto& [u,v]:edges) insert_undirected(u,v);
  }
  Graph(const Graph&)=delete; Graph& operator=(const Graph&)=delete;
  [[nodiscard]] bool has_edge(VertexId u,VertexId v) const {auto* self=const_cast<Graph*>(this);SnapshotTransaction tx=self->tm_.getSnapshotTransaction(self->ds_.get(),false);double w=0.0;const bool found=tx.get_weight(edge_t{static_cast<dst_t>(u),static_cast<dst_t>(v)},reinterpret_cast<char*>(&w));self->tm_.transactionCompleted(tx);return found;}
  template<class Fn> void for_each_neighbor(VertexId u,Fn&& fn) const {auto* self=const_cast<Graph*>(this);SnapshotTransaction tx=self->tm_.getSnapshotTransaction(self->ds_.get(),false);if(!tx.has_vertex(u)){self->tm_.transactionCompleted(tx);return;}const auto physical=tx.physical_id(u);vertexID e;double w;SORTLEDTON_ITERATE_WITH_PROPERTIES_NAMED(tx,physical,e,w,end_iteration,{fn(static_cast<VertexId>(tx.logical_id(e)));});self->tm_.transactionCompleted(tx);}
  void apply(const UpdateBatch& batch){for(const auto& op:batch.updates){if(op.add)insert_undirected(op.src,op.dst);else remove_undirected(op.src,op.dst);}if(!batch.empty())++version_;}
  std::size_t vertices_{0}; std::uint64_t version_{0};
 private:
  void insert_undirected(VertexId u,VertexId v){SnapshotTransaction tx=tm_.getSnapshotTransaction(ds_.get(),true);const double weight=1.0;tx.insert_edge(edge_t{static_cast<dst_t>(u),static_cast<dst_t>(v)},const_cast<char*>(reinterpret_cast<const char*>(&weight)),sizeof(weight));if(u!=v)tx.insert_edge(edge_t{static_cast<dst_t>(v),static_cast<dst_t>(u)},const_cast<char*>(reinterpret_cast<const char*>(&weight)),sizeof(weight));tx.execute();tm_.transactionCompleted(tx);}
  void remove_undirected(VertexId u,VertexId v){SnapshotTransaction tx=tm_.getSnapshotTransaction(ds_.get(),true);if(u!=v)tx.delete_edge(edge_t{static_cast<dst_t>(v),static_cast<dst_t>(u)});tx.delete_edge(edge_t{static_cast<dst_t>(u),static_cast<dst_t>(v)});tx.execute();tm_.transactionCompleted(tx);}
  mutable TransactionManager tm_; std::unique_ptr<VersioningBlockedSkipListAdjacencyList> ds_;
};
std::size_t vx_vertex_count(const Graph& g){return g.vertices_;} bool vx_is_directed(const Graph&){return false;} std::uint64_t vx_version(const Graph& g){return g.version_;}
template<class Fn> void vx_for_each_neighbor(const Graph& g,VertexId u,Fn&& fn){g.for_each_neighbor(u,std::forward<Fn>(fn));}
template<class Fn> void vx_for_each_in_neighbor(const Graph& g,VertexId u,Fn&& fn){g.for_each_neighbor(u,std::forward<Fn>(fn));}
bool vx_has_edge(const Graph& g,VertexId u,VertexId v){return g.has_edge(u,v);} void vx_apply_updates(Graph& g,const UpdateBatch& b){g.apply(b);}
}

namespace {
using velographx::BasicIncrementalBFS; using velographx::CsrGraph; using velographx::DynamicGraph; using velographx::UpdateBatch; using velographx::VertexId;
struct InputGraph{std::size_t vertices{0};std::vector<std::pair<VertexId,VertexId>> edges;};
InputGraph make_synthetic(std::size_t n){std::set<std::pair<VertexId,VertexId>> unique;constexpr VertexId offsets[]={1,7,31,127};for(VertexId u=0;u<n;++u)for(auto off:offsets){VertexId v=static_cast<VertexId>((static_cast<std::size_t>(u)+off)%n);if(u==v)continue;auto e=std::minmax(u,v);unique.insert({e.first,e.second});}return{n,{unique.begin(),unique.end()}};}
InputGraph load_undirected_edge_list(const std::string& path){std::ifstream in(path);if(!in)throw std::runtime_error("cannot open edge list: "+path);std::set<std::pair<VertexId,VertexId>> unique;std::size_t n=0;std::string line;while(std::getline(in,line)){if(line.empty()||line[0]=='#'||line[0]=='%')continue;std::istringstream iss(line);std::uint64_t a=0,b=0;if(!(iss>>a>>b)||a==b)continue;auto u=static_cast<VertexId>(a),v=static_cast<VertexId>(b);auto e=std::minmax(u,v);unique.insert({e.first,e.second});n=std::max(n,static_cast<std::size_t>(std::max(u,v))+1);}return{n,{unique.begin(),unique.end()}};}
template<class G> double median_recompute_us(G& g,VertexId s,std::vector<std::uint32_t>& d){BasicIncrementalBFS<G> bfs(g,s);std::vector<double> samples;for(int r=0;r<5;++r){auto a=std::chrono::steady_clock::now();bfs.recompute();auto b=std::chrono::steady_clock::now();samples.push_back(std::chrono::duration<double,std::micro>(b-a).count());}std::sort(samples.begin(),samples.end());d=bfs.distances();return samples[samples.size()/2];}
UpdateBatch make_update_batch(std::size_t n,const std::vector<std::pair<VertexId,VertexId>>& edges,std::size_t count){UpdateBatch batch;std::set<std::pair<VertexId,VertexId>> present(edges.begin(),edges.end());const auto removals=std::min(count/2,edges.size());for(std::size_t i=0;i<removals;++i)batch.remove(edges[i].first,edges[i].second);std::size_t added=0;for(VertexId u=0;u<n&&added<count-removals;++u){VertexId v=static_cast<VertexId>((static_cast<std::size_t>(u)*97+53)%n);if(u==v)continue;auto e=std::minmax(u,v);if(!present.contains({e.first,e.second})){batch.add(e.first,e.second);present.insert({e.first,e.second});++added;}}return batch;}
template<class G> double apply_batch_us(G& g,VertexId s,const UpdateBatch& batch,std::vector<std::uint32_t>& d){BasicIncrementalBFS<G> bfs(g,s);auto a=std::chrono::steady_clock::now();bfs.apply(batch);auto b=std::chrono::steady_clock::now();d=bfs.distances();return std::chrono::duration<double,std::micro>(b-a).count();}
std::uint64_t digest(const std::vector<std::uint32_t>& d){std::uint64_t h=1469598103934665603ULL;for(auto x:d){h^=x;h*=1099511628211ULL;}return h;}
}
int main(int argc,char** argv){InputGraph input;std::string dataset="synthetic";if(argc>2&&std::string(argv[1])=="--edge-list"){dataset=argv[2];input=load_undirected_edge_list(argv[2]);}else{const std::size_t n=argc>1?static_cast<std::size_t>(std::stoull(argv[1])):8192;input=make_synthetic(n);}if(input.vertices==0)return 3;const VertexId source=0;DynamicGraph dynamic(input.vertices,false);dynamic.bulk_load_edges(input.edges);CsrGraph csr(input.edges,false);sortledton_adapter::Graph sortledton(input.vertices,input.edges);std::vector<std::uint32_t> dd,cd,sd;const double du=median_recompute_us(dynamic,source,dd),cu=median_recompute_us(csr,source,cd),su=median_recompute_us(sortledton,source,sd);const bool rexact=dd==cd&&dd==sd;DynamicGraph dm(input.vertices,false);dm.bulk_load_edges(input.edges);sortledton_adapter::Graph sm(input.vertices,input.edges);const auto batch=make_update_batch(input.vertices,input.edges,64);std::vector<std::uint32_t> da,sa;const double dmu=apply_batch_us(dm,source,batch,da),smu=apply_batch_us(sm,source,batch,sa);const bool uexact=da==sa,exact=rexact&&uexact;std::cout<<std::fixed<<std::setprecision(3)<<"{\"schema_version\":2,\"benchmark\":\"same-algorithm-storage-bfs\",\"algorithm\":\"BasicIncrementalBFS\",\"dataset\":\""<<dataset<<"\",\"vertices\":"<<input.vertices<<",\"edges\":"<<input.edges.size()<<",\"source\":"<<source<<",\"repetitions\":5,\"update_count\":"<<batch.updates.size()<<",\"recompute_exact\":"<<(rexact?"true":"false")<<",\"update_exact\":"<<(uexact?"true":"false")<<",\"exact\":"<<(exact?"true":"false")<<",\"digest\":"<<digest(dd)<<",\"updated_digest\":"<<digest(da)<<",\"dynamic_graph_median_us\":"<<du<<",\"csr_graph_median_us\":"<<cu<<",\"sortledton_median_us\":"<<su<<",\"dynamic_update_us\":"<<dmu<<",\"sortledton_update_us\":"<<smu<<"}\n";return exact?0:2;}
