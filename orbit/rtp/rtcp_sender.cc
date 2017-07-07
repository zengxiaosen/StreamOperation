/*
 * Copyright 2016 (C) Orange Labs Inc. All Rights Reserved.
 * Author: chenteng@orangelab.cn (Chen Teng)
 *
 * rtcp_sender.cc
 * ---------------------------------------------------------------------------
 * Implement a module as Rtcp sender with the functions to send RTCP SR/RR
 * (sender/receiver report) and a dedicated thread for sending it periodly.
 * ---------------------------------------------------------------------------
 * Created on: May 17, 2016
 */

#include "rtcp_sender.h"

#include <glib.h>
#include <chrono>
#include <sys/prctl.h>
#include "stream_service/orbit/sdp_info.h"
#include "stream_service/orbit/rtp/rtp_headers.h"
#include "stream_service/orbit/transport_delegate.h"

#define SEND_RRSR_SLEEP_TIME 10000 // in us, i.e. 1000us = 1ms
#define DEFAULT_RTCP_SR_INTERVAL 500000 // in us, i.e 500 ms
#define DEFAULT_RTCP_RR_INTERVAL 1 // in sec. 
namespace orbit {

RtcpSender::RtcpSender(TransportDelegate *transport_delegate) {
  audio_rtcp_ctx_ = NULL;
  video_rtcp_ctx_ = NULL;
  audio_rtcp_last_rr_ = 0;
  video_rtcp_last_rr_ = 0;
  audio_rtcp_last_sr_ = 0;
  video_rtcp_last_sr_ = 0;

  trans_delegate_ = transport_delegate;
  clock_ = webrtc::Clock::GetRealTimeClock();

  network_status_ = NetworkStatusManager::Get(transport_delegate->session_id(),
                                              transport_delegate->stream_id());
}

RtcpSender::~RtcpSender() {
  if (rrsr_send_thread_ != NULL) {
    rrsr_send_thread_->join();
  }
}

void RtcpSender::SetRemoteSdp(const SdpInfo* remote_sdp) {
  remote_audio_ssrc_ = remote_sdp->getAudioSsrc();
  remote_video_ssrc_ = remote_sdp->getVideoSsrc();
  // Initate one rr&sr send thread.
  rrsr_send_thread_.reset(
    new boost::thread(boost::bind(&RtcpSender::SendRrSrPacketLoop, this)));
}

void RtcpSender::InitSrSendHistories(
    std::vector<uint32_t> extended_video_ssrcs) {
  uint32_t default_video_ssrc = trans_delegate_->VIDEO_SSRC;
  uint32_t default_audio_ssrc = trans_delegate_->AUDIO_SSRC;
  std::vector<uint32_t> ext_video_ssrcs = 
    trans_delegate_->GetExtendedVideoSsrcs();
  {
    std::lock_guard<std::mutex> lock(sr_send_history_mtx_);
    if (!ext_video_ssrcs.empty()) {
      for (auto local_ssrc : ext_video_ssrcs) {
        SrSendHistory sr_send_history{0};
        sr_send_histories_[local_ssrc] = sr_send_history; 
      }
    }
    SrSendHistory sr_send_history{0};
    sr_send_histories_[default_video_ssrc] = sr_send_history;
    sr_send_histories_[default_audio_ssrc] = sr_send_history;
  }
}

void RtcpSender::Init(AudioRtcpContext *audio_rtcp_ctx, 
                      VideoRtcpContext *video_rtcp_ctx) {
  signed long before = janus_get_monotonic_time();
  audio_rtcp_last_rr_ = before;
  audio_rtcp_last_sr_ = before;
  video_rtcp_last_rr_ = before;
  video_rtcp_last_sr_ = before;

  audio_rtcp_ctx_ = audio_rtcp_ctx;
  video_rtcp_ctx_ = video_rtcp_ctx;
}

void RtcpSender::SendRTCP(int rtcp_type, packetType media_type, 
                          char* rtcp_buf, int len) {
  //TODO implement other RTCP types
  switch(rtcp_type) {
  case RTCP_BYE:
    SendBYE();
    break; 
  case RTCP_SDES:
    SendSDES(media_type);
    break;
  case RTCP_RTP_Feedback_PT:
    SendRtpFeedback(media_type, rtcp_buf, len);
    break;
  default:
    LOG(ERROR) <<"The packet type " << rtcp_type <<" is not supported!";
    break;
  }
}

void RtcpSender::SendBYE() {
  char buf[20];
  memset(buf, 0, 20);
  int len = janus_rtcp_bye((char *)&buf, 12);
  RelayRtcpPacket(VIDEO_PACKET, buf, len);
}

void RtcpSender::ConstructAndRelaySrPacket(packetType packet_type, 
                                           uint32_t send_packets, 
                                           uint32_t send_bytes,
                                           uint32_t last_ts,
                                           uint32_t local_ssrc,
                                           uint32_t remote_ssrc,
                                           uint32_t ntp_sec, 
                                           uint32_t ntp_frac) {
  /* Create a SR packet */
  int srlen = 28;
  char rtcpbuf[srlen];
  rtcp_sr *sr = (rtcp_sr *)&rtcpbuf;
  sr->header.version = 2;
  sr->header.type = RTCP_SR;
  sr->header.rc = 0;
  sr->header.length = htons((srlen/4)-1);
  sr->si.ntp_ts_msw = htonl(ntp_sec);
  sr->si.ntp_ts_lsw = htonl(ntp_frac);
  sr->si.rtp_ts = htonl(last_ts);
  sr->si.s_packets = htonl(send_packets);
  sr->si.s_octets = htonl(send_bytes);
  // fix ssrc
  janus_rtcp_fix_ssrc(NULL,rtcpbuf, srlen, 1, local_ssrc, remote_ssrc);
  // relay packet
  RelayRtcpPacket(packet_type, rtcpbuf, srlen);
}

/**
 * Construct and send a SDES packet to the remote, the new local ssrc is not
 * indicate by this method, that will be fixed by janus_fix_ssrc method when
 * the SDES packet really relay by transport_delegate's queueData() method.
 */
void RtcpSender::SendSDES(packetType media_type) {
  int len = 20;
  char buf[20];
  memset(buf, 0, 20);
  if (media_type == VIDEO_PACKET) {
    janus_rtcp_sdes((char *)&buf, len, "orbitvideo", 10);
  } else if (media_type == AUDIO_PACKET) {
    janus_rtcp_sdes((char *)&buf, len, "orbitaudio", 10);  
  }
  RelayRtcpPacket(VIDEO_PACKET, buf, len);
}

void RtcpSender::SendRtpFeedback(packetType media_type, 
                                 char* rtcp_buf, int len) {
  RelayRtcpPacket(media_type, rtcp_buf, len);
}

void RtcpSender::RelayRtcpPacket(packetType media_type, char* buf, int len) {
  RtcpHeader* rtcp = reinterpret_cast<RtcpHeader*>(buf);
  VLOG(3) <<"virsion = " << (uint32_t)rtcp->version
          <<", packettype = " << (uint32_t)rtcp->getPacketType();

  dataPacket p;
  p.comp = 0;
  p.type = media_type;
  p.length = len;
  memcpy(&(p.data[0]), buf, len);
  trans_delegate_->RelayPacket(p);
}

void RtcpSender::SendRrSrPacketLoop() {
  // set thread name
  prctl(PR_SET_NAME, (unsigned long)"SendRrSr");
  while(trans_delegate_->isRunning()) {
    if(SendRrPacketInternal(AUDIO_RR)) {
      network_status_->UpdateJitters(SERVER_REPORT,
                                     false,
                                     server_report_audio_jitter_);
    }
    if(SendRrPacketInternal(VIDEO_RR)) {
      network_status_->UpdateJitters(SERVER_REPORT,
                                     true,
                                     server_report_video_jitter_);
    }
    SendSrPacketInternal(AUDIO_SR);
    SendSrPacketInternal(VIDEO_SR);

    usleep(SEND_RRSR_SLEEP_TIME);
  }
}

void RtcpSender::SendSrPacketInternal(ReportPacketType type) {
  // The send packets and send bytes from network statisticals
  int send_packets;
  int send_bytes;
  // Last sent audio RTP timestamp
  guint32 last_ts;
  // The timestamp of the last SR.
  unsigned long rtcp_last_sr;
  // The local ssrc of the stream
  int local_ssrc;
  // The remote ssrc of the stream
  int remote_ssrc;
  // AUDIO_PACKET/VIDEO_PACKET

  switch(type) {
  case AUDIO_SR: {
    rtcp_last_sr = audio_rtcp_last_sr_;
    local_ssrc = trans_delegate_->AUDIO_SSRC;
    send_packets = trans_delegate_->GetSendPackets(local_ssrc);
    send_bytes = trans_delegate_->GetSendBytes(local_ssrc);
    last_ts = trans_delegate_->GetLastTs(local_ssrc);
    remote_ssrc = remote_audio_ssrc_;

    unsigned long now = janus_get_monotonic_time();
    /* Let's check if it's time to send a RTCP SR as well */
    if(now - rtcp_last_sr >= DEFAULT_RTCP_SR_INTERVAL) {  // 500 ms
      if(send_packets > 0) {
        uint32_t ntp_sec, ntp_frac;
        clock_->CurrentNtp(ntp_sec, ntp_frac);
        ConstructAndRelaySrPacket(AUDIO_PACKET, send_packets, send_bytes, last_ts, 
                                  local_ssrc, remote_ssrc, ntp_sec, ntp_frac);
        //Update the last_send_report and rtcp_time array.
        UpdateLastSendReportAndRtcpTime(ntp_sec, ntp_frac, local_ssrc);
      }
      audio_rtcp_last_sr_ = now;
    }
    break;
  }
  case VIDEO_SR: {
    rtcp_last_sr = video_rtcp_last_sr_;
    local_ssrc = trans_delegate_->VIDEO_SSRC;
    send_packets = trans_delegate_->GetSendPackets(local_ssrc);
    send_bytes = trans_delegate_->GetSendBytes(local_ssrc);
    last_ts = trans_delegate_->GetLastTs(local_ssrc);
    remote_ssrc = remote_video_ssrc_;
      // Let's check if it's time to send a RTCP SR as well
    unsigned long now = janus_get_monotonic_time();
    if(now - rtcp_last_sr < DEFAULT_RTCP_SR_INTERVAL)   // 500 ms
      break;
      // Scan local ssrcs ,construct and send a SR packet
    std::vector<uint32_t> local_ssrcs = 
      trans_delegate_->GetExtendedVideoSsrcs();
    if (!local_ssrcs.empty()) {
      for (auto ssrc_iter : local_ssrcs) {
        send_packets = trans_delegate_->GetSendPackets(ssrc_iter);
        send_bytes = trans_delegate_->GetSendBytes(ssrc_iter);
        last_ts = trans_delegate_->GetLastTs(ssrc_iter);

        if(send_packets > 0) {
          uint32_t ntp_sec, ntp_frac;
          clock_->CurrentNtp(ntp_sec, ntp_frac);
          ConstructAndRelaySrPacket(VIDEO_PACKET, send_packets, send_bytes, last_ts, 
                                    ssrc_iter, remote_ssrc, ntp_sec, ntp_frac);
          //Update the last_send_report and rtcp_time array used to calculate RTT
          //values when the remote RR packet incoming.
          UpdateLastSendReportAndRtcpTime(ntp_sec, ntp_frac, ssrc_iter);
        }
      }
    } else {
      // Send SR message for the default video ssrc
      if(send_packets > 0) {
        uint32_t ntp_sec, ntp_frac;
        clock_->CurrentNtp(ntp_sec, ntp_frac);
        ConstructAndRelaySrPacket(VIDEO_PACKET, send_packets, send_bytes, last_ts, 
                                  local_ssrc, remote_ssrc, ntp_sec, ntp_frac);
          //Update the last_send_report and rtcp_time array.
        UpdateLastSendReportAndRtcpTime(ntp_sec, ntp_frac, local_ssrc);
      }
    }
    video_rtcp_last_sr_ = now;
    break;
  }
  default: {
    LOG(ERROR) << "Neither type is AUDIO_SR, nor VIDEO_SR. Fatal error.";
    return;
  }
  }
}

bool RtcpSender::SendRrPacketInternal(ReportPacketType type) {
  // The timestamp of the last RR.
  unsigned long rtcp_last_rr;
  uint32_t* server_report_jitter;
  // The RTCP context of video/audio stream
  JanusRtcpContext* rtcp_ctx;
  // AUDIO_PACKET/VIDEO_PACKET
  packetType packet_type;
  string packet_type_str;

  switch(type) {
  case AUDIO_RR: {
    rtcp_last_rr = audio_rtcp_last_rr_;
    rtcp_ctx = audio_rtcp_ctx_;
    packet_type = AUDIO_PACKET;
    packet_type_str = "AUDIO_PACKET";
    server_report_jitter = &server_report_audio_jitter_;
    break;
  }
  case VIDEO_RR: {
    rtcp_last_rr = video_rtcp_last_rr_;
    rtcp_ctx = video_rtcp_ctx_;
    packet_type = VIDEO_PACKET;
    packet_type_str = "VIDEO_PACKET";
    server_report_jitter = &server_report_video_jitter_;
    break;
  }
  default: {
    LOG(ERROR) << "Neither type is AUDIO_RR, nor VIDEO_RR. Fatal error.";
    return false;
  }
  }

  bool rr_sent = false;
  /* Let's check if it's time to send a RTCP RR as well */
  unsigned long now = janus_get_monotonic_time();
  if(now - rtcp_last_rr >=
     DEFAULT_RTCP_RR_INTERVAL * G_USEC_PER_SEC) {  // 1 second
    if(rtcp_ctx->rtcp_ctx) {
      /* Create a RR */
      int rrlen = 32;
      char rtcpbuf[32];
      rtcp_rr *rr = (rtcp_rr *)&rtcpbuf;
      rr->header.version = 2;
      rr->header.type = RTCP_RR;
      rr->header.rc = 1;
      rr->header.length = htons((rrlen/4)-1);
      
      rtcp_ctx->rtcp_mtx.lock();
      janus_rtcp_report_block(rtcp_ctx->rtcp_ctx, &rr->rb[0]);
      rtcp_ctx->rtcp_mtx.unlock();
      
      /* Enqueue it, we'll send it later */
      RelayRtcpPacket(packet_type, rtcpbuf, rrlen);
      
      RtcpHeader* rtcp = reinterpret_cast<RtcpHeader*>(rtcpbuf);      
      // for update varz/statusz output
      *server_report_jitter = (uint32_t)rtcp->getJitter();
      
      rr_sent = true;
    }
    // Update the audio{video}_rtcp_last_rr
    if (type == AUDIO_RR) {
      audio_rtcp_last_rr_ = now;
    } else {
      video_rtcp_last_rr_ = now;
    }
  }
  return rr_sent;
}

void RtcpSender::UpdateLastSendReportAndRtcpTime(uint32_t ntp_sec, 
                                                 uint32_t ntp_frac,
                                                 uint32_t local_ssrc) {
  {
    std::lock_guard<std::mutex> lock(sr_send_history_mtx_);
    auto iter = sr_send_histories_.find(local_ssrc);
    if (iter == sr_send_histories_.end()) {
      LOG(ERROR) <<"*****************ERROR*************,local_ssrc = " 
                 << local_ssrc;
      return;
    }
    SrSendHistory* sr_history = &(iter->second);
    // local NTP time when we received this  
    // Shift the old elements of the array.
    for (int i = (webrtc::RTCP_NUMBER_OF_SR * 2 - 2); i >= 0; i--) {
      sr_history->last_send_report_[i + 1] = sr_history->last_send_report_[i];
      sr_history->last_rtcp_time_[i + 1] = sr_history->last_rtcp_time_[i];
    }
    // Add the new one.
    sr_history->last_rtcp_time_[0] = webrtc::Clock::NtpToMs(ntp_sec, ntp_frac);
    sr_history->last_send_report_[0] = (ntp_sec << 16) + (ntp_frac >> 16);
  } // end of sr_send_history_mtx_
}

int64_t RtcpSender::GetSendTimeOfSendReport(uint32_t sendReport, uint32_t ssrc) {
  SrSendHistory* sr_send_history = NULL;
  // This is only saved when we are the sender
  {
    std::lock_guard<std::mutex> lock(sr_send_history_mtx_);
    auto iter = sr_send_histories_.find(ssrc);
    if (iter != sr_send_histories_.end()) {
      sr_send_history = &(iter->second); 
  }
  if ((sr_send_history == NULL) || 
    (sr_send_history->last_send_report_[0] == 0) || (sendReport == 0)) {
    return 0;  // will be ignored
  } else {
    for (int i = 0; i < webrtc::RTCP_NUMBER_OF_SR; ++i) {
      if (sr_send_history->last_send_report_[i] == sendReport)
        return sr_send_history->last_rtcp_time_[i];
    }
    VLOG(3) <<" not found the sr send report = " << sendReport;
  }
  return 0;
  } // end of sr_send_history_mtx_
}

} /* namespace orbit */
