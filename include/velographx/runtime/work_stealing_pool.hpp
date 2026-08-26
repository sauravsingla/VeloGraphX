#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace velographx {

struct WorkStealingStats {
  std::size_t submitted{0};
  std::size_t executed{0};
  std::size_t steal_attempts{0};
  std::size_t successful_steals{0};
};

class WorkStealingPool {
 public:
  using Task = std::function<void()>;

  explicit WorkStealingPool(std::size_t threads = std::thread::hardware_concurrency()) {
    if (threads == 0) threads = 1;
    queues_.reserve(threads);
    for (std::size_t i = 0; i < threads; ++i) queues_.push_back(std::make_unique<Queue>());
    workers_.reserve(threads);
    for (std::size_t i = 0; i < threads; ++i) workers_.emplace_back([this, i] { worker(i); });
  }

  WorkStealingPool(const WorkStealingPool&) = delete;
  WorkStealingPool& operator=(const WorkStealingPool&) = delete;

  ~WorkStealingPool() {
    wait_idle();
    stop_.store(true, std::memory_order_release);
    cv_.notify_all();
    for (auto& worker : workers_) if (worker.joinable()) worker.join();
  }

  std::size_t size() const noexcept { return workers_.size(); }

  void submit(Task task, std::size_t locality_hint = 0) {
    const auto queue = locality_hint % queues_.size();
    {
      std::lock_guard<std::mutex> lock(queues_[queue]->mutex);
      queues_[queue]->tasks.push_back(std::move(task));
    }
    submitted_.fetch_add(1, std::memory_order_relaxed);
    outstanding_.fetch_add(1, std::memory_order_release);
    cv_.notify_one();
  }

  template <class Fn>
  void parallel_for(std::size_t begin, std::size_t end, Fn&& fn,
                    std::size_t grain = 0) {
    if (end <= begin) return;
    if (grain == 0) grain = adaptive_grain(end - begin, size());
    std::size_t hint = 0;
    for (std::size_t first = begin; first < end; first += grain, ++hint) {
      const auto last = (first + grain < end) ? first + grain : end;
      submit([first, last, &fn] {
        for (std::size_t i = first; i < last; ++i) fn(i);
      }, hint);
    }
    wait_idle();
  }

  void wait_idle() {
    std::unique_lock<std::mutex> lock(wait_mutex_);
    idle_cv_.wait(lock, [this] { return outstanding_.load(std::memory_order_acquire) == 0; });
  }

  WorkStealingStats stats() const noexcept {
    return {submitted_.load(std::memory_order_relaxed), executed_.load(std::memory_order_relaxed),
            steal_attempts_.load(std::memory_order_relaxed), successful_steals_.load(std::memory_order_relaxed)};
  }

  static std::size_t adaptive_grain(std::size_t work_items, std::size_t threads) noexcept {
    if (threads == 0) threads = 1;
    const std::size_t target_chunks = threads * 8;
    const std::size_t grain = (work_items + target_chunks - 1) / target_chunks;
    return grain == 0 ? 1 : grain;
  }

 private:
  struct Queue {
    std::mutex mutex;
    std::deque<Task> tasks;
  };

  bool pop_local(std::size_t index, Task& task) {
    std::lock_guard<std::mutex> lock(queues_[index]->mutex);
    if (queues_[index]->tasks.empty()) return false;
    task = std::move(queues_[index]->tasks.back());
    queues_[index]->tasks.pop_back();
    return true;
  }

  bool steal(std::size_t thief, Task& task) {
    for (std::size_t offset = 1; offset < queues_.size(); ++offset) {
      steal_attempts_.fetch_add(1, std::memory_order_relaxed);
      const std::size_t victim = (thief + offset) % queues_.size();
      std::lock_guard<std::mutex> lock(queues_[victim]->mutex);
      if (queues_[victim]->tasks.empty()) continue;
      task = std::move(queues_[victim]->tasks.front());
      queues_[victim]->tasks.pop_front();
      successful_steals_.fetch_add(1, std::memory_order_relaxed);
      return true;
    }
    return false;
  }

  void worker(std::size_t index) {
    while (!stop_.load(std::memory_order_acquire)) {
      Task task;
      if (pop_local(index, task) || steal(index, task)) {
        task();
        executed_.fetch_add(1, std::memory_order_relaxed);
        if (outstanding_.fetch_sub(1, std::memory_order_acq_rel) == 1) idle_cv_.notify_all();
        continue;
      }
      std::unique_lock<std::mutex> lock(cv_mutex_);
      cv_.wait_for(lock, std::chrono::milliseconds(1), [this] {
        return stop_.load(std::memory_order_acquire) || outstanding_.load(std::memory_order_acquire) != 0;
      });
    }
  }

  std::vector<std::unique_ptr<Queue>> queues_;
  std::vector<std::thread> workers_;
  std::atomic<bool> stop_{false};
  std::atomic<std::size_t> outstanding_{0};
  std::atomic<std::size_t> submitted_{0};
  std::atomic<std::size_t> executed_{0};
  std::atomic<std::size_t> steal_attempts_{0};
  std::atomic<std::size_t> successful_steals_{0};
  std::mutex cv_mutex_;
  std::condition_variable cv_;
  std::mutex wait_mutex_;
  std::condition_variable idle_cv_;
};

}  // namespace velographx
