#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace velographx {

using VertexId = std::uint32_t;

struct EdgeUpdate {
  VertexId src{};
  VertexId dst{};
  bool add{true};
  std::uint64_t timestamp{0};
};

struct UpdateBatch {
  std::vector<EdgeUpdate> updates;
  void add(VertexId u, VertexId v, std::uint64_t ts = 0) { updates.push_back({u, v, true, ts}); }
  void remove(VertexId u, VertexId v, std::uint64_t ts = 0) { updates.push_back({u, v, false, ts}); }
  [[nodiscard]] bool empty() const noexcept { return updates.empty(); }
};

namespace storage_detail {

class SegmentedCsr {
 public:
  static constexpr std::size_t kVerticesPerSegment = 1u << 16;

  [[nodiscard]] std::size_t vertex_count() const noexcept { return vertex_count_; }
  [[nodiscard]] std::size_t edge_count() const noexcept { return edge_count_; }
  [[nodiscard]] std::size_t segment_count() const noexcept { return segments_.size(); }

  [[nodiscard]] std::size_t segment_begin(std::size_t index) const noexcept {
    return index < segments_.size() ? segments_[index].first_vertex : vertex_count_;
  }

  [[nodiscard]] std::size_t segment_end(std::size_t index) const noexcept {
    if (index >= segments_.size()) return vertex_count_;
    return segments_[index].first_vertex + segments_[index].vertex_count;
  }

  [[nodiscard]] std::size_t segment_edge_count(std::size_t index) const noexcept {
    return index < segments_.size() ? segments_[index].edges.size() : 0;
  }

  void resize_vertices(std::size_t vertices) {
    if (vertices <= vertex_count_) return;
    while (vertex_count_ < vertices) {
      const auto segment_index = vertex_count_ / kVerticesPerSegment;
      if (segment_index == segments_.size()) {
        Segment segment;
        segment.first_vertex = segment_index * kVerticesPerSegment;
        segment.offsets.push_back(0);
        segments_.push_back(std::move(segment));
      }
      auto& segment = segments_[segment_index];
      const auto target_count = std::min(kVerticesPerSegment, vertices - segment.first_vertex);
      segment.offsets.resize(target_count + 1, segment.edges.size());
      segment.vertex_count = target_count;
      vertex_count_ = segment.first_vertex + target_count;
    }
  }

  void clear(std::size_t vertices = 0) {
    segments_.clear();
    vertex_count_ = 0;
    edge_count_ = 0;
    resize_vertices(vertices);
  }

  void build(std::size_t vertices, std::vector<std::pair<VertexId, VertexId>> arcs) {
    clear(vertices);
    arcs.erase(std::remove_if(arcs.begin(), arcs.end(), [vertices](const auto& e) {
                 return e.first == e.second || e.first >= vertices || e.second >= vertices;
               }),
               arcs.end());
    std::sort(arcs.begin(), arcs.end());
    arcs.erase(std::unique(arcs.begin(), arcs.end()), arcs.end());

    for (const auto& [u, v] : arcs) {
      (void)v;
      auto& segment = segment_for(u);
      const auto local = static_cast<std::size_t>(u) - segment.first_vertex;
      ++segment.offsets[local + 1];
    }
    for (auto& segment : segments_) {
      for (std::size_t i = 1; i < segment.offsets.size(); ++i) {
        segment.offsets[i] += segment.offsets[i - 1];
      }
      segment.edges.resize(segment.offsets.back());
    }

    std::vector<std::vector<std::size_t>> cursors;
    cursors.reserve(segments_.size());
    for (const auto& segment : segments_) cursors.push_back(segment.offsets);
    for (const auto& [u, v] : arcs) {
      const auto segment_index = static_cast<std::size_t>(u) / kVerticesPerSegment;
      auto& segment = segments_[segment_index];
      const auto local = static_cast<std::size_t>(u) - segment.first_vertex;
      segment.edges[cursors[segment_index][local]++] = v;
    }
    edge_count_ = arcs.size();
  }

