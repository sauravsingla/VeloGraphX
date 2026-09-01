#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "velographx/types.hpp"

namespace velographx {

namespace graph_access_detail {

template <class T>
[[nodiscard]] constexpr VertexId neighbor_target(const T& value) noexcept {
  if constexpr (requires { value.first; }) {
    return static_cast<VertexId>(value.first);
  } else {
    return static_cast<VertexId>(value);
  }
}

template <class Graph>
concept MemberVertexCount = requires(const Graph& graph) {
  { graph.vertex_count() } -> std::convertible_to<std::size_t>;
};

template <class Graph>
concept AdlVertexCount = requires(const Graph& graph) {
  { vx_vertex_count(graph) } -> std::convertible_to<std::size_t>;
};

template <class Graph>
concept MemberDirected = requires(const Graph& graph) {
  { graph.directed() } -> std::convertible_to<bool>;
};

template <class Graph>
concept AdlDirected = requires(const Graph& graph) {
  { vx_is_directed(graph) } -> std::convertible_to<bool>;
};

template <class Graph>
concept MemberVersion = requires(const Graph& graph) {
  { graph.version() } -> std::convertible_to<std::uint64_t>;
};

template <class Graph>
concept AdlVersion = requires(const Graph& graph) {
  { vx_version(graph) } -> std::convertible_to<std::uint64_t>;
};

}  // namespace graph_access_detail

template <class Graph>
concept ReadableGraph =
    (graph_access_detail::MemberVertexCount<Graph> || graph_access_detail::AdlVertexCount<Graph>) &&
    (graph_access_detail::MemberDirected<Graph> || graph_access_detail::AdlDirected<Graph>);

template <class Graph>
concept MutableGraph = ReadableGraph<Graph> &&
    (graph_access_detail::MemberVersion<Graph> || graph_access_detail::AdlVersion<Graph>);

template <ReadableGraph Graph>
[[nodiscard]] constexpr std::size_t vertex_count(const Graph& graph) {
  if constexpr (graph_access_detail::MemberVertexCount<Graph>) {
    return static_cast<std::size_t>(graph.vertex_count());
  } else {
    return static_cast<std::size_t>(vx_vertex_count(graph));
  }
}

template <ReadableGraph Graph>
[[nodiscard]] constexpr bool is_directed(const Graph& graph) {
  if constexpr (graph_access_detail::MemberDirected<Graph>) {
    return static_cast<bool>(graph.directed());
  } else {
    return static_cast<bool>(vx_is_directed(graph));
  }
}

template <MutableGraph Graph>
[[nodiscard]] constexpr std::uint64_t graph_version(const Graph& graph) {
  if constexpr (graph_access_detail::MemberVersion<Graph>) {
    return static_cast<std::uint64_t>(graph.version());
  } else {
    return static_cast<std::uint64_t>(vx_version(graph));
  }
}

template <ReadableGraph Graph, class Fn>
void for_each_neighbor(const Graph& graph, VertexId u, Fn&& fn) {
  if constexpr (requires { graph.for_each_neighbor(u, std::forward<Fn>(fn)); }) {
    graph.for_each_neighbor(u, std::forward<Fn>(fn));
  } else if constexpr (requires { vx_for_each_neighbor(graph, u, std::forward<Fn>(fn)); }) {
    vx_for_each_neighbor(graph, u, std::forward<Fn>(fn));
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
  } else if constexpr (requires { vx_for_each_in_neighbor(graph, v, std::forward<Fn>(fn)); }) {
    vx_for_each_in_neighbor(graph, v, std::forward<Fn>(fn));
  } else if constexpr (requires { graph.in_neighbors(v); }) {
    for (const auto& neighbor : graph.in_neighbors(v)) {
      fn(graph_access_detail::neighbor_target(neighbor));
    }
  } else {
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
  } else if constexpr (requires { vx_neighbor_count(graph, u); }) {
    return static_cast<std::size_t>(vx_neighbor_count(graph, u));
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
  } else if constexpr (requires { vx_has_edge(graph, u, v); }) {
    return vx_has_edge(graph, u, v);
  } else {
    bool found = false;
    for_each_neighbor(graph, u, [&](VertexId dst) { found = found || dst == v; });
    return found;
  }
}

template <class Graph, class Batch>
void apply_updates(Graph& graph, const Batch& batch) {
  if constexpr (requires { graph.apply(batch); }) {
    graph.apply(batch);
  } else {
    vx_apply_updates(graph, batch);
  }
}

}  // namespace velographx
