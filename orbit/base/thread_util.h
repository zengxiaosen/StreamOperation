/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: caixinyu@orangelab.cn (Cai Xinyu)
 *
 * thread_util.h
 * ---------------------------------------------------------------------------
 * Some utilities for multi-thread programming.
 * ---------------------------------------------------------------------------
 * Created on : August 27, 2016 (Cai Xinyu)
 */

#pragma once

#include <stdint.h>

#include <list>
#include <mutex>
#include <condition_variable>

#include "timeutil.h"

namespace orbit {

template <typename T>
class ProductQueue final {
 public:
  typedef typename std::list<T>::size_type size_type;
  static const size_type NOLIMIT = 0;
  ProductQueue(size_type limit = NOLIMIT) : limit_(limit) {}
  size_type Limit() const {
    // lock is not needed here.
    return limit_;
  }
  size_type Size() const {
    std::unique_lock<std::mutex> lock(mutex_);
    return queue_.size();
  }
  bool Empty() const {
    std::unique_lock<std::mutex> lock(mutex_);
    return queue_.empty();
  }
  bool HasSpace() const {
    std::unique_lock<std::mutex> lock(mutex_);
    return PrivHasSpace();
  }
  void Add(const T &e) {
    std::unique_lock<std::mutex> lock(mutex_);
    while (!PrivHasSpace()) {
      cv_has_space_.wait(lock);
    }
    queue_.push_back(e);
    cv_has_element_.notify_all();
  }
  bool TryAdd(const T &e) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (PrivHasSpace()) {
      queue_.push_back(e);
      cv_has_element_.notify_all();
      return true;
    }
    return false;
  }
  bool TimedAdd(uint64_t timeout, const T &e) {
    std::unique_lock<std::mutex> lock(mutex_);
    while (!PrivHasSpace()) {
      long long start = GetCurrentTime_MS();
      if (cv_has_space_.wait_for(lock, std::chrono::milliseconds(timeout))
          == std::cv_status::timeout) {
        return false;
      }
      timeout -= (GetCurrentTime_MS() - start);
    }
    queue_.push_back(e);
    cv_has_element_.notify_all();
    return true;
  }
  T Take() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (queue_.empty()) {
      cv_has_element_.wait(lock);
    }
    T ret(queue_.front());
    queue_.pop_front();
    cv_has_space_.notify_all();
    return ret;
  }
  bool TryTake(T *e) {
    if (!e) {
      return false;
    }
    std::unique_lock<std::mutex> lock(mutex_);
    if (queue_.empty()) {
      return false;
    }
    *e = queue_.front();
    queue_.pop_front();
    cv_has_space_.notify_all();
    return true;
  }
  bool TimedTake(uint64_t timeout, T *e) {
    if (!e) {
      return false;
    }
    std::unique_lock<std::mutex> lock(mutex_);
    while (queue_.empty()) {
      long long start = GetCurrentTime_MS();
      if (cv_has_element_.wait_for(lock, std::chrono::milliseconds(timeout))
          == std::cv_status::timeout) {
        return false;
      }
      timeout -= (GetCurrentTime_MS() - start);
    }
    *e = queue_.front();
    queue_.pop_front();
    cv_has_space_.notify_all();
    return true;
  }
 private:
  bool PrivHasSpace() const {
    if (limit_ == NOLIMIT) {
      return true;
    }
    return queue_.size() < limit_;
  }

  const size_type limit_;
  mutable std::mutex mutex_;
  std::condition_variable cv_has_element_;
  std::condition_variable cv_has_space_;
  std::list<T> queue_;
};

} //namespace orbit

