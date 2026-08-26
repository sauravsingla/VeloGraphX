#pragma once
#include <algorithm>
#include <cstdint>
#include <span>
#include "velographx/kernels/cpu_features.hpp"
namespace velographx::kernels {
using VertexId=std::uint32_t;
enum class IntersectionKernel{scalar_merge,galloping,avx2,avx512,neon};
inline std::size_t scalar_intersection(std::span<const VertexId>a,std::span<const VertexId>b){std::size_t i=0,j=0,c=0;while(i<a.size()&&j<b.size()){if(a[i]==b[j]){++c;++i;++j;}else if(a[i]<b[j])++i;else ++j;}return c;}
inline std::size_t galloping_intersection(std::span<const VertexId>s,std::span<const VertexId>l){std::size_t c=0;for(auto x:s)c+=std::binary_search(l.begin(),l.end(),x)?1u:0u;return c;}
inline std::size_t avx2_intersection(std::span<const VertexId>a,std::span<const VertexId>b){return scalar_intersection(a,b);} // correctness baseline until calibrated intrinsic path
inline std::size_t avx512_intersection(std::span<const VertexId>a,std::span<const VertexId>b){return scalar_intersection(a,b);}
inline std::size_t neon_intersection(std::span<const VertexId>a,std::span<const VertexId>b){return scalar_intersection(a,b);}
inline IntersectionKernel select_intersection(std::size_t a,std::size_t b){const auto mn=std::min(a,b),mx=std::max(a,b);if(mn==0)return IntersectionKernel::scalar_merge;if(mx>16*mn)return IntersectionKernel::galloping;auto f=detect_cpu_features();if(f.avx512f&&mn>=64)return IntersectionKernel::avx512;if(f.avx2&&mn>=32)return IntersectionKernel::avx2;if(f.neon&&mn>=32)return IntersectionKernel::neon;return IntersectionKernel::scalar_merge;}
inline std::size_t adaptive_intersection(std::span<const VertexId>a,std::span<const VertexId>b){switch(select_intersection(a.size(),b.size())){case IntersectionKernel::galloping:return a.size()<=b.size()?galloping_intersection(a,b):galloping_intersection(b,a);case IntersectionKernel::avx2:return avx2_intersection(a,b);case IntersectionKernel::avx512:return avx512_intersection(a,b);case IntersectionKernel::neon:return neon_intersection(a,b);default:return scalar_intersection(a,b);}}
}
