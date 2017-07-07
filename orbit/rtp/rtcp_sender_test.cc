/*
 * rtcp_sender_test.cc
 *
 *  Created on: May 17, 2016
 *      Author: chenteng
 */

#include "rtcp_sender.h"
#include "gtest/gtest.h"
#include "glog/logging.h"
#include "gflags/gflags.h"
//#include "stream_service/orbit/webrtc_endpoint.h"
#include "stream_service/orbit/sdp_info.h"
#include "stream_service/orbit/nice_connection.h"
#include "stream_service/orbit/transport_delegate.h"

DEFINE_bool(enable_red_fec, true, "EnablR Video RED/FEC codecs");
DEFINE_bool(only_public_ip, false, "use public ip only on candidate.");
DEFINE_string(packet_capture_filename, "", "If any filename is specified, "
              "the rtp packet capture will be enabled and stored into the file."
              " e.g. video_rtp.pb");

namespace orbit {
//namespace {

class TestTransportDelegate : public TransportDelegate {
public:
  TestTransportDelegate(bool audio_enabled, bool video_enabled,
                    bool trickle_enabled, const IceConfig& ice_config,
                        WebRtcEndpoint* endpoint):TransportDelegate(audio_enabled, video_enabled, trickle_enabled, ice_config, endpoint, 0, 0) {};
  ~TestTransportDelegate() {};
  bool isRunning() {
    return running_;
  }
  void setRunning(bool running) {
    running_ = running;
  }

  void RelayPacket(const dataPacket& packet) {
    if (packet.type == VIDEO_PACKET) {
      LOG(INFO) <<"SEND VIDEO_PACKET..";
    }
    if (packet.type == VIDEO_RTX_PACKET) {
      LOG(INFO) <<"SEND VIDEO_RTX_PACKET..";
    }
    if (packet.type == RETRANSMIT_PACKET) {
      LOG(INFO) <<"SEND RETRANSMIT_PACKET..";
    }
  }
private:
  // The flag of the running status of the class/thread.
  bool running_ = false;
};

orbit::IceConfig ice_config;
orbit::TestTransportDelegate transport_delegate(true,true,true,ice_config ,NULL);
orbit::SdpInfo remote_sdp;

class RtcpSenderTest : public testing::Test {
 protected:
  virtual void SetUp() override {
  }

  virtual void TearDown() override {
  }
};


TEST_F(RtcpSenderTest, rtcpSenderTestInit) {
  orbit::RtcpSender rtcp_sender(&transport_delegate,remote_sdp);
  AudioRtcpContext audio_rtcp_ctx;
  VideoRtcpContext video_rtcp_ctx;
  rtcp_sender.Init(&audio_rtcp_ctx, &video_rtcp_ctx);
}

//} // anonymous namespaces
} /* namespace orbit */