  template <class RowProvider>
  void build_from_rows(std::size_t vertices, RowProvider&& rows) {
    clear(vertices);
    edge_count_ = 0;
    for (std::size_t u = 0; u < vertices; ++u) {
      auto row_values = rows(static_cast<VertexId>(u));
      auto& segment = segment_for(static_cast<VertexId>(u));
      const auto local = u - segment.first_vertex;
      segment.offsets[local] = segment.edges.size();
      segment.edges.insert(segment.edges.end(), row_values.begin(), row_values.end());
      segment.offsets[local + 1] = segment.edges.size();
      edge_count_ += row_values.size();
    }
  }

  template <class RowProvider>
  void rebuild_segment(std::size_t index, RowProvider&& rows) {
    if (index >= segments_.size()) return;
    const auto& old = segments_[index];
    Segment rebuilt;
    rebuilt.first_vertex = old.first_vertex;
    rebuilt.vertex_count = old.vertex_count;
    rebuilt.offsets.reserve(rebuilt.vertex_count + 1);
    rebuilt.offsets.push_back(0);
    for (std::size_t local = 0; local < rebuilt.vertex_count; ++local) {
      const auto u = static_cast<VertexId>(rebuilt.first_vertex + local);
      auto row_values = rows(u);
      rebuilt.edges.insert(rebuilt.edges.end(), row_values.begin(), row_values.end());
      rebuilt.offsets.push_back(rebuilt.edges.size());
    }
    edge_count_ = edge_count_ - old.edges.size() + rebuilt.edges.size();
    segments_[index] = std::move(rebuilt);
  }

  void build_transpose_from(const SegmentedCsr& source) {
    clear(source.vertex_count());
    std::vector<std::size_t> indegree(vertex_count_, 0);
    for (std::size_t u = 0; u < source.vertex_count(); ++u) {
      for (auto v : source.row(static_cast<VertexId>(u))) ++indegree[v];
    }

    for (std::size_t v = 0; v < vertex_count_; ++v) {
      auto& segment = segment_for(static_cast<VertexId>(v));
      const auto local = v - segment.first_vertex;
      segment.offsets[local + 1] = indegree[v];
    }
    for (auto& segment : segments_) {
      for (std::size_t i = 1; i < segment.offsets.size(); ++i) {
        segment.offsets[i] += segment.offsets[i - 1];
      }
      segment.edges.resize(segment.offsets.back());
    }

    std::vector<std::vector<std::size_t>> cursors;
    cursors.reserve(segments_.size());
    for (const auto& segment : segments_) cursors.push_back(segment.offsets);
    for (std::size_t u = 0; u < source.vertex_count(); ++u) {
      for (auto v : source.row(static_cast<VertexId>(u))) {
        const auto segment_index = static_cast<std::size_t>(v) / kVerticesPerSegment;
        auto& segment = segments_[segment_index];
        const auto local = static_cast<std::size_t>(v) - segment.first_vertex;
        segment.edges[cursors[segment_index][local]++] = static_cast<VertexId>(u);
      }
    }
    edge_count_ = source.edge_count();
  }

  [[nodiscard]] std::span<const VertexId> row(VertexId u) const noexcept {
    if (u >= vertex_count_) return {};
    const auto segment_index = static_cast<std::size_t>(u) / kVerticesPerSegment;
    const auto& segment = segments_[segment_index];
    const auto local = static_cast<std::size_t>(u) - segment.first_vertex;
    const auto begin = segment.offsets[local];
    const auto end = segment.offsets[local + 1];
    return {segment.edges.data() + begin, end - begin};
  }

  [[nodiscard]] bool contains(VertexId u, VertexId v) const noexcept {
    const auto neighbors = row(u);
    return std::binary_search(neighbors.begin(), neighbors.end(), v);
  }

  [[nodiscard]] std::size_t storage_bytes() const noexcept {
    std::size_t bytes = sizeof(*this) + segments_.capacity() * sizeof(Segment);
    for (const auto& segment : segments_) {
      bytes += segment.offsets.capacity() * sizeof(std::size_t);
      bytes += segment.edges.capacity() * sizeof(VertexId);
    }
    return bytes;
  }

 private:
  struct Segment {
    std::size_t first_vertex{0};
    std::size_t vertex_count{0};
    std::vector<std::size_t> offsets;
    std::vector<VertexId> edges;
  };

  Segment& segment_for(VertexId u) {
    return segments_[static_cast<std::size_t>(u) / kVerticesPerSegment];
  }

