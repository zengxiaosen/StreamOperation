/*
 * Copyright 2016 (C) Orange Labs Inc. All Rights Reserved.
 * Author: caixinyu@orangelab.cn (Cai Xinyu)
 *
 * video_map.cc
 * ---------------------------------------------------------------------------
 * Implements classes which manage the video source in a video conference.
 * ---------------------------------------------------------------------------
 */

#include "video_map.h"

#include <set>

#include "glog/logging.h"

namespace orbit {

template <typename T>
static inline bool MapContains(const std::map<VideoMap::id_t, T> &map, VideoMap::id_t id) {
  return map.find(id) != map.end();
}

/*
 * In MasterAwaredVideoMap, people's relationship
 * are saved by class Node. These Node make up a graph
 * by some pointers.
 */
class VideoMap::Node final {
 public:
  Node(id_t id):id_(id) {}
  id_t id() const { return id_; }
  Node *src() const { return src_; }
  void set_src(Node *src) {
    if (src) {
      if (src_) {
        src_->lookers_.erase(this);
      }
      src_ = src;
      src_->lookers_.insert(this);
    }
  }
  void GetAllCurrentLooker(std::vector<Node *> *vec) const {
    if (vec) {
      vec->clear();
      for (Node *looker : lookers_) {
        vec->push_back(looker);
      }
    }
  }
  void UnbindCurrentSrc() {
    if (src_) {
      src_->lookers_.erase(this);
      src_ = nullptr;
    }
  }
  void UnbindCurrentLooker() {
    for (Node *looker : lookers_) {
      looker->src_ = nullptr;
    }
    lookers_.clear();
  }
  void UnbindCurrent() {
    UnbindCurrentSrc();
    UnbindCurrentLooker();
  }

  Node *expected_src() const { return expected_src_; }
  void set_expected_src(Node *expected_src) {
    if (!expected_src) {
      return;
    }
    if (expected_src_) {
      expected_src_->expected_lookers_.erase(this);
    }
    expected_src_ = expected_src;
    expected_src_->expected_lookers_.insert(this);
  }
  void GetAllExpectedLooker(std::vector<Node *> *vec) const {
    if (vec) {
      vec->clear();
      for (Node *expected_looker : expected_lookers_) {
        vec->push_back(expected_looker);
      }
    }
  }
  void UnbindExpectedSrc() {
    if (expected_src_) {
      expected_src_->expected_lookers_.erase(this);
      expected_src_ = nullptr;
    }
  }
  void UnbindExpectedLooker() {
    for (Node *expected_looker : expected_lookers_) {
      expected_looker->expected_src_ = nullptr;
    }
    expected_lookers_.clear();
  }
  void UnbindExpected() {
    UnbindExpectedSrc();
    UnbindExpectedLooker();
  }
  void Unbind() {
    UnbindCurrent();
    UnbindExpected();
  }
  int GetLookerCount() const {
    return lookers_.size() + expected_lookers_.size();
  }
  bool HasCurLooker() const {
    return !lookers_.empty();
  }
 private:
  const id_t id_;
  Node *src_ = nullptr;
  Node *expected_src_ = nullptr;
  std::set<Node *> lookers_;
  std::set<Node *> expected_lookers_;
}; // class MasterAwaredVideoMap::Node


class VideoMap::EnabledDetector final {
 public:
  EnabledDetector(VideoMap *video_map)
   : video_map_(video_map) {

    for (auto &pair : video_map->all_node_) {
      id_t id = pair.first;
      const Node *node = pair.second;
      old_look_count_[id] = node->GetLookerCount();
    }
  }
  void Detect() {
    const std::map<id_t, Node *> &all_node = video_map_->all_node_;
    for (auto &pair : all_node) {
      id_t id = pair.first;
      Node *node = pair.second;
      int look_count = node->GetLookerCount();
      if (look_count > 0) {
        if (!MapContains(old_look_count_, id)) {
          video_map_->FireEnabledEvent(id);
          continue;
        }
        int old_count = old_look_count_[id];
        if (old_count == 0) {
          video_map_->FireEnabledEvent(id);
        }
      }
    }
  }
 private:
  typedef VideoMap::id_t id_t;
  VideoMap * const video_map_;
  std::map<id_t, int> old_look_count_;
};

class VideoMap::DisabledDetector final {
 public:
  DisabledDetector(VideoMap *video_map)
   : video_map_(video_map) {

    for (auto &pair : video_map->all_node_) {
      id_t id = pair.first;
      const Node *node = pair.second;
      old_look_count_[id] = node->GetLookerCount();
    }
  }
  void Detect() {
    const std::map<id_t, Node *> &all_node = video_map_->all_node_;
    for (auto &pair : old_look_count_) {
      id_t id = pair.first;
      count_t old_count = pair.second;
      if (old_count == 0) {
        continue;
      }

      auto iter = all_node.find(id);
      if (iter == all_node.end()) {
        video_map_->FireDisabledEvent(id);
        continue;
      }
      const Node *node = iter->second;
      if (!node) {
        video_map_->FireDisabledEvent(id);
        continue;
      }
      if (node->GetLookerCount() == 0) {
        video_map_->FireDisabledEvent(id);
      }
    }
  }
 private:
  typedef VideoMap::id_t id_t;

