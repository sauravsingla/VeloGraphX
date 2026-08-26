#pragma once
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include "velographx/storage/dynamic_graph.hpp"

namespace velographx::io {
inline void save_binary(const DynamicGraph& g, const std::string& path) {
  std::ofstream out(path, std::ios::binary); if(!out) throw std::runtime_error("cannot open output");
  const std::uint64_t magic=0x564758303031ULL, n=g.vertex_count(), m=g.edge_count_directed();
  out.write(reinterpret_cast<const char*>(&magic),sizeof(magic)); out.write(reinterpret_cast<const char*>(&n),sizeof(n)); out.write(reinterpret_cast<const char*>(&m),sizeof(m));
  for(VertexId u=0;u<g.vertex_count();++u) for(auto v:g.neighbors(u)){ out.write(reinterpret_cast<const char*>(&u),sizeof(u)); out.write(reinterpret_cast<const char*>(&v),sizeof(v)); }
}
inline DynamicGraph load_binary(const std::string& path, bool directed=false) {
  std::ifstream in(path,std::ios::binary); if(!in) throw std::runtime_error("cannot open input");
  std::uint64_t magic=0,n=0,m=0; in.read(reinterpret_cast<char*>(&magic),sizeof(magic)); in.read(reinterpret_cast<char*>(&n),sizeof(n)); in.read(reinterpret_cast<char*>(&m),sizeof(m));
  if(magic!=0x564758303031ULL) throw std::runtime_error("invalid VeloGraphX binary graph");
  DynamicGraph g(n,directed); UpdateBatch b; for(std::uint64_t i=0;i<m;++i){ VertexId u,v; in.read(reinterpret_cast<char*>(&u),sizeof(u)); in.read(reinterpret_cast<char*>(&v),sizeof(v)); if(!in) throw std::runtime_error("truncated graph file"); if(directed || u<=v) b.add(u,v); }
  g.apply(b); g.compact(); return g;
}
} // namespace velographx::io
