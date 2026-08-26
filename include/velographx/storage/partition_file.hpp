#pragma once

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(__linux__) || defined(__APPLE__)
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace velographx {

struct PartitionFileHeader {
  std::uint32_t magic{0x56475850u};  // VGXP
  std::uint16_t version{1};
  std::uint16_t flags{0};
  std::uint64_t partition_id{0};
  std::uint64_t payload_bytes{0};
};

class PartitionFile {
 public:
  static void write(const std::filesystem::path& path,
                    std::uint64_t partition_id,
                    const std::vector<std::uint8_t>& payload) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("failed to open partition file for write");
    PartitionFileHeader header;
    header.partition_id = partition_id;
    header.payload_bytes = static_cast<std::uint64_t>(payload.size());
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    if (!payload.empty()) {
      out.write(reinterpret_cast<const char*>(payload.data()),
                static_cast<std::streamsize>(payload.size()));
    }
    if (!out) throw std::runtime_error("failed to write partition file");
  }

  static std::vector<std::uint8_t> read(const std::filesystem::path& path,
                                        std::uint64_t expected_partition_id) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("failed to open partition file for read");
    PartitionFileHeader header{};
    in.read(reinterpret_cast<char*>(&header), sizeof(header));
    validate_header(header, expected_partition_id);
    std::vector<std::uint8_t> payload(static_cast<std::size_t>(header.payload_bytes));
    if (!payload.empty()) in.read(reinterpret_cast<char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
    if (!in) throw std::runtime_error("truncated partition payload");
    return payload;
  }

  static std::vector<std::uint8_t> read_mmap_or_fallback(
      const std::filesystem::path& path,
      std::uint64_t expected_partition_id,
      bool* used_mmap = nullptr) {
    if (used_mmap) *used_mmap = false;
#if defined(__linux__) || defined(__APPLE__)
    const int fd = ::open(path.c_str(), O_RDONLY);
    if (fd >= 0) {
      struct stat st {};
      if (::fstat(fd, &st) == 0 && st.st_size >= static_cast<off_t>(sizeof(PartitionFileHeader))) {
        void* addr = ::mmap(nullptr, static_cast<std::size_t>(st.st_size), PROT_READ, MAP_PRIVATE, fd, 0);
        if (addr != MAP_FAILED) {
          PartitionFileHeader header{};
          std::memcpy(&header, addr, sizeof(header));
          try {
            validate_header(header, expected_partition_id);
            const auto total = sizeof(PartitionFileHeader) + static_cast<std::size_t>(header.payload_bytes);
            if (total > static_cast<std::size_t>(st.st_size)) {
              throw std::runtime_error("truncated mmap partition payload");
            }
            const auto* begin = static_cast<const std::uint8_t*>(addr) + sizeof(PartitionFileHeader);
            std::vector<std::uint8_t> payload(begin, begin + static_cast<std::size_t>(header.payload_bytes));
            ::munmap(addr, static_cast<std::size_t>(st.st_size));
            ::close(fd);
            if (used_mmap) *used_mmap = true;
            return payload;
          } catch (...) {
            ::munmap(addr, static_cast<std::size_t>(st.st_size));
            ::close(fd);
            throw;
          }
        }
      }
      ::close(fd);
    }
#endif
    return read(path, expected_partition_id);
  }

 private:
  static void validate_header(const PartitionFileHeader& header,
                              std::uint64_t expected_partition_id) {
    if (header.magic != 0x56475850u) throw std::runtime_error("invalid partition magic");
    if (header.version != 1) throw std::runtime_error("unsupported partition version");
    if (header.partition_id != expected_partition_id) throw std::runtime_error("partition id mismatch");
  }
};

}  // namespace velographx
