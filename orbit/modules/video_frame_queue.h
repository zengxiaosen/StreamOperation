
#pragma once

#include <stdint.h>

#include <mutex>
#include <map>
#include <memory>

namespace orbit {

class VideoFrame final {
 public:
  VideoFrame(const uint8_t *data, size_t len);
  uint16_t seq() const { return seq_; }
  uint32_t ts() const { return ts_; }
  uint8_t  marker() const { return marker_; }
  const uint8_t *data() const { return data_; }
  size_t len() const { return len_; }
  bool is_keyframe() const { return is_keyframe_; }
 private:
  static bool IsKeyFrame(const uint8_t *data, size_t len);

  uint16_t seq_;
  uint32_t ts_;
  uint8_t  marker_;
  uint8_t  data_[1500];
  size_t   len_;
  bool     is_keyframe_;
};

class VideoFrameQueue final {
 public:
  typedef intmax_t id_t;
  VideoFrameQueue(id_t id, std::function<void(id_t)> prepared_listener)
   : id_(id), prepared_listener_(prepared_listener) {}

  void PushPacket(const uint8_t *data, size_t len);
  std::shared_ptr<VideoFrame> PopPacket();
  std::shared_ptr<VideoFrame> TopPacket() const;

  void Enable();
  void Disable();
  bool Enabled() const;
  void SetHasLooker(bool has_looker);
  bool HasLooker() const;
 private:
  typedef std::recursive_mutex Mutex;
  typedef std::lock_guard<Mutex> Lock;

  bool Contains(uint16_t seq) const;
  void FirePreparedEvent();
  bool FirstIsKeyFrame() const;
  void CheckPrepared();

  void PushPacketWhenHasLooker(std::shared_ptr<VideoFrame> frame);
  void PushPacketWhenNoLooker(std::shared_ptr<VideoFrame> frame);

  mutable Mutex mutex_;
  const id_t id_;

  std::function<void(id_t)> prepared_listener_;
  std::map<uint16_t, std::shared_ptr<VideoFrame>> frames_;

  bool enabled_ = false;
  bool has_looker_ = false;
};

} // namespace orbit
