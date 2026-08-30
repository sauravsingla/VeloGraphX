#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <queue>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "velographx/incremental/bfs.hpp"
#include "velographx/storage/dynamic_graph.hpp"

namespace {
using Edge=std::pair<velographx::VertexId,velographx::VertexId>; using Clock=std::chrono::steady_clock;
constexpr double kAffectedBudget=.05, kPreflight=.05, kSparseReach=.20, kSparseReachFullUpdate=.0025, kEma=.25;
struct PolicyResult{std::string name;std::vector<double> batch_us,decision_us;std::size_t full_recompute_batches{0},affected_vertices{0},oracle_matches{0};bool exact{true};};
std::vector<Edge> read_edges(const std::string&p,std::size_t&n){std::ifstream in(p);if(!in)throw std::runtime_error("cannot open edge list");std::vector<Edge>e;std::string l;std::uint64_t m=0;bool s=false;while(std::getline(in,l)){if(l.empty()||l[0]=='#')continue;std::istringstream r(l);std::uint64_t u,v;if(!(r>>u>>v))continue;e.emplace_back((velographx::VertexId)u,(velographx::VertexId)v);m=std::max(m,std::max(u,v));s=true;}n=s?(std::size_t)m+1:0;return e;}
std::vector<std::uint32_t> full_bfs(const velographx::DynamicGraph&g,velographx::VertexId s){auto inf=std::numeric_limits<std::uint32_t>::max();std::vector<std::uint32_t>d(g.vertex_count(),inf);if(s>=g.vertex_count())return d;std::queue<velographx::VertexId>q;d[s]=0;q.push(s);while(!q.empty()){auto u=q.front();q.pop();g.for_each_neighbor(u,[&](auto v){if(d[v]==inf){d[v]=d[u]+1;q.push(v);}});}return d;}
std::size_t reachable(const std::vector<std::uint32_t>&d){auto inf=std::numeric_limits<std::uint32_t>::max();return std::count_if(d.begin(),d.end(),[&](auto x){return x!=inf;});}
velographx::UpdateBatch batch(const std::vector<Edge>&e,std::size_t imp,std::size_t b,std::size_t z){velographx::UpdateBatch u;u.updates.reserve((z-b)*2);for(auto i=b;i<z;++i)u.add(e[i].first,e[i].second);for(auto i=b;i<z;++i){auto j=i-imp;u.remove(e[j].first,e[j].second);}return u;}
PolicyResult run(const std::string&p,const std::vector<Edge>&e,std::size_t n,velographx::VertexId root,std::size_t imp,std::size_t bs,double simple){std::vector<Edge>init(e.begin(),e.begin()+imp);velographx::DynamicGraph g(n,true);g.bulk_load_edges(init);velographx::IncrementalBFS bfs(g,root,p=="always_incremental"?2.0:kAffectedBudget);PolicyResult R;R.name=p;double prev_aff=0,ema_inc=0,ema_full=0;bool have_inc=false,have_full=false;
 for(std::size_t b=imp;b<e.size();b+=bs){auto z=std::min(b+bs,e.size());auto u=batch(e,imp,b,z);auto t0=Clock::now();double uf=(double)u.updates.size()/std::max<std::size_t>(1,g.edge_count_directed());double rf=(double)reachable(bfs.distances())/std::max<std::size_t>(1,n);bool choose_full=false;
  auto d0=Clock::now();if(p=="simple_threshold")choose_full=uf>=simple;else if(p=="adaptive"){double predicted_inc=have_inc?ema_inc*(1.0+2.0*prev_aff):0;double predicted_full=have_full?ema_full:0;bool learned=have_inc&&have_full&&predicted_full>0;choose_full=(rf<kSparseReach&&uf>=kSparseReachFullUpdate)||(uf>=kPreflight)||(learned&&predicted_inc>predicted_full*.95);}auto d1=Clock::now();R.decision_us.push_back(std::chrono::duration<double,std::micro>(d1-d0).count());
  auto x0=Clock::now();if(p=="always_full"||choose_full){g.apply(u);bfs.recompute();++R.full_recompute_batches;}else{bfs.apply(u);R.affected_vertices+=bfs.last_affected_vertices();R.full_recompute_batches+=bfs.last_used_full_recompute()?1:0;}auto x1=Clock::now();double exec=std::chrono::duration<double,std::micro>(x1-x0).count();R.batch_us.push_back(std::chrono::duration<double,std::micro>(x1-t0).count());
  if(p=="adaptive"){if(choose_full){ema_full=have_full?(1-kEma)*ema_full+kEma*exec:exec;have_full=true;}else{ema_inc=have_inc?(1-kEma)*ema_inc+kEma*exec:exec;have_inc=true;}prev_aff=(double)bfs.last_affected_vertices()/std::max<std::size_t>(1,n);}auto ref=full_bfs(g,root);if(ref!=bfs.distances())R.exact=false;
 }return R;}
void arr(const std::vector<double>&v){std::cout<<'[';for(size_t i=0;i<v.size();++i){if(i)std::cout<<',';std::cout<<v[i];}std::cout<<']';}
}
int main(int ac,char**av){if(ac!=6)return 2;std::string path=av[1];auto root=(velographx::VertexId)std::stoull(av[2]);double rate=std::stod(av[3]);auto bs=(std::size_t)std::stoull(av[4]);double simple=std::stod(av[5]);std::size_t n=0;auto e=read_edges(path,n);auto imp=(std::size_t)(e.size()*rate);std::vector<std::string>names={"always_incremental","always_full","simple_threshold","adaptive"};std::vector<PolicyResult>rs;for(auto&p:names)rs.push_back(run(p,e,n,root,imp,bs,simple));auto nb=rs[0].batch_us.size();std::vector<double>oracle(nb,std::numeric_limits<double>::infinity());for(auto&r:rs)for(size_t i=0;i<nb;++i)oracle[i]=std::min(oracle[i],r.batch_us[i]);bool exact=true;std::cout<<"{\"schema_version\":4,\"selector\":\"root-state-online-v1\",\"root\":"<<root<<",\"vertices\":"<<n<<",\"batch_size\":"<<bs<<",\"batches\":"<<nb<<",\"policies\":[";for(size_t j=0;j<rs.size();++j){auto&r=rs[j];if(j)std::cout<<',';double total=0,du=0;for(auto x:r.batch_us)total+=x;for(auto x:r.decision_us)du+=x;exact&=r.exact;std::cout<<"{\"name\":\""<<r.name<<"\",\"exact\":"<<(r.exact?"true":"false")<<",\"total_us\":"<<total<<",\"mean_batch_us\":"<<total/std::max<size_t>(1,nb)<<",\"mean_decision_us\":"<<du/std::max<size_t>(1,nb)<<",\"full_recompute_batches\":"<<r.full_recompute_batches<<",\"affected_vertices\":"<<r.affected_vertices<<",\"batch_us\":";arr(r.batch_us);std::cout<<'}';}std::cout<<"],\"oracle_batch_us\":";arr(oracle);std::cout<<",\"all_policies_exact\":"<<(exact?"true":"false")<<"}\n";return exact?0:1;}
