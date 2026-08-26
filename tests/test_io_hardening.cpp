#include "velographx/io.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {

std::filesystem::path write_temp(const std::string& name, const std::string& content) {
  const auto path = std::filesystem::temp_directory_path() / name;
  std::ofstream out(path, std::ios::binary);
  out << content;
  return path;
}

template <class Exception>
void expect_failure(const std::filesystem::path& path) {
  bool threw = false;
  try {
    (void)velographx::load_edge_list(path, true);
  } catch (const Exception&) {
    threw = true;
  }
  assert(threw);
  std::filesystem::remove(path);
}

}  // namespace

int main() {
  {
    const auto path = write_temp("velographx_io_comments.txt",
                                 "   # comment\n\t% another comment\n0 1\n1 2   \n");
    const auto graph = velographx::load_edge_list(path, true);
    assert(graph.vertex_count() == 3);
    assert(graph.edge_count() == 2);
    std::filesystem::remove(path);
  }

  expect_failure<std::runtime_error>(
      write_temp("velographx_io_missing_col.txt", "0\n"));
  expect_failure<std::runtime_error>(
      write_temp("velographx_io_text.txt", "abc def\n"));
  expect_failure<std::runtime_error>(
      write_temp("velographx_io_trailing.txt", "0 1 junk\n"));
  expect_failure<std::overflow_error>(
      write_temp("velographx_io_overflow.txt", "4294967296 1\n"));
  expect_failure<std::runtime_error>(
      write_temp("velographx_io_mixed.txt", "0 1\n1 2 extra\n2 3\n"));

  return 0;
}