  std::vector<Segment> segments_;
  std::size_t vertex_count_{0};
  std::size_t edge_count_{0};
};

class PackedDeltaStore {
 public:
  struct Entry {
    VertexId dst{};
    bool present{true};
  };

  void resize_vertices(std::size_t vertices) {
    if (vertices > rows_.size()) rows_.resize(vertices);
  }

  void clear() {
    arena_.clear();
    live_entries_ = 0;
    present_entries_ = 0;
    absent_entries_ = 0;
    for (auto& row_meta : rows_) row_meta = {};
  }

  [[nodiscard]] std::span<const Entry> row(VertexId u) const noexcept {
    if (u >= rows_.size()) return {};
    const auto& meta = rows_[u];
    if (meta.count == 0) return {};
    return {arena_.data() + meta.offset, meta.count};
  }

  [[nodiscard]] std::optional<bool> override_for(VertexId u, VertexId v) const noexcept {
    const auto entries = row(u);
    const auto it = std::lower_bound(entries.begin(), entries.end(), v,
                                     [](const Entry& e, VertexId target) { return e.dst < target; });
    if (it == entries.end() || it->dst != v) return std::nullopt;
    return it->present;
  }

  void set(VertexId u, VertexId v, bool desired_present, bool base_present) {
    resize_vertices(static_cast<std::size_t>(u) + 1);
    if (desired_present == base_present) {
      erase(u, v);
      return;
    }
    upsert(u, v, desired_present);
  }

  [[nodiscard]] std::size_t size() const noexcept { return live_entries_; }
  [[nodiscard]] std::size_t additions() const noexcept { return present_entries_; }
  [[nodiscard]] std::size_t deletions() const noexcept { return absent_entries_; }
  [[nodiscard]] bool empty() const noexcept { return live_entries_ == 0; }

  [[nodiscard]] std::size_t count_range(std::size_t begin, std::size_t end) const noexcept {
    end = std::min(end, rows_.size());
    std::size_t total = 0;
    for (std::size_t u = begin; u < end; ++u) total += rows_[u].count;
    return total;
  }

  void clear_range(std::size_t begin, std::size_t end) {
    end = std::min(end, rows_.size());
    for (std::size_t u = begin; u < end; ++u) {
      auto& meta = rows_[u];
      if (meta.count == 0) continue;
      for (const auto& entry : row(static_cast<VertexId>(u))) {
        if (entry.present) --present_entries_;
        else --absent_entries_;
      }
      live_entries_ -= meta.count;
      meta = {};
    }
  }

  [[nodiscard]] std::size_t storage_bytes() const noexcept {
    return sizeof(*this) + rows_.capacity() * sizeof(Row) + arena_.capacity() * sizeof(Entry);
  }

  void repack() {
    if (live_entries_ == 0) {
      arena_.clear();
      for (auto& row_meta : rows_) row_meta = {};
      return;
    }
    std::vector<Entry> packed;
    packed.reserve(live_entries_);
    for (auto& meta : rows_) {
      if (meta.count == 0) {
        meta = {};
        continue;
      }
      const auto old_offset = meta.offset;
      const auto count = meta.count;
      const auto new_offset = packed.size();
      packed.insert(packed.end(), arena_.begin() + old_offset,
                    arena_.begin() + old_offset + count);
      meta.offset = new_offset;
      meta.capacity = count;
    }
    arena_.swap(packed);
  }

  [[nodiscard]] double fragmentation_ratio() const noexcept {
    if (arena_.empty()) return 0.0;
    return 1.0 - static_cast<double>(live_entries_) / static_cast<double>(arena_.size());
  }

 private:
  static constexpr std::size_t kNoOffset = std::numeric_limits<std::size_t>::max();

  struct Row {
    std::size_t offset{kNoOffset};
    std::size_t count{0};
    std::size_t capacity{0};
  };

  void ensure_capacity(VertexId u, std::size_t required) {
    auto& meta = rows_[u];
    if (meta.capacity >= required) return;
    const auto new_capacity = std::max(required, std::max<std::size_t>(1, meta.capacity * 2));
    const auto new_offset = arena_.size();
    arena_.resize(new_offset + new_capacity);
    if (meta.count != 0) {
      std::copy_n(arena_.begin() + meta.offset, meta.count, arena_.begin() + new_offset);
    }
    meta.offset = new_offset;
    meta.capacity = new_capacity;
  }

