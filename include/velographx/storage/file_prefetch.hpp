#pragma once

#include <cstdint>
#include <filesystem>

#if defined(__linux__)
#include <fcntl.h>
#include <unistd.h>
#endif

namespace velographx {

struct FilePrefetchResult {
  bool supported{false};
  bool advised{false};
};

class FilePrefetchAdvisor {
 public:
  static FilePrefetchResult advise_will_need(const std::filesystem::path& path) noexcept {
#if defined(__linux__)
    const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) return {true, false};
    const int rc = ::posix_fadvise(fd, 0, 0, POSIX_FADV_WILLNEED);
    ::close(fd);
    return {true, rc == 0};
#else
    (void)path;
    return {false, false};
#endif
  }

  [[nodiscard]] static constexpr bool supported() noexcept {
#if defined(__linux__)
    return true;
#else
    return false;
#endif
  }
};

}  // namespace velographx
