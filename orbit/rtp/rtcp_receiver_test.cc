/*
 * rtcp_receiver_test.cc
 *
 *  Created on: May 17, 2016
 *      Author: chenteng
 */
#include "gtest/gtest.h"
#include "rtcp_receiver.h"
#include "stream_service/orbit/media_definitions.h"
#include "rtp_headers.h"

namespace orbit {
namespace {

class RtcpReceiverTest : public testing::Test {
 protected:
  virtual void SetUp() override {
  }

  virtual void TearDown() override {
  }
};

TEST_F(RtcpReceiverTest, correctRtcpProcessIncomingRtp) {
  AudioRtcpContext audio_rtcp_ctx;
  VideoRtcpContext video_rtcp_ctx;
  orbit::RtcpReceiver rtcp_receiver(&audio_rtcp_ctx, &video_rtcp_ctx);
  packetType pt = VIDEO_PACKET;

  orbit::RtpHeader header1;
  header1.setSeqNumber(12);

  rtcp_receiver.RtcpProcessIncomingRtp(pt, (char *)&header1, sizeof(orbit::RtpHeader));

  // Validate video init value
  rtcp_context *rtcp_ctx = video_rtcp_ctx.rtcp_ctx;

  EXPECT_EQ(1, rtcp_ctx->enabled);
  EXPECT_EQ(12, rtcp_ctx->base_seq);    // The base_seq == header1.getSeqNumber();
  EXPECT_EQ(1, rtcp_ctx->expected);
  EXPECT_EQ(0, rtcp_ctx->expected_prior);
  EXPECT_EQ(0, rtcp_ctx->jitter);
  EXPECT_EQ(0, rtcp_ctx->last_sent);
  EXPECT_EQ(12, rtcp_ctx->last_seq_nr);  // The last seq number 's defalut value is equal to the base_seq's value.
  EXPECT_EQ(0, rtcp_ctx->lost);
  EXPECT_EQ(0, rtcp_ctx->lsr);
  EXPECT_EQ(0, rtcp_ctx->lsr_ts);
  EXPECT_EQ(0, rtcp_ctx->pt);
  EXPECT_EQ(1, rtcp_ctx->received);
  EXPECT_EQ(0, rtcp_ctx->received_prior);
  EXPECT_EQ(0, rtcp_ctx->seq_cycle);
  EXPECT_EQ(90000, rtcp_ctx->tb);
  EXPECT_GT(rtcp_ctx->transit, 90000);

  // Another video packet arrives, and it's sequence number is 14, the packet that it's sequence number is 13 is lost.
  orbit::RtpHeader header2;
  header2.setSeqNumber(14);
  rtcp_receiver.RtcpProcessIncomingRtp(pt, (char *)&header2, sizeof(orbit::RtpHeader));
  EXPECT_EQ(1, rtcp_ctx->lost);

}

TEST_F(RtcpReceiverTest, correctRtcpProcessIncomingRtcp) {

}

}
} /* namespace orbit */
