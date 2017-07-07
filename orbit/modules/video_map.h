/*
 * Copyright 2016 (C) Orange Labs Inc. All Rights Reserved.
 * Author: caixinyu@orangelab.cn (Cai Xinyu)
 *
 * video_map.h
 * ---------------------------------------------------------------------------
 * Define classes which manage the video source in a video conference.
 * ---------------------------------------------------------------------------
 */

#pragma once

#include <stdint.h>
#include <map>
#include <vector>
#include <mutex>

namespace orbit {

/*
 * In a VideoMap, there is a only master.
 * Everyone always try to show master's video.
 */
class VideoMap final {
 public:
  typedef intmax_t id_t;
  static const id_t NOID = -1;
  /*
   * add a video source.
   */
  void Add(id_t id);

  /*
   * remove a video source.
   */
  void Remove(id_t id);

  /*
   * get all video source ids managed by this VideoMap.
   */
  void GetAllId(std::vector<id_t> *vec) const;

  /*
   * get the video source id to be shown for a people.
   */
  id_t GetSrc(id_t id) const;

  /*
   * get current master.
   */
  id_t GetMaster() const;

  /*
   * set master.
   */
  void SetMaster(id_t id);

  /*
   * tell the manager the video source is prepared.
   */
  void SetPrepared(id_t id);

  /*
   * get expected video source for one people.
   */
  id_t GetExpectedSrc(id_t id) const;

  void SetPrepareCallback(std::function<void(id_t)> callback) {
    Lock lock(mutex_);
    prepare_callback_ = callback;
  }
  void AddEnabledListener(std::function<void(id_t)> listener) {
    Lock lock(mutex_);
    enabled_listeners_.push_back(listener);
  }
  void AddDisabledListener(std::function<void(id_t)> listener) {
    Lock lock(mutex_);
    disabled_listeners_.push_back(listener);
  }
  void AddHasLookerListener(std::function<void(id_t)> listener) {
    Lock lock(mutex_);
    has_looker_listeners_.push_back(listener);
  }
  void AddNoLookerListener(std::function<void(id_t)> listener) {
    Lock lock(mutex_);
    no_looker_listeners_.push_back(listener);
  }
  void AddSrcChangedListener(std::function<void(id_t)> listener) {
    Lock lock(mutex_);
    src_changed_listeners_.push_back(listener);
  }
 private:
  class Node;
  class EventDetector;
  class EnabledDetector;
  class DisabledDetector;
  class HasLookerDetector;
  class NoLookerDetector;

  typedef std::map<id_t, Node*>::size_type count_t;
  typedef std::recursive_mutex Mutex;
  typedef std::lock_guard<Mutex> Lock;

  bool Contains(id_t id) const {
    return all_node_.find(id) != all_node_.end();
  }
  id_t SelectNewSrc(id_t myid) const;
  id_t SelectNewMaster() const;
  void Prepare(id_t id) {
    prepare_callback_(id);
  }
  void FireEnabledEvent(id_t id) {
    for (auto listener : enabled_listeners_) {
      listener(id);
    }
  }
  void FireDisabledEvent(id_t id) {
    for (auto listener : disabled_listeners_) {
      listener(id);
    }
  }
  void FireHasLookerEvent(id_t id) {
    for (auto listener : has_looker_listeners_) {
      listener(id);
    }
  }
  void FireNoLookerEvent(id_t id) {
    for (auto listener : no_looker_listeners_) {
      listener(id);
    }
  }
  void FireSrcChangedEvent(id_t stream) {
    for (auto listener : src_changed_listeners_) {
      listener(stream);
    }
  }

  mutable Mutex mutex_;
  std::function<void(id_t)> prepare_callback_;
  std::vector<std::function<void(id_t)>> enabled_listeners_;
  std::vector<std::function<void(id_t)>> disabled_listeners_;
  std::vector<std::function<void(id_t)>> has_looker_listeners_;
  std::vector<std::function<void(id_t)>> no_looker_listeners_;
  std::vector<std::function<void(id_t)>> src_changed_listeners_;
  std::map<id_t, Node *> all_node_;
  id_t master_ = NOID;
};

} // namespace orbit
