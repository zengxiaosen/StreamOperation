/*
 * rtcp_receiver.cc
 *
 *  Created on: May 17, 2016
 *      Author: chenteng
 */

#include "rtcp_receiver.h"
#include "glog/logging.h"

namespace orbit {

RtcpReceiver::RtcpReceiver(AudioRtcpContext *audio_rtcp_ctx, VideoRtcpContext *video_rtcp_ctx) {
  audio_rtcp_ctx_ = audio_rtcp_ctx;
  video_rtcp_ctx_ = video_rtcp_ctx;
}

RtcpReceiver::~RtcpReceiver() {
}

void RtcpReceiver::RtcpProcessIncomingRtp(packetType packet_type, 
                                          char* buf, int len) {
  /* Update the RTCP context as well */
  bool is_video = (packet_type == VIDEO_PACKET);
  rtcp_context *rtcp_ctx = is_video ? video_rtcp_ctx_->rtcp_ctx : 
                           audio_rtcp_ctx_->rtcp_ctx;
  std::mutex *rtcp_mtx = is_video ? &(video_rtcp_ctx_->rtcp_mtx) :
                           &(audio_rtcp_ctx_->rtcp_mtx);
  rtcp_mtx->lock();
  janus_rtcp_process_incoming_rtp(rtcp_ctx, buf, len, janus_get_max_nack_queue());
  rtcp_mtx->unlock();
}

void RtcpReceiver::RtcpProcessIncomingRtcp(packetType packet_type,
                                           char* buf, int len) {
  /* Let's process this RTCP (compound?) packet, and update the RTCP context for this stream in case */
  bool is_video = (packet_type == VIDEO_PACKET);
  rtcp_context *rtcp_ctx = 
    is_video ? video_rtcp_ctx_->rtcp_ctx : audio_rtcp_ctx_->rtcp_ctx;
  std::mutex *rtcp_mtx = 
    is_video ? &(video_rtcp_ctx_->rtcp_mtx) : &(audio_rtcp_ctx_->rtcp_mtx);
  rtcp_mtx->lock();
  janus_rtcp_parse(rtcp_ctx, buf, len);
  rtcp_mtx->unlock();
}

} /* namespace orbit */
