#include "velographx/storage/partition_file.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

int main() {
  namespace fs = std::filesystem;
  const auto path = fs::temp_directory_path() / "velographx_partition_test.vgxp";
  const std::vector<std::uint8_t> payload{1, 2, 3, 5, 8, 13};
  velographx::PartitionFile::write(path, 7, payload);

  const auto normal = velographx::PartitionFile::read(path, 7);
  assert(normal == payload);

  bool used_mmap = false;
  const auto mapped = velographx::PartitionFile::read_mmap_or_fallback(path, 7, &used_mmap);
  assert(mapped == payload);

  bool mismatch = false;
  try { (void)velographx::PartitionFile::read(path, 8); } catch (...) { mismatch = true; }
  assert(mismatch);

  {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    const std::uint32_t bad = 0;
    out.write(reinterpret_cast<const char*>(&bad), sizeof(bad));
  }
  bool bad_header = false;
  try { (void)velographx::PartitionFile::read(path, 7); } catch (...) { bad_header = true; }
  assert(bad_header);

  fs::remove(path);
  return 0;
}
