// Copyright 2016 Orangelab Inc. All Rights Reserved.
// Author: cheng@orangelab.com (Cheng Xu)
//
// Unittest for RtpCapture

#include "gtest/gtest.h"

#include "stream_service/orbit/media_definitions.h"
#include "stream_service/orbit/base/file.h"
#include "rtp_capture.h"

namespace orbit {
namespace {

class RtpCaptureTest : public testing::Test {
 protected:
  virtual void SetUp() override {
  }
  virtual void TearDown() override {
  }
};

dataPacket constructFakeDataPacket() {
  dataPacket packet;
  packet.comp = 0;
  char* buf = "abcdefg...";
  int len = 10;
  memcpy(&(packet.data[0]), buf, len);
  packet.length = len;
  packet.type = AUDIO_PACKET;
  return packet;
}

// Test rtp_capture with simple usage.
TEST_F(RtpCaptureTest, CaptureAPacket) {
  string packet_capture_filename = "./test.pb";
  RtpCapture* rtp_capture = new RtpCapture(packet_capture_filename);
  intptr_t transport_address = reinterpret_cast<intptr_t>(this);
  dataPacket packet = constructFakeDataPacket();
  rtp_capture->CapturePacket(transport_address, packet);
  delete rtp_capture;

  RtpReplay rtp_replay;
  rtp_replay.Init(packet_capture_filename);
  int i = 0;
  std::shared_ptr<StoredPacket> read_packet;
  do {
    read_packet = rtp_replay.Next();
    if (read_packet == NULL) {
      break;
    } else {
      EXPECT_EQ(packet.length, read_packet->packet_length());
      EXPECT_EQ(packet.data[5], read_packet->data()[5]);
      EXPECT_EQ(packet.data[9], read_packet->data()[9]);
      EXPECT_EQ(AUDIO_PACKET, read_packet->type());
    }
    i++;
  } while (read_packet != NULL);

  EXPECT_EQ(1, i);
  EXPECT_TRUE(File::Delete(packet_capture_filename));
}

}  // namespace annoymous

}  // namespace orbit
  
