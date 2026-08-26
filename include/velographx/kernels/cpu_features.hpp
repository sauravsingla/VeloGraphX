#pragma once
#include <string>
namespace velographx::kernels {
struct CpuFeatures { bool avx2{false}; bool avx512f{false}; bool neon{false}; };
inline CpuFeatures detect_cpu_features(){ CpuFeatures f;
#if defined(__x86_64__) || defined(_M_X64)
#if defined(__GNUC__) || defined(__clang__)
  __builtin_cpu_init(); f.avx2=__builtin_cpu_supports("avx2"); f.avx512f=__builtin_cpu_supports("avx512f");
#endif
#endif
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
  f.neon=true;
#endif
  return f; }
inline std::string best_isa(){auto f=detect_cpu_features();if(f.avx512f)return "avx512";if(f.avx2)return "avx2";if(f.neon)return "neon";return "scalar";}
}
