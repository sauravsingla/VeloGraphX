#include "velographx/io.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace velographx {
CsrGraph load_edge_list(const std::filesystem::path& path, bool directed) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("unable to open edge-list file");
  std::vector<CsrGraph::Edge> edges;
  std::string line;
  std::size_t line_number = 0;
  while (std::getline(input, line)) {
    ++line_number;
    if (line.empty() || line[0] == '#' || line[0] == '%') continue;
    std::istringstream row(line);
    std::uint64_t u = 0, v = 0;
    if (!(row >> u >> v)) throw std::runtime_error("malformed edge list at line " + std::to_string(line_number));
    if (u > UINT32_MAX || v > UINT32_MAX) throw std::overflow_error("vertex id exceeds uint32 range");
    edges.emplace_back(static_cast<VertexId>(u), static_cast<VertexId>(v));
  }
  return CsrGraph(std::move(edges), directed);
}
}
