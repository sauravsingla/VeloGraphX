#pragma once
#include <cstddef>
#include <cstdlib>
#include <new>

namespace velographx::memory {
inline void* aligned_allocate(std::size_t bytes, std::size_t alignment = 64) {
#if defined(_MSC_VER)
  auto* p = _aligned_malloc(bytes, alignment); if(!p) throw std::bad_alloc{}; return p;
#else
  void* p=nullptr; if(posix_memalign(&p, alignment, bytes)!=0) throw std::bad_alloc{}; return p;
#endif
}
inline void aligned_deallocate(void* p) noexcept {
#if defined(_MSC_VER)
  _aligned_free(p);
#else
  std::free(p);
#endif
}
} // namespace velographx::memory
