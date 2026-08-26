#pragma once
#include <thread>
#include <string>

namespace velographx {
enum class NumaMode { auto_detect, off, interleave };
struct NumaInfo { std::size_t hardware_threads{std::thread::hardware_concurrency()}; std::size_t nodes{1}; bool native_support{false}; };
inline NumaInfo detect_numa() { return {}; }
inline std::string numa_mode_name(NumaMode m){ return m==NumaMode::off?"off":(m==NumaMode::interleave?"interleave":"auto"); }
} // namespace velographx
