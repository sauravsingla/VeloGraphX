#pragma once
#include <cstdint>
#include <stdexcept>
#include <vector>
namespace velographx::storage {
using VertexId=std::uint32_t;
inline std::vector<std::uint32_t> delta_encode(const std::vector<VertexId>& ids){std::vector<std::uint32_t> out;out.reserve(ids.size());VertexId prev=0;for(std::size_t i=0;i<ids.size();++i){if(i&&ids[i]<ids[i-1])throw std::invalid_argument("adjacency must be sorted");out.push_back(i?ids[i]-prev:ids[i]);prev=ids[i];}return out;}
inline std::vector<VertexId> delta_decode(const std::vector<std::uint32_t>& d){std::vector<VertexId> out;out.reserve(d.size());VertexId x=0;for(std::size_t i=0;i<d.size();++i){x=i?x+d[i]:d[i];out.push_back(x);}return out;}
}