  void upsert(VertexId u, VertexId v, bool present) {
    auto& meta = rows_[u];
    const auto entries = row(u);
    const auto it = std::lower_bound(entries.begin(), entries.end(), v,
                                     [](const Entry& e, VertexId target) { return e.dst < target; });
    const auto pos = static_cast<std::size_t>(it - entries.begin());
    if (it != entries.end() && it->dst == v) {
      if (it->present != present) {
        if (it->present) {
          --present_entries_;
          ++absent_entries_;
        } else {
          --absent_entries_;
          ++present_entries_;
        }
        arena_[meta.offset + pos].present = present;
      }
      return;
    }

    ensure_capacity(u, meta.count + 1);
    auto& refreshed = rows_[u];
    for (std::size_t i = refreshed.count; i > pos; --i) {
      arena_[refreshed.offset + i] = arena_[refreshed.offset + i - 1];
    }
    arena_[refreshed.offset + pos] = {v, present};
    ++refreshed.count;
    ++live_entries_;
    if (present) ++present_entries_;
    else ++absent_entries_;
  }

  void erase(VertexId u, VertexId v) {
    if (u >= rows_.size()) return;
    auto& meta = rows_[u];
    if (meta.count == 0) return;
    const auto entries = row(u);
    const auto it = std::lower_bound(entries.begin(), entries.end(), v,
                                     [](const Entry& e, VertexId target) { return e.dst < target; });
    if (it == entries.end() || it->dst != v) return;
    const auto pos = static_cast<std::size_t>(it - entries.begin());
    if (it->present) --present_entries_;
    else --absent_entries_;
    for (std::size_t i = pos + 1; i < meta.count; ++i) {
      arena_[meta.offset + i - 1] = arena_[meta.offset + i];
    }
    --meta.count;
    --live_entries_;
  }

  std::vector<Row> rows_;
  std::vector<Entry> arena_;
  std::size_t live_entries_{0};
  std::size_t present_entries_{0};
  std::size_t absent_entries_{0};
};

}  // namespace storage_detail

class IncrementalTriangleCount;

class DynamicGraph {
 public:
  explicit DynamicGraph(std::size_t vertices = 0, bool directed = false)
      : directed_(directed) {
    base_out_.resize_vertices(vertices);
    base_in_.resize_vertices(vertices);
    delta_out_.resize_vertices(vertices);
    delta_in_.resize_vertices(vertices);
    resize_dirty_maps();
  }

  [[nodiscard]] std::size_t vertex_count() const noexcept { return base_out_.vertex_count(); }
  [[nodiscard]] std::uint64_t version() const noexcept { return version_; }
  [[nodiscard]] bool directed() const noexcept { return directed_; }

  void ensure_vertex(VertexId v) {
    const auto n = static_cast<std::size_t>(v) + 1;
    if (n <= vertex_count()) return;
    base_out_.resize_vertices(n);
    base_in_.resize_vertices(n);
    delta_out_.resize_vertices(n);
    delta_in_.resize_vertices(n);
    resize_dirty_maps();
  }

  void bulk_load_edges(const std::vector<std::pair<VertexId, VertexId>>& edges) {
    VertexId max_vertex = 0;
    bool saw_edge = false;
    std::vector<std::pair<VertexId, VertexId>> arcs;
    arcs.reserve(directed_ ? edges.size() : edges.size() * 2);
    for (const auto& [u, v] : edges) {
      if (u == v) continue;
      max_vertex = std::max(max_vertex, std::max(u, v));
      saw_edge = true;
      arcs.emplace_back(u, v);
      if (!directed_) arcs.emplace_back(v, u);
    }
    if (saw_edge) ensure_vertex(max_vertex);

    base_out_.build(vertex_count(), std::move(arcs));
    base_in_.build_transpose_from(base_out_);
    delta_out_.clear();
    delta_in_.clear();
    delta_out_.resize_vertices(vertex_count());
    delta_in_.resize_vertices(vertex_count());
    resize_dirty_maps();
    clear_dirty_maps();
    ++version_;
  }

  void add_edge(VertexId u, VertexId v) {
    UpdateBatch batch;
    batch.add(u, v);
    apply(batch);
  }

