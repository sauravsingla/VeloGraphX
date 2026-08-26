#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

#if defined(VELOGRAPHX_ENABLE_IO_URING) && defined(__linux__) && __has_include(<liburing.h>)
#include <fcntl.h>
#include <liburing.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace velographx {

struct IoUringPrefetchResult {
  bool compiled{false};
  bool attempted{false};
  bool succeeded{false};
  std::size_t bytes_requested{0};
  int error_code{0};
};

class IoUringPrefetchAdvisor {
 public:
  [[nodiscard]] static constexpr bool compiled() noexcept {
#if defined(VELOGRAPHX_ENABLE_IO_URING) && defined(__linux__) && __has_include(<liburing.h>)
    return true;
#else
    return false;
#endif
  }

  static IoUringPrefetchResult prefetch(const std::filesystem::path& path,
                                        std::size_t max_bytes = 1U << 20U) noexcept {
    IoUringPrefetchResult result;
    result.compiled = compiled();
#if defined(VELOGRAPHX_ENABLE_IO_URING) && defined(__linux__) && __has_include(<liburing.h>)
    result.attempted = true;
    const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
      result.error_code = errno;
      return result;
    }

    struct stat st {};
    if (::fstat(fd, &st) != 0 || st.st_size <= 0) {
      result.error_code = errno;
      ::close(fd);
      return result;
    }

    const auto bytes = static_cast<std::size_t>(st.st_size) < max_bytes
                           ? static_cast<std::size_t>(st.st_size)
                           : max_bytes;
    result.bytes_requested = bytes;
    std::vector<std::uint8_t> buffer(bytes);

    io_uring ring {};
    int rc = io_uring_queue_init(2, &ring, 0);
    if (rc < 0) {
      result.error_code = -rc;
      ::close(fd);
      return result;
    }

    io_uring_sqe* sqe = io_uring_get_sqe(&ring);
    if (sqe == nullptr) {
      result.error_code = EBUSY;
      io_uring_queue_exit(&ring);
      ::close(fd);
      return result;
    }

    io_uring_prep_read(sqe, fd, buffer.data(), static_cast<unsigned>(buffer.size()), 0);
    rc = io_uring_submit(&ring);
    if (rc < 0) {
      result.error_code = -rc;
      io_uring_queue_exit(&ring);
      ::close(fd);
      return result;
    }

    io_uring_cqe* cqe = nullptr;
    rc = io_uring_wait_cqe(&ring, &cqe);
    if (rc < 0) {
      result.error_code = -rc;
    } else if (cqe->res < 0) {
      result.error_code = -cqe->res;
      io_uring_cqe_seen(&ring, cqe);
    } else {
      result.succeeded = true;
      io_uring_cqe_seen(&ring, cqe);
    }

    io_uring_queue_exit(&ring);
    ::close(fd);
#else
    (void)path;
    (void)max_bytes;
#endif
    return result;
  }
};

}  // namespace velographx