  VideoMap * const video_map_;
  std::map<id_t, int> old_look_count_;
};

class VideoMap::HasLookerDetector final {
 public:
  HasLookerDetector(VideoMap *video_map)
   : video_map_(video_map) {

    for (auto &pair : video_map->all_node_) {
      id_t id = pair.first;
      const Node *node = pair.second;
      old_has_cur_looker_[id] = node->HasCurLooker();
    }
  }
  void Detect() {
    const std::map<id_t, Node *> &all_node = video_map_->all_node_;
    for (auto &pair : all_node) {
      id_t id = pair.first;
      const Node *node = pair.second;
      if (node->HasCurLooker()) {
        if (!MapContains(old_has_cur_looker_, id)) {
          video_map_->FireHasLookerEvent(id);
          continue;
        }
        bool old_has_cur = old_has_cur_looker_[id];
        if (!old_has_cur) {
          video_map_->FireHasLookerEvent(id);
        }
      }
    }
  }
 private:
  typedef VideoMap::id_t id_t;
  VideoMap * const video_map_;
  std::map<id_t, bool> old_has_cur_looker_;
};

class VideoMap::NoLookerDetector final {
 public:
  NoLookerDetector(VideoMap *video_map)
   : video_map_(video_map) {

    for (auto &pair : video_map_->all_node_) {
      id_t id = pair.first;
      const Node *node = pair.second;
      old_has_cur_looker_[id] = node->HasCurLooker();
    }
  }
  void Detect() {
    for (auto &pair : old_has_cur_looker_) {
      id_t id = pair.first;
      bool old_has = pair.second;
      if (old_has) {
        if (!MapContains(video_map_->all_node_, id)) {
          continue;
        }
        const Node *node = video_map_->all_node_[id];
        if (!node->HasCurLooker()) {
          video_map_->FireNoLookerEvent(id);
        }
      }
    }
  }
 private:
  typedef VideoMap::id_t id_t;
  VideoMap * const video_map_;
  std::map<id_t, bool> old_has_cur_looker_;
};

class VideoMap::EventDetector final {
 public:
  EventDetector(VideoMap *video_map)
   : enabled_detector_(video_map),
     disabled_detector_(video_map),
     has_looker_detector_(video_map),
     no_looker_detector_(video_map) {}

  void Detect() {
    enabled_detector_.Detect();
    disabled_detector_.Detect();
    has_looker_detector_.Detect();
    no_looker_detector_.Detect();
  }

