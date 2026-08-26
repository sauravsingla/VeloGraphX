#pragma once
#include <cstddef>
namespace velographx { enum class TraversalDirection{push,pull}; inline TraversalDirection choose_direction(std::size_t frontier,std::size_t vertices,std::size_t frontier_edges,std::size_t total_edges){if(vertices==0)return TraversalDirection::push;const double fd=static_cast<double>(frontier)/vertices;const double ew=total_edges?static_cast<double>(frontier_edges)/total_edges:0.0;return (fd>0.08||ew>0.12)?TraversalDirection::pull:TraversalDirection::push;} }
