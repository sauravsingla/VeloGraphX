#include "velographx/io.hpp"

#include <cctype>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace velographx {
namespace {

std::string_view trim_left(std::string_view value) {
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
    value.remove_prefix(1);
  }
  return value;
}

}  // namespace

CsrGraph load_edge_list(const std::filesystem::path& path, bool directed) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("unable to open edge-list file");

  std::vector<CsrGraph::Edge> edges;
  std::string line;
  std::size_t line_number = 0;
  while (std::getline(input, line)) {
    ++line_number;
    auto view = trim_left(line);
    if (view.empty() || view.front() == '#' || view.front() == '%') continue;

    std::istringstream row{std::string(view)};
    std::uint64_t u = 0;
    std::uint64_t v = 0;
    if (!(row >> u >> v)) {
      throw std::runtime_error("malformed edge list at line " + std::to_string(line_number));
    }
    if (u > std::numeric_limits<VertexId>::max() || v > std::numeric_limits<VertexId>::max()) {
      throw std::overflow_error("vertex id exceeds uint32 range at line " + std::to_string(line_number));
    }

    row >> std::ws;
    if (!row.eof()) {
      throw std::runtime_error("unexpected trailing data at line " + std::to_string(line_number));
    }

    edges.emplace_back(static_cast<VertexId>(u), static_cast<VertexId>(v));
  }
  return CsrGraph(std::move(edges), directed);
}
}  // namespace velographx
