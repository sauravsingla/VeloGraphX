#pragma once
#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>
namespace velographx {
inline std::vector<std::pair<std::size_t,std::size_t>> contiguous_partitions(std::size_t n,std::size_t parts){parts=std::max<std::size_t>(1,std::min(parts,std::max<std::size_t>(1,n)));std::vector<std::pair<std::size_t,std::size_t>> out;out.reserve(parts);for(std::size_t p=0;p<parts;++p){auto b=n*p/parts,e=n*(p+1)/parts;out.push_back({b,e});}return out;}
}
