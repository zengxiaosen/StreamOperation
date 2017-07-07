/*
 * video_frame_queue.cc
 *
 *  Created on: 2016-9-20
 *      Author: cxy
 */

#include "video_frame_queue.h"

#include "stream_service/orbit/rtp/rtp_headers.h"
#include "stream_service/orbit/webrtc/webrtc_format_util.h"

#include "glog/logging.h"

namespace orbit {

VideoFrame::VideoFrame(const uint8_t *data, size_t len) {
  memcpy(data_, data, len);
  len_ = len;
  is_keyframe_ = IsKeyFrame(data, len);

  const RtpHeader *h = reinterpret_cast<const RtpHeader*>(data);
  seq_ = h->getSeqNumber();
  ts_ = h->getTimestamp();
  marker_ = h->getMarker();
}

bool VideoFrame::IsKeyFrame(const uint8_t *data, size_t len) {
  webrtc::RtpDepacketizer::ParsedPayload payload;
  const RtpHeader *h = reinterpret_cast<const RtpHeader*>(data);
  int video_payload = (int)(h->getPayloadType());
  bool ok = RtpDepacketizer_Parse(video_payload, &payload, data + 12, len - 12);
  return ok && payload.frame_type == webrtc::kVideoFrameKey;
}

void VideoFrameQueue::PushPacket(const uint8_t *data, size_t len) {
  if (!data || len <= 0) {
    return;
  }
  Lock lock(mutex_);
  if (!enabled_) {
    return;
  }
  auto frame = std::make_shared<VideoFrame>(data, len);
  if (has_looker_) {
    PushPacketWhenHasLooker(frame);
  } else {
    PushPacketWhenNoLooker(frame);
  }
}

void VideoFrameQueue::PushPacketWhenHasLooker(std::shared_ptr<VideoFrame> frame) {
  uint16_t seq = frame->seq();
  if (Contains(seq)) {
    return;
  }
  frames_[seq] = frame;
  CheckPrepared();
}

void VideoFrameQueue::PushPacketWhenNoLooker(std::shared_ptr<VideoFrame> frame) {
  uint16_t seq = frame->seq();
  if (Contains(seq)) {
    return;
  }
  if (frame->is_keyframe()) {
    frames_.clear();
    frames_[seq] = frame;
    prepared_listener_(id_);
  } else {
    if (FirstIsKeyFrame()) {
      frames_[seq] = frame;
    }
  }
}

std::shared_ptr<VideoFrame> VideoFrameQueue::PopPacket() {
  Lock lock(mutex_);
  if (!enabled_) {
    return nullptr;
  }
  auto iter = frames_.begin();
  if (iter == frames_.end()) {
    return nullptr;
  }
  std::shared_ptr<VideoFrame> frame = iter->second;
  frames_.erase(iter);
  CheckPrepared();
  return frame;
}

std::shared_ptr<VideoFrame> VideoFrameQueue::TopPacket() const {
  Lock lock(mutex_);
  if (!enabled_) {
    return nullptr;
  }
  auto iter = frames_.begin();
  if (iter == frames_.end()) {
    return nullptr;
  }
  std::shared_ptr<VideoFrame> frame = iter->second;
  return frame;
}

void VideoFrameQueue::Enable() {
  Lock lock(mutex_);
  enabled_ = true;
}

void VideoFrameQueue::Disable() {
  Lock lock(mutex_);
  if (enabled_) {
    enabled_ = false;
    frames_.clear();
  }
}

bool VideoFrameQueue::Enabled() const {
  Lock lock(mutex_);
  return enabled_;
}

void VideoFrameQueue::SetHasLooker(bool has_looker) {
  Lock lock(mutex_);
  has_looker_ = has_looker;
}

bool VideoFrameQueue::HasLooker() const {
  Lock lock(mutex_);
  return has_looker_;
}

bool VideoFrameQueue::FirstIsKeyFrame() const {
  auto iter = frames_.cbegin();
  if (iter == frames_.cend()) {
    return false;
  }
  std::shared_ptr<VideoFrame> frame = iter->second;
  if (frame) {
    return frame->is_keyframe();
  } else {
    return false;
  }
}

bool VideoFrameQueue::Contains(uint16_t seq) const {
  return frames_.find(seq) != frames_.end();
}

void VideoFrameQueue::FirePreparedEvent() {
  if (prepared_listener_) {
    prepared_listener_(id_);
  }
}

void VideoFrameQueue::CheckPrepared() {
  if (FirstIsKeyFrame()) {
    FirePreparedEvent();
  }
}

} // namespace orbit
