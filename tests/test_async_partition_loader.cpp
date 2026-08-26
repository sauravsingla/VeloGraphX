#include "velographx/storage/async_partition_loader.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <vector>

int main() {
  namespace fs = std::filesystem;
  const auto dir = fs::temp_directory_path() / "velographx_async_partition_loader";
  fs::create_directories(dir);

  const auto p1 = dir / "p1.vgxp";
  const auto p2 = dir / "p2.vgxp";
  const std::vector<std::uint8_t> a{1, 2, 3, 4};
  const std::vector<std::uint8_t> b{5, 6, 7, 8};
  velographx::PartitionFile::write(p1, 1, a);
  velographx::PartitionFile::write(p2, 2, b);

  velographx::AsyncPartitionLoader loader(8);
  auto f = loader.prefetch(1, p1);
  assert(f.get() == a);
  assert(loader.resident_bytes() == a.size());

  const auto io_uring_result = loader.last_io_uring_result();
#if defined(VELOGRAPHX_ENABLE_IO_URING) && defined(__linux__)
  assert(velographx::AsyncPartitionLoader::io_uring_prefetch_compiled());
  assert(io_uring_result.compiled);
  assert(io_uring_result.attempted);
#else
  assert(!velographx::AsyncPartitionLoader::io_uring_prefetch_compiled());
  assert(!io_uring_result.compiled);
  assert(!io_uring_result.attempted);
#endif

  const auto prefetch_result = loader.last_prefetch_result();
#if defined(__linux__)
  assert(velographx::AsyncPartitionLoader::native_prefetch_supported());
  if (!io_uring_result.succeeded) {
    assert(prefetch_result.supported);
    assert(prefetch_result.advised);
  }
#else
  assert(!velographx::AsyncPartitionLoader::native_prefetch_supported());
  assert(!prefetch_result.supported);
  assert(!prefetch_result.advised);
#endif

  auto cached = loader.load(1, p1);
  assert(cached == a);
  assert(loader.stats().hits >= 1);

  auto second = loader.prefetch(2, p2).get();
  assert(second == b);
  assert(loader.resident_bytes() == 8);

  fs::remove_all(dir);
  return 0;
}
