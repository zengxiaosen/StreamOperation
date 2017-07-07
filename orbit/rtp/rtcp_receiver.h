/*
 * rtcp_receiver.h
 *
 *  Created on: May 17, 2016
 *      Author: chenteng
 */

#pragma once
#include "janus_rtcp_processor.h"
#include "stream_service/orbit/media_definitions.h"

namespace orbit {

class RtcpReceiver {
public:
  RtcpReceiver(AudioRtcpContext *audio_rtcp_ctx, VideoRtcpContext *video_rtcp_ctx);
  virtual ~RtcpReceiver();
  void RtcpProcessIncomingRtp(packetType packet_type, char* buf, int len);
  void RtcpProcessIncomingRtcp(packetType packet_type, char* buf, int len);

private:
  AudioRtcpContext *audio_rtcp_ctx_;
  VideoRtcpContext *video_rtcp_ctx_;
};

} /* namespace orbit */