  void remove_edge(VertexId u, VertexId v) {
    UpdateBatch batch;
    batch.remove(u, v);
    apply(batch);
  }

  void apply(const UpdateBatch& batch) {
    for (const auto& op : batch.updates) apply_unversioned(op);
    if (!batch.empty()) {
      ++version_;
      automatic_storage_maintenance();
    }
  }

  [[nodiscard]] std::vector<VertexId> neighbors(VertexId u) const {
    return materialize_row(base_out_, delta_out_, u);
  }

  [[nodiscard]] std::vector<VertexId> in_neighbors(VertexId v) const {
    return materialize_row(base_in_, delta_in_, v);
  }

  [[nodiscard]] bool is_compact() const noexcept {
    return delta_out_.empty() && delta_in_.empty();
  }

  [[nodiscard]] std::span<const VertexId> compact_neighbors(VertexId u) const noexcept {
    return base_out_.row(u);
  }

  [[nodiscard]] std::span<const VertexId> compact_in_neighbors(VertexId v) const noexcept {
    return base_in_.row(v);
  }

  [[nodiscard]] bool has_edge(VertexId u, VertexId v) const {
    if (u >= vertex_count()) return false;
    if (const auto overlay = delta_out_.override_for(u, v); overlay.has_value()) return *overlay;
    return base_out_.contains(u, v);
  }

  [[nodiscard]] std::size_t edge_count_directed() const noexcept {
    return base_out_.edge_count() + delta_out_.additions() - delta_out_.deletions();
  }

  [[nodiscard]] std::size_t base_edge_count_directed() const noexcept {
    return base_out_.edge_count();
  }

  [[nodiscard]] std::size_t delta_edge_count() const noexcept { return delta_out_.size(); }

  [[nodiscard]] std::size_t storage_bytes() const noexcept {
    return base_out_.storage_bytes() + base_in_.storage_bytes() +
           delta_out_.storage_bytes() + delta_in_.storage_bytes() +
           dirty_out_segments_.capacity() + dirty_in_segments_.capacity();
  }

  [[nodiscard]] double delta_ratio() const noexcept {
    return static_cast<double>(delta_out_.size()) /
           static_cast<double>(std::max<std::size_t>(1, base_out_.edge_count()));
  }

  [[nodiscard]] std::size_t dirty_out_segment_count() const noexcept {
    return static_cast<std::size_t>(std::count(dirty_out_segments_.begin(), dirty_out_segments_.end(), 1));
  }

  [[nodiscard]] std::size_t dirty_in_segment_count() const noexcept {
    return static_cast<std::size_t>(std::count(dirty_in_segments_.begin(), dirty_in_segments_.end(), 1));
  }

  bool maybe_compact(double threshold = 0.25) {
    bool compacted = false;
    compacted |= compact_dense_segments(base_out_, delta_out_, dirty_out_segments_, threshold,
                                        [this](VertexId u) { return neighbors(u); });
    compacted |= compact_dense_segments(base_in_, delta_in_, dirty_in_segments_, threshold,
                                        [this](VertexId v) { return in_neighbors(v); });
    if (compacted) {
      delta_out_.repack();
      delta_in_.repack();
    }
    return compacted;
  }

  void compact() {
    if (is_compact()) return;
    compact_marked_segments(base_out_, delta_out_, dirty_out_segments_,
                            [this](VertexId u) { return neighbors(u); });
    compact_marked_segments(base_in_, delta_in_, dirty_in_segments_,
                            [this](VertexId v) { return in_neighbors(v); });
    delta_out_.repack();
    delta_in_.repack();
  }

 private:
  friend class IncrementalTriangleCount;

  static std::vector<VertexId> materialize_row(const storage_detail::SegmentedCsr& base,
                                                const storage_detail::PackedDeltaStore& delta,
                                                VertexId u) {
    const auto base_row = base.row(u);
    const auto overlay = delta.row(u);
    if (overlay.empty()) return {base_row.begin(), base_row.end()};

    std::vector<VertexId> out;
    out.reserve(base_row.size() + overlay.size());
    std::size_t i = 0;
    std::size_t j = 0;
    while (i < base_row.size() || j < overlay.size()) {
      if (j == overlay.size() || (i < base_row.size() && base_row[i] < overlay[j].dst)) {
        out.push_back(base_row[i++]);
      } else if (i == base_row.size() || overlay[j].dst < base_row[i]) {
        if (overlay[j].present) out.push_back(overlay[j].dst);
        ++j;
      } else {
        if (overlay[j].present) out.push_back(base_row[i]);
        ++i;
        ++j;
      }
    }
    return out;
  }

