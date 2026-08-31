#pragma once

#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

#include "velographx/types.hpp"

namespace velographx {

// Lightweight C++20 graph customisation layer. Algorithms depend on these
// operations rather than concrete storage classes. Foreign graph types can opt
// in by exposing the small member surface below or by being wrapped in an
// adapter; no inheritance is required.
template <class Graph>
concept ReadableGraph = requires(const Graph& graph, VertexId vertex) {
  { graph.vertex_count() } -> std::convertible_to<std::size_t>;
  { graph.directed() } -> std::convertible_to<bool>;
};

template <class Graph>
concept MutableGraph = ReadableGraph<Graph> && requires(Graph& graph) {
  { graph.version() } -> std::convertible_to<std::uint64_t>;
};

namespace graph_access_detail {

template <class T>
[[nodiscard]] constexpr VertexId neighbor_target(const T& value) noexcept {
  if constexpr (requires { value.first; }) {
    return static_cast<VertexId>(value.first);
  } else {
    return static_cast<VertexId>(value);
  }
}

}  // namespace graph_access_detail

template <ReadableGraph Graph>
[[nodiscard]] constexpr std::size_t vertex_count(const Graph& graph) noexcept(noexcept(graph.vertex_count())) {
  return static_cast<std::size_t>(graph.vertex_count());
}

template <ReadableGraph Graph>
[[nodiscard]] constexpr bool is_directed(const Graph& graph) noexcept(noexcept(graph.directed())) {
  return static_cast<bool>(graph.directed());
}

template <ReadableGraph Graph, class Fn>
void for_each_neighbor(const Graph& graph, VertexId u, Fn&& fn) {
  if constexpr (requires { graph.for_each_neighbor(u, std::forward<Fn>(fn)); }) {
    graph.for_each_neighbor(u, std::forward<Fn>(fn));
  } else {
    for (const auto& neighbor : graph.neighbors(u)) {
      fn(graph_access_detail::neighbor_target(neighbor));
    }
  }
}

template <ReadableGraph Graph, class Fn>
void for_each_in_neighbor(const Graph& graph, VertexId v, Fn&& fn) {
  if constexpr (requires { graph.for_each_in_neighbor(v, std::forward<Fn>(fn)); }) {
    graph.for_each_in_neighbor(v, std::forward<Fn>(fn));
  } else if constexpr (requires { graph.in_neighbors(v); }) {
    for (const auto& neighbor : graph.in_neighbors(v)) {
      fn(graph_access_detail::neighbor_target(neighbor));
    }
  } else {
    // Portable fallback for read-only foreign graph types. Adapters should
    // provide in-neighbor iteration when PageRank-style workloads matter.
    for (VertexId u = 0; u < vertex_count(graph); ++u) {
      for_each_neighbor(graph, u, [&](VertexId dst) {
        if (dst == v) fn(u);
      });
    }
  }
}

template <ReadableGraph Graph>
[[nodiscard]] std::size_t neighbor_count(const Graph& graph, VertexId u) {
  if constexpr (requires { graph.degree(u); }) {
    return static_cast<std::size_t>(graph.degree(u));
  } else {
    std::size_t count = 0;
    for_each_neighbor(graph, u, [&](VertexId) { ++count; });
    return count;
  }
}

template <ReadableGraph Graph>
[[nodiscard]] bool has_edge(const Graph& graph, VertexId u, VertexId v) {
  if constexpr (requires { graph.has_edge(u, v); }) {
    return graph.has_edge(u, v);
  } else {
    bool found = false;
    for_each_neighbor(graph, u, [&](VertexId dst) { found = found || dst == v; });
    return found;
  }
}

template <class Graph, class Batch>
void apply_updates(Graph& graph, const Batch& batch) {
  graph.apply(batch);
}

}  // namespace velographx
