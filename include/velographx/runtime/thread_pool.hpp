#pragma once
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace velographx {
class ThreadPool {
 public:
  explicit ThreadPool(std::size_t threads = std::thread::hardware_concurrency()) : stop_(false) {
    if (threads == 0) threads = 1;
    workers_.reserve(threads);
    for (std::size_t i=0;i<threads;++i) workers_.emplace_back([this]{ worker(); });
  }
  ~ThreadPool() {
    { std::lock_guard<std::mutex> lock(mu_); stop_ = true; }
    cv_.notify_all();
    for (auto& t : workers_) if (t.joinable()) t.join();
  }
  void submit(std::function<void()> fn) {
    { std::lock_guard<std::mutex> lock(mu_); tasks_.push(std::move(fn)); }
    cv_.notify_one();
  }
  void wait_idle() {
    std::unique_lock<std::mutex> lock(mu_);
    idle_cv_.wait(lock,[this]{ return tasks_.empty() && active_==0; });
  }
 private:
  void worker() {
    for (;;) {
      std::function<void()> fn;
      { std::unique_lock<std::mutex> lock(mu_); cv_.wait(lock,[this]{return stop_||!tasks_.empty();}); if(stop_&&tasks_.empty()) return; fn=std::move(tasks_.front()); tasks_.pop(); ++active_; }
      fn();
      { std::lock_guard<std::mutex> lock(mu_); --active_; if(tasks_.empty()&&active_==0) idle_cv_.notify_all(); }
    }
  }
  std::vector<std::thread> workers_; std::queue<std::function<void()>> tasks_; std::mutex mu_; std::condition_variable cv_, idle_cv_; bool stop_; std::size_t active_{0};
};
} // namespace velographx