 private:
  EnabledDetector enabled_detector_;
  DisabledDetector disabled_detector_;
  HasLookerDetector has_looker_detector_;
  NoLookerDetector no_looker_detector_;
};

void VideoMap::Add(id_t id) {
  Lock lock(mutex_);
  if (Contains(id)) {
    return;
  }

  EventDetector event_detector(this);

  Node *new_node = new Node(id);
  all_node_[id] = new_node;
  const count_t count = all_node_.size();

  // count == 0 never happen.

  // only one people,
  // set him as master,
  // he see himself.
  if (count == 1) {
    master_ = id;
    Node *master_node = all_node_[master_];
    new_node->set_expected_src(master_node);
    Prepare(master_);
    event_detector.Detect();
    return;
  }

  // two people,
  // they see another one.
  if (count == 2) {
    Node *master_node = all_node_[master_];
    master_node->set_expected_src(new_node);
    Prepare(id);
    new_node->set_expected_src(master_node);
    Prepare(master_);
    event_detector.Detect();
    return;
  }

  // count >= 3
  // three or more people, they see master.
  Node *master_node = all_node_[master_];
  new_node->set_expected_src(master_node);
  Prepare(master_);
  event_detector.Detect();
}

void VideoMap::Remove(id_t id) {
  Lock lock(mutex_);
  if (!Contains(id)) {
    return;
  }

  EventDetector event_detector(this);

  Node *removed_node = all_node_[id];
  all_node_.erase(id);

  // no one left
  if (all_node_.empty()) {
    master_ = NOID;
    delete removed_node;
    event_detector.Detect();
    return;
  }

  // -----------------------------
  // we are removing master.
  // -----------------------------
  if (id == master_) {

    // remove this node from graph.
    removed_node->Unbind();

    // select a new master.
    id_t new_master = SelectNewMaster();
    master_ = new_master;
    Node *master_node = all_node_[master_];

    // select a source for new selected master.
    id_t new_master_src = SelectNewSrc(master_);
    Node *new_master_src_node = all_node_[new_master_src];
    master_node->set_expected_src(new_master_src_node);

    // tell new source of new master to prepare.
    Prepare(new_master_src);

    // make other people prepare to see new master.
    for (auto &pair : all_node_) {
      Node *n = pair.second;
      if (n->id() != master_) {
        n->set_expected_src(master_node);
      }
    }

    // tell new master to prepare.
    Prepare(master_);

  } else {

    // ----------------------------------
    // we are removing a non-master one.
    // ----------------------------------

    // ids we should be told to prepare.
    std::set<id_t> prep_ids;

    std::vector<Node *> lookers, expected_lookers;
    removed_node->GetAllCurrentLooker(&lookers);
    removed_node->GetAllExpectedLooker(&expected_lookers);

    // choose new source for people who are looking this people
    // because it will be removed.
    for (Node *looker : lookers) {
      id_t id = looker->id();
      id_t new_src = SelectNewSrc(id);
      Node *new_src_node = all_node_[new_src];
      looker->set_expected_src(new_src_node);
      prep_ids.insert(new_src);
    }

    // choose new source for people who are expected to see
    // this people.
    for (Node *expected_looker : expected_lookers) {
      id_t id = expected_looker->id();
      id_t new_src = SelectNewSrc(id);
      Node *new_src_node = all_node_[new_src];
      expected_looker->set_expected_src(new_src_node);
      prep_ids.insert(new_src);
    }

    // remove this node from graph.
    removed_node->Unbind();

    // tell video sources to prepare if needed.
    for (id_t id : prep_ids) {
      Prepare(id);
    }

  }

  delete removed_node;
  event_detector.Detect();
}

void VideoMap::GetAllId(std::vector<id_t> *vec) const {
  if (vec) {
    vec->clear();
    Lock lock(mutex_);
    for (auto &pair : all_node_) {
      vec->push_back(pair.first);
    }
  }
}

VideoMap::id_t VideoMap::GetSrc(id_t id) const {
  Lock lock(mutex_);
  if (!Contains(id)) {
    return NOID;
  }
  const Node *node = all_node_.find(id)->second;
  if (!node) {
    return NOID;
  }
  const Node *src = node->src();
  if (!src) {
    return NOID;
  }
  return src->id();
}

VideoMap::id_t VideoMap::GetMaster() const {
  Lock lock(mutex_);
  return master_;
}

void VideoMap::SetMaster(id_t id) {
  Lock lock(mutex_);
  if (!Contains(id)) {
    return;
  }
  if (id == master_) {
    return;
  }

  EventDetector event_detector(this);

  master_ = id;
  Node *master_node = all_node_[master_];

  bool need_prepare = false;
  for (auto &pair : all_node_) {
    Node *node = pair.second;
    if (node != master_node) {
      need_prepare = true;
      node->set_expected_src(master_node);
    }
  }

  if (need_prepare) {
    Prepare(master_);
  }

  event_detector.Detect();
}

void VideoMap::SetPrepared(id_t id) {
  Lock lock(mutex_);
  if (!Contains(id)) {
    return;
  }

  EventDetector event_detector(this);

  Node *prepared_node = all_node_[id];
  for (auto &pair : all_node_) {
    Node *node = pair.second;
    Node *current_src = node->src();
    Node *expected_src = node->expected_src();
    if (node->expected_src() == prepared_node) {
      node->UnbindExpectedSrc();
      node->set_src(prepared_node);
      if (current_src != expected_src) {
        FireSrcChangedEvent(node->id());
      }
    }
  }

  event_detector.Detect();
}

VideoMap::id_t VideoMap::GetExpectedSrc(id_t id) const {
  Lock lock(mutex_);
  if (!Contains(id)) {
    return NOID;
  }
  auto iter = all_node_.find(id);
  const Node *node = iter->second;
  if (!node) {
    return NOID;
  }
  const Node *expected_src_node = node->expected_src();
  if (!expected_src_node) {
    return NOID;
  }
  return expected_src_node->id();
}

VideoMap::id_t VideoMap::SelectNewSrc(id_t myid) const {
  const count_t count = all_node_.size();
  if (count == 0) {
    return NOID;
  }
  if (myid != master_) {
    return master_;
  }
  if (count == 1) {
    return myid;
  }
  for (auto &pair : all_node_) {
    const Node *node = pair.second;
    id_t id = node->id();
    if (id != myid) {
      return id;
    }
  }

  // never reach here !
  LOG(FATAL) << "Logic Error !";
  return NOID;
}

VideoMap::id_t VideoMap::SelectNewMaster() const {
  const count_t count = all_node_.size();
  if (count == 0) {
    return NOID;
  }
  return all_node_.begin()->first;
}

} // namespace orbit

