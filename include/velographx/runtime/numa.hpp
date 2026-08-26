#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace velographx {

enum class NumaMode { auto_detect, off, interleave };

struct NumaNodeInfo {
  std::size_t id{0};
  std::vector<std::size_t> cpus;
};

struct NumaInfo {
  std::size_t hardware_threads{std::thread::hardware_concurrency()};
  std::size_t nodes{1};
  bool native_support{false};
  std::vector<NumaNodeInfo> topology;
};

inline std::vector<std::size_t> parse_cpu_list(const std::string& text) {
  std::vector<std::size_t> cpus;
  std::size_t pos = 0;
  while (pos < text.size()) {
    const auto comma = text.find(',', pos);
    const auto token = text.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
    const auto dash = token.find('-');
    if (!token.empty()) {
      if (dash == std::string::npos) {
        try { cpus.push_back(static_cast<std::size_t>(std::stoull(token))); } catch (...) {}
      } else {
        try {
          const auto first = static_cast<std::size_t>(std::stoull(token.substr(0, dash)));
          const auto last = static_cast<std::size_t>(std::stoull(token.substr(dash + 1)));
          if (last >= first) for (std::size_t cpu = first; cpu <= last; ++cpu) cpus.push_back(cpu);
        } catch (...) {}
      }
    }
    if (comma == std::string::npos) break;
    pos = comma + 1;
  }
  return cpus;
}

inline NumaInfo detect_numa() {
  NumaInfo info;
#if defined(__linux__)
  namespace fs = std::filesystem;
  const fs::path root{"/sys/devices/system/node"};
  std::error_code ec;
  if (fs::exists(root, ec) && !ec) {
    for (const auto& entry : fs::directory_iterator(root, ec)) {
      if (ec) break;
      const auto name = entry.path().filename().string();
      if (name.rfind("node", 0) != 0 || name.size() <= 4) continue;
      try {
        const auto id = static_cast<std::size_t>(std::stoull(name.substr(4)));
        std::ifstream in(entry.path() / "cpulist");
        if (!in) continue;
        std::string cpulist;
        std::getline(in, cpulist);
        info.topology.push_back({id, parse_cpu_list(cpulist)});
      } catch (...) {}
    }
    if (!info.topology.empty()) {
      info.nodes = info.topology.size();
      info.native_support = true;
    }
  }
#endif
  return info;
}

inline std::string numa_mode_name(NumaMode mode) {
  return mode == NumaMode::off ? "off" : (mode == NumaMode::interleave ? "interleave" : "auto");
}

}  // namespace velographx
