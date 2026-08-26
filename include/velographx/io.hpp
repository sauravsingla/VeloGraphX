#pragma once
#include "velographx/csr_graph.hpp"
#include <filesystem>

namespace velographx {
CsrGraph load_edge_list(const std::filesystem::path& path, bool directed = false);
}
