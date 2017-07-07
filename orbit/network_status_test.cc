/*
 * Copyright 2016 All Rights Reserved.
 * Author: chenteng@orangelab.cn (Chen teng)
 *
 * network_status_test.cc
 */

#include "network_status.h"
#include <unistd.h>
#include "gtest/gtest.h"
#include "stream_service/orbit/base/singleton.h"
#include "stream_service/orbit/rtp/rtp_headers.h"
#include "stream_service/orbit/base/session_info.h"
#include "stream_service/orbit/rtp/janus_rtcp_defines.h"

#define FRACTION_UPDATE_INTERVAL 3000  // ms

namespace orbit {
namespace {

class NetworkStatusTest : public testing::Test {
 protected:
  virtual void SetUp() override {
  }

  virtual void TearDown() override {
  }
};

// Simulate the incoming packet's seqnumber, and calculate the lost rate every 3
// seconds into recv_fraction_lost_, get the recv_fraction_lost_ and compare to
// expected value to see whether NetworkStatus::StatisticRecvFractionLost()
// function is working correctly.
TEST_F(NetworkStatusTest, correctStatisticRecvFractionLostForVideo) {
  bool is_video = true;
  // create a NetworkStatus object.
  unsigned int session_id = 111;
  unsigned int stream_id = 222;
  orbit::NetworkStatus network_status(session_id, stream_id);
  // construct simulate data, min = 1002, max = 1021
  int packet[16] = {1002,1001,1004,1006,1008,
                    1012,1003,1005,1007,1009,
                    1015,1016,1017,1020,1018,1021};
  int expected_count = 20; // 1002 - 1021, except 1001
  unsigned int expected_value = 10 * 100 / expected_count;
 
  for (int i=0; i<16; i++) {
    network_status.StatisticRecvFractionLost(is_video, packet[i]);
    usleep (FRACTION_UPDATE_INTERVAL * 1000 / 15); // 200ms interval
  }
  // compare result
  unsigned int recv_fraction_lost = network_status.recv_fraction_lost_of_video();
  EXPECT_EQ(expected_value, recv_fraction_lost);

  /* again, input 10002 - 10017, lost = 8 */
  int another_packets[16] = {10002, 10001, 10004, 10006, 10008,
                             10012, 10003, 10005, 10007, 10009,
                             10015, 10016, 10016, 10006, 10008, 10017};
  expected_count = 16; //10002 - 10017 ,except 10001
  expected_value = 8 * 100 / expected_count; 
  // lost 10003,10005,10007,10009,10006, 10008

  for (int i=0; i<16; i++) {
    network_status.StatisticRecvFractionLost(is_video, another_packets[i]);
    usleep (FRACTION_UPDATE_INTERVAL * 1000 / 15); // 200 ms interval
  }
  // compare result again
  recv_fraction_lost = network_status.recv_fraction_lost_of_video();
  EXPECT_EQ(expected_value, recv_fraction_lost);
}

TEST_F(NetworkStatusTest, correctStatisticRecvFractionLostForAudio) {
  bool is_video = false;
  // create a NetworkStatus object.
  unsigned int session_id = 111;
  unsigned int stream_id = 222;
  orbit::NetworkStatus network_status(session_id, stream_id);
  // construct simulate data
  int packet[16] = {1002,1001,1004,1006,1008,
                    1012,1003,1005,1007,1009,
                    1015,1016,1017,1020,1018,1021};
  int expected_count = 20;
  unsigned int expected_value = 10 * 100 / expected_count;
 
  for (int i=0; i<16; i++) {
    network_status.StatisticRecvFractionLost(is_video, packet[i]);
    usleep (FRACTION_UPDATE_INTERVAL * 1000 / 15); // 200ms interval
  }
  // compare result
  unsigned int recv_fraction_lost = network_status.recv_fraction_lost_of_audio();
  EXPECT_EQ(expected_value, recv_fraction_lost);

  /* again, input 10002 - 10017, lost = 8 */
  int another_packets[16] = {10002, 10001, 10004, 10006, 10008,
                             10012, 10003, 10005, 10007, 10009,
                             10015, 10016, 10016, 10006, 10008, 10017};
  expected_count = 16;
  expected_value = 8 * 100 / expected_count;

  for (int i=0; i<16; i++) {
    network_status.StatisticRecvFractionLost(is_video, another_packets[i]);
    usleep (FRACTION_UPDATE_INTERVAL * 1000 / 15); // 200 ms interval
  }
  // compare result again
  recv_fraction_lost = network_status.recv_fraction_lost_of_audio();
  EXPECT_EQ(expected_value, recv_fraction_lost);
}

TEST_F(NetworkStatusTest, correctStatisticSendFractionLostForVideo) {
  bool is_video = true;
  uint32_t ssrc = 12345;
  // create a NetworkStatus object.
  unsigned int session_id = 111;
  unsigned int stream_id = 222;
  orbit::NetworkStatus network_status(session_id, stream_id);
  vector<unsigned int> extended_video_ssrcs;
  network_status.InitWithSsrcs(extended_video_ssrcs,
                               ssrc,
                               111);
  // construct simulate data
  int RR_highest_seqn[22] = {0, 20, 40, 60, 80,
                             100, 120, 140, 160, 180,
                             200, 220, 240, 260, 280,
                             300, 320, 340, 360, 380, 400, 420};
  int RR_cumulative_lost[22] = {0, 0, 5, 5, 10,
                                10, 15, 15, 20, 20,
                                25, 25, 30, 30, 35,
                                40, 40, 50, 50, 60, 65, 65};
  unsigned int expected_value_1 = (25-0) * 100 / (200-0);
  unsigned int expected_value_2 = (65-25) * 100 / (400-200);
  for (int i=0; i<22; i++) {
    network_status.StatisticSendFractionLost(is_video, RR_highest_seqn[i],
                                             RR_cumulative_lost[i], ssrc);
    usleep(FRACTION_UPDATE_INTERVAL * 1000 / 10); // 300 ms interval
    unsigned int send_fraction_lost = network_status.GetSendFractionLost(false, ssrc);
    if (i==10) {
      EXPECT_EQ(expected_value_1, send_fraction_lost);
    } 
  }
  unsigned int send_fraction_lost2 =
    network_status.GetSendFractionLost(false, ssrc);
  EXPECT_EQ(expected_value_2, send_fraction_lost2);
} 

TEST_F(NetworkStatusTest, correctStatisticSendFractionLostForAudio) {
  bool is_video = false;
  uint32_t ssrc = 12345;
  // create a NetworkStatus object.
  unsigned int session_id = 111;
  unsigned int stream_id = 222;
  orbit::NetworkStatus network_status(session_id, stream_id);
  vector<unsigned int> extended_video_ssrcs;
  network_status.InitWithSsrcs(extended_video_ssrcs,
                               ssrc,
                               111);
  // construct simulate data
  int RR_highest_seqn[22] = {0, 20, 40, 60, 80,
                             100, 120, 140, 160, 180,
                             200, 220, 240, 260, 280,
                             300, 320, 340, 360, 380, 400, 420};
  int RR_cumulative_lost[22] = {0, 0, 5, 5, 10,
                                10, 15, 15, 20, 20,
                                25, 25, 30, 30, 35,
                                40, 40, 50, 50, 60, 65, 65};
  unsigned int expected_value_1 = (25-0) * 100 / (200-0);
  unsigned int expected_value_2 = (65-25) * 100 / (400-200);
  for (int i=0; i<22; i++) {
    network_status.StatisticSendFractionLost(is_video, RR_highest_seqn[i],
                                             RR_cumulative_lost[i], ssrc);
    usleep(FRACTION_UPDATE_INTERVAL * 1000 / 10); // 300 ms interval
    unsigned int send_fraction_lost = network_status.GetSendFractionLost(false, ssrc);

    if (i==10) {
      EXPECT_EQ(expected_value_1, send_fraction_lost);
    } 
  }
  unsigned int send_fraction_lost2 =
    network_status.GetSendFractionLost(false, ssrc);
  EXPECT_EQ(expected_value_2, send_fraction_lost2);
}

}  // anonymous namespace
}  // namespace orbit
