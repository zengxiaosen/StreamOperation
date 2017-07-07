/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * rtp_packet_queue_test.cc
 */

#include "gtest/gtest.h"

#include "stream_service/erizo/rtp/RtpPacketQueue.h"
#include "stream_service/erizo/rtp/RtpHeaders.h"
#include "stream_service/erizo/MediaDefinitions.h"

namespace erizo {
namespace {

class RtpPacketQueueTest : public testing::Test {
 protected:
  virtual void SetUp() override {
  }

  virtual void TearDown() override {
  }
};

TEST_F(RtpPacketQueueTest, rtpPacketQueueDefaults) {
  erizo::RtpPacketQueue queue;
  EXPECT_EQ(0, queue.getSize());
  EXPECT_EQ(false, queue.hasData());
}

TEST_F(RtpPacketQueueTest, rtpPacketQueueBasicBehavior) {
  erizo::RtpPacketQueue queue;

  erizo::RtpHeader header;
  header.setSeqNumber(12);
  queue.pushPacket((const char *)&header, sizeof(erizo::RtpHeader));

  // We should have one packet, and we're not ready to pop any data.
  EXPECT_EQ(1, queue.getSize());
  EXPECT_EQ(false, queue.hasData());

  // If we try to pop this packet, we should get back a null shared_ptr object.
  boost::shared_ptr<erizo::dataPacket> packet = queue.popPacket();
  ASSERT_TRUE(packet == 0);
  EXPECT_EQ(1, queue.getSize());
  EXPECT_EQ(false, queue.hasData());

  // If we use the override in popPacket, we should get back header.
  packet = queue.popPacket(true);
  const erizo::RtpHeader *poppedHeader = reinterpret_cast<const erizo::RtpHeader*>(packet->data);
  EXPECT_EQ(12, poppedHeader->getSeqNumber());

  // Validate our size and that we have no data to offer.
  EXPECT_EQ(0, queue.getSize());
  EXPECT_EQ(false, queue.hasData());
}

TEST_F(RtpPacketQueueTest, rtpPacketQueueCorrectlyReordersSamples) {
  erizo::RtpPacketQueue queue;
  // Add sequence numbers 10 9 8 7 6 5 4 3 2 1.
  for(int x = 10; x >0; x--) {
    erizo::RtpHeader header;
    header.setSeqNumber(x);
    queue.pushPacket((const char *)&header, sizeof(erizo::RtpHeader));
  }

  for(int x = 1; x <=10; x++) {
    boost::shared_ptr<erizo::dataPacket> packet = queue.popPacket(true); // override our default pop behavior so we can validate these are ordered
    const erizo::RtpHeader *poppedHeader = reinterpret_cast<const erizo::RtpHeader*>(packet->data);
    EXPECT_EQ(x, poppedHeader->getSeqNumber());
  }
}


TEST_F(RtpPacketQueueTest, rtpPacketQueueCorrectlyHandlesSequenceNumberRollover) {
  erizo::RtpPacketQueue queue;
  // We'll start at 65530 and add one to an unsigned integer until we have overflow
  // samples.
  uint16_t x = 65530;
  while(x != 5) {
    erizo::RtpHeader header;
    header.setSeqNumber(x);
    x += 1;
    queue.pushPacket((const char *)&header, sizeof(erizo::RtpHeader));
  }

  x = 65530;
  while( x != 5) {
    boost::shared_ptr<erizo::dataPacket> packet = queue.popPacket(true); // override our default pop behavior so we can validate these are ordered
    const erizo::RtpHeader *poppedHeader = reinterpret_cast<const erizo::RtpHeader*>(packet->data);
    EXPECT_EQ(x, poppedHeader->getSeqNumber());
    x += 1;
  }

  EXPECT_EQ(0, queue.getSize());
}

TEST_F(RtpPacketQueueTest, rtpPacketQueueCorrectlyHandlesSequenceNumberRolloverBackwards) {
  erizo::RtpPacketQueue queue;
  // We'll start at 5 and subtract till we're at 65530.  Then make sure we get back what we put in.
  uint16_t x = 4;     // has to be 4, or we have an off-by-one error
  while(x != 65529) { // has to be 29, or have have an off-by-one error
    erizo::RtpHeader header;
    header.setSeqNumber(x);
    x -= 1;
    queue.pushPacket((const char *)&header, sizeof(erizo::RtpHeader));
  }
  
  x = 65530;
  while( x != 5) {
    boost::shared_ptr<erizo::dataPacket> packet = queue.popPacket(true); // override our default pop behavior so we can validate these are ordered
    const erizo::RtpHeader *poppedHeader = reinterpret_cast<const erizo::RtpHeader*>(packet->data);
    EXPECT_EQ(x, poppedHeader->getSeqNumber());
    x += 1;
  }
  
  EXPECT_EQ(0, queue.getSize());
}

TEST_F(RtpPacketQueueTest, rtpPacketQueueDoesNotPushSampleLessThanWhatHasBeenPopped) {
  erizo::RtpPacketQueue queue;

  erizo::RtpHeader header;
  header.setSeqNumber(12);
  queue.pushPacket((const char *)&header, sizeof(erizo::RtpHeader));

  // Pop this packet
  queue.popPacket(true);

  // Then try to add a packet with a lower sequence number.  This packet should not have
  // been added to the queue.
  header.setSeqNumber(11);
  queue.pushPacket((const char *)&header, sizeof(erizo::RtpHeader));
  EXPECT_EQ(0, queue.getSize());
  EXPECT_EQ(false, queue.hasData()); 

  // Then try to add a packet with the same sequence number.  This packet should not have
  // been added to the queue.
  header.setSeqNumber(12);
  queue.pushPacket((const char *)&header, sizeof(erizo::RtpHeader));
  EXPECT_EQ(0, queue.getSize());
  EXPECT_EQ(false, queue.hasData());
}

TEST_F(RtpPacketQueueTest, rtpPacketQueueMakesDataAvailableOnceEnoughSamplesPushed) {
  unsigned int max = 10, depth = 5;
  erizo::RtpPacketQueue queue(depth, max);  // max and depth.
  queue.setTimebase(1);

  uint16_t x = 0;

  for(x = 0; x < (depth - 1); x++) {
    erizo::RtpHeader header;
    header.setSeqNumber(x);
    queue.setTimebase(1);
    queue.pushPacket((const char *)&header, sizeof(erizo::RtpHeader));
  }

  // Should have (depth - 1) samples.  We should not be ready yet.
  EXPECT_EQ(depth - 1, queue.getSize());
  EXPECT_EQ(false, queue.hasData());

  // Add one more sample, verify that we have 5, and hasData now returns true.
  erizo::RtpHeader header;
  header.setSeqNumber(++x);
  header.setTimestamp(++x);
  queue.pushPacket((const char *)&header, sizeof(erizo::RtpHeader));

  EXPECT_EQ(depth, queue.getSize());
  EXPECT_EQ(true, queue.hasData());
}

TEST_F(RtpPacketQueueTest, rtpPacketQueueRespectsMax) {
  unsigned int max = 10, depth = 5;
  erizo::RtpPacketQueue queue(depth, max);  // max and depth.
  queue.setTimebase(1);   // dummy timebase.
  uint16_t x = 0;

  // let's push ten times the max.
  for(uint16_t x = 0; x < (max * 10); x++) {
    erizo::RtpHeader header;
    header.setSeqNumber(x);
    header.setTimestamp(x);
    queue.pushPacket((const char *)&header, sizeof(erizo::RtpHeader));
  }

  EXPECT_EQ(max+1, queue.getSize());
  EXPECT_EQ(true, queue.hasData());
}

TEST_F(RtpPacketQueueTest, rtpPacketQueueRejectsDuplicatePackets) {
  erizo::RtpPacketQueue queue;
  // Add ten packets.
  for(uint16_t x = 0; x < 10; x++) {
    erizo::RtpHeader header;
    header.setSeqNumber(x);
    queue.pushPacket((const char *)&header, sizeof(erizo::RtpHeader));
  }

  // Let's try to add 5, 6, 7, 8, and 9 again.
  for(int x = 5; x < 10; x++) {
    erizo::RtpHeader header;
    header.setSeqNumber(x);
    queue.pushPacket((const char *)&header, sizeof(erizo::RtpHeader));
  }

  // We should only see ten packets, because those should all have been rejected.
  EXPECT_EQ(10, queue.getSize());
}


TEST_F(RtpPacketQueueTest, depthCalculationHandlesTimestampWrap) {
  // In the RTP spec, timestamps for a packet are 32 bit unsigned, and can overflow (very possible given that the starting
  // point is random.  Test that our depth works correctly
  unsigned int max = 10, depth = 5;
  erizo::RtpPacketQueue queue(depth, max);  // max and depth.
  queue.setTimebase(1);   // dummy timebase.

  uint32_t x = UINT_MAX - 4;
  while( x != 1) {
    erizo::RtpHeader header;
    header.setSeqNumber(x);
    header.setTimestamp(x);
    queue.pushPacket((const char *)&header, sizeof(erizo::RtpHeader));
    x++;    // overflow causes us to hit 1
  }

  // at this point, we should have data, but not enough to pass hasData()
  EXPECT_EQ(false, queue.hasData());

  // Add one more packet, and we should have data
  erizo::RtpHeader header;
  header.setSeqNumber(x);
  header.setTimestamp(x);
  queue.pushPacket((const char *)&header, sizeof(erizo::RtpHeader));
  EXPECT_EQ(true, queue.hasData());

  // Add a bunch more packets, and make sure the max is being enforced
  while (x != 100) {
    erizo::RtpHeader header;
    header.setSeqNumber(x);
    header.setTimestamp(x);
    queue.pushPacket((const char *)&header, sizeof(erizo::RtpHeader));
    x++;
  }

  // Max should be respected, we should have data
  EXPECT_EQ(max + 1, queue.getSize());
  EXPECT_EQ(true, queue.hasData());
}


}  // anonymous namespace
}  // namespace erizo
