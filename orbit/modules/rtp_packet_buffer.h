/*
 * Copyright 2016 (C) Orange Labs Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * rtp_packet_buffer.h
 * ---------------------------------------------------------------------------
 * Defines the interface for a Rtp-aware packet buffer - RtpPacketBuffer
 *  - The buffer is used to store the incoming (or outgoing) rtp packets as a
 *    queue. The queue has the intelligence to be aware of situations and
 *    conditions the rtp transportations, for examples:
 *     - A prebuffering_size is used to configure the prebuffering threshold. During
 *        prebuffering phase, the PopPacket() will not pop any packets from the queue.
 *     - The Queue use a priority_queue to store the inserted packets in the order
 *        of sequence number.
 *    Sample usage of this queue: 
 *     -  If use as a simple jittersbuffer, change the prebuffering_size to hold
 *        the received packets in the queue for a longer time, and it will be
 *        sorted when the packet is pop out. And also use to wait for the late
 *        packet.
 *    
 *  VideoAwareFrameBuffer is a specific version of RtpPacketBuffer
 *    The Buffer could be in 3 different types of special buffer, determined 
 *    byThe buffer_type variables:
 *  - NORMAL_BUFFER   ---> all the Push and Pop are Non-Op(just drop the packets)
 *  - STORAGE_BUFFER  -====> We want to the buffer to hold at least one key frame
 *                     |     And the KF(keyframe) should be the first element of the queue,
 *                     |     And only if there is KF in the queue, newer packets will be
 *                     |     inserted and sorted. Otherwise the packet will be dropped.
 *                     |===> Pop will be Non-Op.
 *  - PLAY_BUFFER   --->  Works just like a Rtp-aware packet buffer above.
 * ---------------------------------------------------------------------------
 */

#pragma once

#include <mutex>
#include <queue>
#include <memory>

#include "stream_service/orbit/media_definitions.h"
#include "stream_service/orbit/rtp/rtp_headers.h"
#include "glog/logging.h"

#include "gtest/gtest_prod.h"

namespace orbit {

class RtpPacketBuffer {
 public:
  RtpPacketBuffer(int prebuffering_size) {
    prebuffering_size_ = prebuffering_size;
  }
  virtual ~RtpPacketBuffer() {
  }

  // push and pop methods.
  virtual void PushPacket(const char *data, int length);
  virtual void PushPacket(const dataPacket& packet);

  virtual std::shared_ptr<dataPacket> PopPacket();
  virtual std::shared_ptr<dataPacket> TopPacket();

  virtual int GetSize();  // total size of all items in the queue
  virtual bool IsEmpty(); //Whether or not has data;
  virtual bool PrebufferingDone(); // Whether the data prebuffering is done.
  // Clean the buffer.
  virtual void CleanBuffer() {
    while(!IsEmpty()) {
      //PopPacket();
      queue_.pop();
    }
  }

  virtual void SetPrebufferingSize(int size) {
    prebuffering_size_ = size;
  }

 protected:
  // Implements x < y, taking into account RTP sequence number wrap
  // The general idea is if there's a very large difference between
  // x and y, that implies that the larger one is actually "less than"
  // the smaller one.
  //
  // I picked 0x8000 as my "very large" threshold because it splits
  // 0xffff, so it seems like a logical choice.
  class dataPacketComparator {
  public:
    bool operator() (std::shared_ptr<orbit::dataPacket> a,
                     std::shared_ptr<orbit::dataPacket> b) {
      RtpHeader* h1 = reinterpret_cast<RtpHeader*>(&(a->data[0]));
      RtpHeader* h2 = reinterpret_cast<RtpHeader*>(&(b->data[0]));
      uint16_t x = h1->getSeqNumber();
      uint16_t y = h2->getSeqNumber();
      int diff = x - y;
      if (diff > 0) {
        return (diff < 0x8000);
      } else if (diff < 0) {
        return (diff < -0x8000);
      } else { // diff == 0
        return false;
      }
    }
  };
  
  // A priority queue to make the inserted dataPacket sorted.
  std::priority_queue<std::shared_ptr<orbit::dataPacket>,
    std::vector<std::shared_ptr<orbit::dataPacket>>,
    dataPacketComparator> queue_;

  // A mutex to protect the access to priority queue above.
  std::mutex queue_mutex_;

  int prebuffering_size_ = 0;
};

class VideoAwareFrameBuffer : public RtpPacketBuffer {
 public:
  // Defines the buffer type for this buffer
  //  - Normal buffer - the buffer did not do anything than just a placeholder. No push, no pop.
  //  - Storage buffer- the buffer will keep only the keyframe as the first element, and all the other frames are kept afterwards
  //  - Play buffer  -  the buffer will do just the normal push and pop operation.
  enum BufferType {
    NORMAL_BUFFER = 0,
    STORAGE_BUFFER = 1,
    PLAY_BUFFER = 2
  };
  static std::string GetBufferTypeAsString(BufferType type) {
    switch(type) {
    case NORMAL_BUFFER: 
      return "NORMAL_BUFFER";
    case STORAGE_BUFFER: 
      return "STORAGE_BUFFER";
    case PLAY_BUFFER: 
      return "PLAY_BUFFER";
    }
    return "INVALID";
  }

  VideoAwareFrameBuffer(int prebuffering_size) : RtpPacketBuffer(prebuffering_size) {
    buffer_type_ = NORMAL_BUFFER;
  }
  virtual ~VideoAwareFrameBuffer() {
  }

  void SetBufferType(BufferType type) {
    std::lock_guard<std::mutex> lock(mutex_);
    // Change to another buffer type.
    buffer_type_ = type;
  }
  BufferType GetBufferType() {
    std::lock_guard<std::mutex> lock(mutex_);
    return buffer_type_;
  }

  void ResetBuffer() {
    std::lock_guard<std::mutex> lock(mutex_);
    has_key_frame_ = false;
    CleanBuffer();
  }

  // push and pop methods.
  virtual void PushPacket(const char *data, int length);
  virtual std::shared_ptr<dataPacket> PopPacket();

  static bool IsKeyFramePacketStatic(const unsigned char* buf, int length);

  virtual bool IsKeyFramePacket(const unsigned char* buf, int length) {
    return IsKeyFramePacketStatic(buf, length);
  }
  virtual bool IsKeyFramePacket(std::shared_ptr<dataPacket> rtp_packet) {
    return IsKeyFramePacket((const unsigned char*)&(rtp_packet->data[0]), rtp_packet->length);    
  }
 private:
  std::mutex mutex_;
  BufferType buffer_type_;
  bool has_key_frame_ = false;
  int key_frame_seq_ = 0;

  FRIEND_TEST(RtpPacketBufferTest, NormalBufferTest);
  FRIEND_TEST(RtpPacketBufferTest, StorageBufferTest);
  FRIEND_TEST(RtpPacketBufferTest, PlayBufferTest);
};

}  // namespace orbit
