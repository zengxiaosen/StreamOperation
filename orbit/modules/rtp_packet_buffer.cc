/*
 * Copyright 2016 (C) Orange Labs Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * rtp_packet_buffer.cc
 * ---------------------------------------------------------------------------
 * Implementation of rtp_packet_buffer
 * ---------------------------------------------------------------------------
 */

#include "rtp_packet_buffer.h"

#include "gflags/gflags.h"

#include "stream_service/orbit/webrtc/webrtc_format_util.h"

namespace orbit {
  void RtpPacketBuffer::PushPacket(const char *data, int length) {
    std::lock_guard<std::mutex> guard(queue_mutex_);
    std::shared_ptr<dataPacket> pkt(new dataPacket());
    memcpy(pkt->data, (const char *)data, length);
    pkt->length = length;
    queue_.push(pkt);
  }

  void RtpPacketBuffer::PushPacket(const dataPacket& packet) {
    std::lock_guard<std::mutex> guard(queue_mutex_);
    std::shared_ptr<dataPacket> pkt(new dataPacket());
    memcpy(pkt->data, (const char *)(&(packet.data[0])), packet.length);
    pkt->length = packet.length;

    pkt->remote_ntp_time_ms = packet.remote_ntp_time_ms;
    pkt->arrival_time_ms = packet.arrival_time_ms;
    pkt->rtp_timestamp = packet.rtp_timestamp;
    queue_.push(pkt);
  }

  std::shared_ptr<dataPacket> RtpPacketBuffer::PopPacket() {
    std::lock_guard<std::mutex> guard(queue_mutex_);
    if (queue_.size() > prebuffering_size_) {
      std::shared_ptr<orbit::dataPacket> rtp_packet = queue_.top();
      queue_.pop();
      return rtp_packet;
    }
    return NULL;    
  }

  std::shared_ptr<dataPacket> RtpPacketBuffer::TopPacket() {
    std::lock_guard<std::mutex> guard(queue_mutex_);
    if (queue_.size() > prebuffering_size_) {
      std::shared_ptr<orbit::dataPacket> rtp_packet = queue_.top();
      return rtp_packet;
    }
    return NULL;    
  }

  int RtpPacketBuffer::GetSize() {  // total size of all items in the queue
    std::lock_guard<std::mutex> guard(queue_mutex_);
    return queue_.size();
  }

  bool RtpPacketBuffer::IsEmpty() { //Whether or not has data;
    return GetSize() == 0;
  }

  bool RtpPacketBuffer::PrebufferingDone() {
    return GetSize() > prebuffering_size_;
  }

  void VideoAwareFrameBuffer::PushPacket(const char *data, int length) {
    std::lock_guard<std::mutex> lock(mutex_);
    RtpHeader* h1 = (RtpHeader*)(data);
    int cur_seq = h1->getSeqNumber();

    if (buffer_type_ == NORMAL_BUFFER) {
      CleanBuffer();
      return;
    } 
    if (buffer_type_ == STORAGE_BUFFER) {
      // data is a frame
      if (IsKeyFramePacket(reinterpret_cast<const unsigned char*>(data), length)) {
        // If the packet is a key frame.
        CleanBuffer();
        key_frame_seq_ = cur_seq;
        has_key_frame_ = true;
        RtpPacketBuffer::PushPacket(data, length);
      } else {
        // Not a key frame, just insert into the queue.
        if (has_key_frame_) {
          // assert the first data in the queue is KeyFrame, and the inserted packet
          // should not be inserted before the KeyFrame.
          if (cur_seq > key_frame_seq_) {
            RtpPacketBuffer::PushPacket(data, length);
          }
        } else {
          // No Op.
          return;
        }
      }
    }
    if (buffer_type_ == PLAY_BUFFER) {
      RtpPacketBuffer::PushPacket(data, length);
    }
  }

  std::shared_ptr<dataPacket> VideoAwareFrameBuffer::PopPacket() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (buffer_type_ == PLAY_BUFFER) {
      return RtpPacketBuffer::PopPacket();
    }
    LOG(ERROR) << "In STORAGE_BUFFER and NORMAL_BUFFER, PopPacket() is not"
               << "supposed to call. BufferType=" << GetBufferTypeAsString(buffer_type_);
    return NULL;
  }

  bool VideoAwareFrameBuffer::IsKeyFramePacketStatic(const unsigned char* buf, int length) {
    webrtc::RtpDepacketizer::ParsedPayload payload;
    const RtpHeader* h = reinterpret_cast<const RtpHeader*>(buf);
    int video_payload = (int)(h->getPayloadType());

    //bool ret = webrtc::RtpDepacketizerVp8_Parse(&payload, buf+12, length-12);
    bool ret = RtpDepacketizer_Parse(video_payload, &payload, buf + 12, length - 12);
    if (!ret) {
      LOG(ERROR) << "Parse the payload failed.";
      return false;
    } else {
     //PrintParsedPayload(payload);
      if (payload.frame_type == webrtc::kVideoFrameKey) {
        return true;
      }
    }
    return false;
  }
}  // namespace orbit