  void resize_dirty_maps() {
    dirty_out_segments_.resize(base_out_.segment_count(), 0);
    dirty_in_segments_.resize(base_in_.segment_count(), 0);
  }

  void clear_dirty_maps() {
    std::fill(dirty_out_segments_.begin(), dirty_out_segments_.end(), 0);
    std::fill(dirty_in_segments_.begin(), dirty_in_segments_.end(), 0);
  }

  void mark_dirty(VertexId u, VertexId v) {
    const auto out_segment = static_cast<std::size_t>(u) / storage_detail::SegmentedCsr::kVerticesPerSegment;
    const auto in_segment = static_cast<std::size_t>(v) / storage_detail::SegmentedCsr::kVerticesPerSegment;
    if (out_segment < dirty_out_segments_.size()) dirty_out_segments_[out_segment] = 1;
    if (in_segment < dirty_in_segments_.size()) dirty_in_segments_[in_segment] = 1;
  }

  void apply_unversioned(const EdgeUpdate& op) {
    ensure_vertex(std::max(op.src, op.dst));
    apply_arc(op.src, op.dst, op.add);
    if (!directed_ && op.src != op.dst) apply_arc(op.dst, op.src, op.add);
  }

  void apply_arc(VertexId u, VertexId v, bool present) {
    const bool current = has_edge(u, v);
    if (current == present) return;
    delta_out_.set(u, v, present, base_out_.contains(u, v));
    delta_in_.set(v, u, present, base_in_.contains(v, u));
    mark_dirty(u, v);
  }

  template <class RowProvider>
  static void compact_marked_segments(storage_detail::SegmentedCsr& base,
                                      storage_detail::PackedDeltaStore& delta,
                                      std::vector<std::uint8_t>& dirty,
                                      RowProvider&& rows) {
    for (std::size_t segment = 0; segment < dirty.size(); ++segment) {
      if (!dirty[segment]) continue;
      const auto begin = base.segment_begin(segment);
      const auto end = base.segment_end(segment);
      base.rebuild_segment(segment, rows);
      delta.clear_range(begin, end);
      dirty[segment] = 0;
    }
  }

  template <class RowProvider>
  static bool compact_dense_segments(storage_detail::SegmentedCsr& base,
                                     storage_detail::PackedDeltaStore& delta,
                                     std::vector<std::uint8_t>& dirty,
                                     double threshold,
                                     RowProvider&& rows) {
    bool compacted = false;
    for (std::size_t segment = 0; segment < dirty.size(); ++segment) {
      if (!dirty[segment]) continue;
      const auto begin = base.segment_begin(segment);
      const auto end = base.segment_end(segment);
      const auto delta_entries = delta.count_range(begin, end);
      const auto base_edges = std::max<std::size_t>(1, base.segment_edge_count(segment));
      const double density = static_cast<double>(delta_entries) / static_cast<double>(base_edges);
      if (density < threshold) continue;
      base.rebuild_segment(segment, rows);
      delta.clear_range(begin, end);
      dirty[segment] = 0;
      compacted = true;
    }
    return compacted;
  }

  void automatic_storage_maintenance() {
    constexpr double kSegmentDeltaDensityThreshold = 0.25;
    constexpr double kFragmentationThreshold = 0.60;
    (void)maybe_compact(kSegmentDeltaDensityThreshold);
    if (delta_out_.fragmentation_ratio() > kFragmentationThreshold) delta_out_.repack();
    if (delta_in_.fragmentation_ratio() > kFragmentationThreshold) delta_in_.repack();
  }

  bool directed_{false};
  storage_detail::SegmentedCsr base_out_;
  storage_detail::SegmentedCsr base_in_;
  storage_detail::PackedDeltaStore delta_out_;
  storage_detail::PackedDeltaStore delta_in_;
  std::vector<std::uint8_t> dirty_out_segments_;
  std::vector<std::uint8_t> dirty_in_segments_;
  std::uint64_t version_{0};
};

}  // namespace velographx
