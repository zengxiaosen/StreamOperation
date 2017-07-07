/*
 * Copyright 2016 (C) Orange Labs Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * rtp_packet_buffer_test.cc
 */

#include "gtest/gtest.h"
#include "gtest/gtest_prod.h"
#include "glog/logging.h"

#include "rtp_packet_buffer.h"

#define MAKE_PACKET(seq, payload) char* data = (char*) malloc(50); \
  RtpHeader* h = (RtpHeader*)data; \
  h->setSeqNumber(seq); \
  const char* str = payload; \
  memcpy(data + 12, str, strlen(str)); \
  int len = 50; \

namespace orbit {
// This is a Fake VideoAwareFrameBuffer - it did not check the key frame 
// of the inserted packet, while it use a Hack way to determine for testing
// purpose.
class FakeVideoAwareFrameBuffer : public VideoAwareFrameBuffer {
public:
  FakeVideoAwareFrameBuffer(int prebuffering_size) :
    VideoAwareFrameBuffer(prebuffering_size) {
  }
  // If the payload of the packet is "ok" and then it is considered as KF.
  virtual bool IsKeyFramePacket(const unsigned char* buf, int length) override {
    unsigned char t1 = *(buf + 12);
    unsigned char t2 = *(buf + 13);
    if (t1 == 'o' && t2 == 'k') {
      return true;
    }
    return false;
  }
};

class RtpPacketBufferTest : public testing::Test {
 protected:
  virtual void SetUp() override {
  }
  virtual void TearDown() override {
  }
};

TEST_F(RtpPacketBufferTest, NormalBufferTest) {
  FakeVideoAwareFrameBuffer buffer(0);
  EXPECT_EQ(VideoAwareFrameBuffer::NORMAL_BUFFER, buffer.GetBufferType());

  MAKE_PACKET(1, "1234");
  // Normal buffer does not push any packet.
  buffer.PushPacket(data, len);
  EXPECT_EQ(0, buffer.GetSize());
  std::shared_ptr<dataPacket> packet = buffer.PopPacket();
  // Normal buffer does not pop any packet.
  EXPECT_EQ(NULL, packet.get());
}

TEST_F(RtpPacketBufferTest, StorageBufferTest) {
  FakeVideoAwareFrameBuffer buffer(0);
  EXPECT_EQ(VideoAwareFrameBuffer::NORMAL_BUFFER, buffer.GetBufferType());

  MAKE_PACKET(1, "1234");
  // Normal buffer does not push any packet.
  buffer.PushPacket(data, len);
  EXPECT_EQ(0, buffer.GetSize());

  {
    // Change to Storage buffer
    buffer.SetBufferType(VideoAwareFrameBuffer::STORAGE_BUFFER);
    buffer.PushPacket(data, len);
    // This is not a keyframe, so drop it.
    EXPECT_EQ(0, buffer.GetSize());
    EXPECT_EQ(false, buffer.has_key_frame_);
    EXPECT_EQ(0, buffer.key_frame_seq_);
  }

  {
    // Make a key frame packet.
    MAKE_PACKET(10, "ok");

    // Storage buffer accept the first keyframe
    buffer.PushPacket(data, len);
    EXPECT_EQ(1, buffer.GetSize());
    EXPECT_EQ(true, buffer.has_key_frame_);
    EXPECT_EQ(10, buffer.key_frame_seq_);
  }

  {
    MAKE_PACKET(11, "123456");

    // The buffer has the key frame now. thus it will accept the packet 11
    buffer.PushPacket(data, len);
    EXPECT_EQ(2, buffer.GetSize());
    EXPECT_EQ(10, buffer.key_frame_seq_);
  }

  {
    MAKE_PACKET(9, "abcdefg");

    // Before insering into the buffer, the buffer has two elements, 10 and 11.
    EXPECT_EQ(2, buffer.GetSize());
    // The buffer has the keyframe but the packet's seqnumber 9 is less than
    // keyframe_seq_. So drop it. Thus the size of the buffer still has two
    // elements
    buffer.PushPacket(data, len);
    EXPECT_EQ(2, buffer.GetSize());
    EXPECT_EQ(10, buffer.key_frame_seq_);
  }

  {
    MAKE_PACKET(12, "ef12345");

    // The buffer will accept the packet 12
    buffer.PushPacket(data, len);
    EXPECT_EQ(3, buffer.GetSize());
    EXPECT_EQ(10, buffer.key_frame_seq_);
  }
}

TEST_F(RtpPacketBufferTest, PlayBufferTest) {
  FakeVideoAwareFrameBuffer buffer(0);
  // Change to Storage buffer
  buffer.SetBufferType(VideoAwareFrameBuffer::STORAGE_BUFFER);
  EXPECT_EQ(VideoAwareFrameBuffer::STORAGE_BUFFER, buffer.GetBufferType());
  // This test demostrates the Buffer status change: from STORAGE_BUFFER to
  // PLAY_BUFFER. Right now it will be STORAGE_BUFFER.
  {
    MAKE_PACKET(1, "12345");

    EXPECT_EQ(0, buffer.GetSize());
    buffer.PushPacket(data, len);
    // This is not a keyframe, so drop it.
    EXPECT_EQ(0, buffer.GetSize());
    EXPECT_EQ(false, buffer.has_key_frame_);
    EXPECT_EQ(0, buffer.key_frame_seq_);
  }

  {
    // Make a key frame packet.
    MAKE_PACKET(10, "ok");

    // Storage buffer accept the first keyframe
    buffer.PushPacket(data, len);
    EXPECT_EQ(1, buffer.GetSize());
    EXPECT_EQ(true, buffer.has_key_frame_);
    EXPECT_EQ(10, buffer.key_frame_seq_);
  }

  {
    // Make another normal packet.
    MAKE_PACKET(11, "eftggg");
    // The buffer accepts packet 11.
    buffer.PushPacket(data, len);
    EXPECT_EQ(2, buffer.GetSize());
    EXPECT_EQ(10, buffer.key_frame_seq_);
  }

  {
    // Make another normal packet.
    MAKE_PACKET(13, "eftggg");

    EXPECT_EQ(2, buffer.GetSize());
    // Push another frame, seq = 13
    buffer.PushPacket(data, len);
    EXPECT_EQ(3, buffer.GetSize());
    EXPECT_EQ(10, buffer.key_frame_seq_);
  }

  // Change this from a Storage buffer to a PLAY_BUFFER.
  buffer.SetBufferType(VideoAwareFrameBuffer::PLAY_BUFFER);
  EXPECT_EQ(VideoAwareFrameBuffer::PLAY_BUFFER, buffer.GetBufferType());

  {
    std::shared_ptr<dataPacket> packet = buffer.PopPacket();
    // Pop the first keyframe. the packet is not NULL.
    EXPECT_TRUE(NULL != packet.get());
    RtpHeader* h2 = reinterpret_cast<RtpHeader*>(&(packet->data[0]));
    EXPECT_EQ(10, h2->getSeqNumber());
    EXPECT_EQ(2, buffer.GetSize());
  }

  {
    // Make another normal packet 12, and insert into the buffer.
    MAKE_PACKET(12, "abcdefg");

    EXPECT_EQ(2, buffer.GetSize());
    // Push the frame to Play buffer, seq = 12. Insert will be okay.
    buffer.PushPacket(data, len);
    EXPECT_EQ(3, buffer.GetSize());
  }

  {
    std::shared_ptr<dataPacket> packet = buffer.PopPacket();
    // Pop the second. the packet is not NULL. It is packet (seq=11)
    EXPECT_TRUE(NULL != packet.get());
    RtpHeader* h2 = reinterpret_cast<RtpHeader*>(&(packet->data[0]));
    EXPECT_EQ(11, h2->getSeqNumber());
    EXPECT_EQ(2, buffer.GetSize());
  }

  {
    std::shared_ptr<dataPacket> packet = buffer.PopPacket();
    // Pop the third frame. the packet is not NULL.It is packet (seq=12)
    EXPECT_TRUE(NULL != packet.get());
    RtpHeader* h2 = reinterpret_cast<RtpHeader*>(&(packet->data[0]));
    EXPECT_EQ(12, h2->getSeqNumber());
    EXPECT_EQ(1, buffer.GetSize());
  }

  {
    std::shared_ptr<dataPacket> packet = buffer.PopPacket();
    // Pop the forth frame. the packet is not NULL.It is packet (seq=13)
    EXPECT_TRUE(NULL != packet.get());
    RtpHeader* h2 = reinterpret_cast<RtpHeader*>(&(packet->data[0]));
    EXPECT_EQ(13, h2->getSeqNumber());
    EXPECT_EQ(0, buffer.GetSize());
  }
}

}  // namespace orbit
