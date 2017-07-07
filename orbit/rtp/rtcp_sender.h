/*
 * Copyright 2016 (C) Orange Labs Inc. All Rights Reserved.
 * Author: chenteng@orangelab.cn (Chen Teng)
 *
 * rtcp_sender.h
 * ---------------------------------------------------------------------------
 * Defines a class to implement a module as Rtcp sender.
 * The class contains the functions to send RTCP SR/RR (sender/receiver report)
 * and a dedicated thread for sending it periodly.
 * ---------------------------------------------------------------------------
 * Created on: May 17, 2016
 */

#pragma once

#include <boost/scoped_ptr.hpp>
#include <boost/thread.hpp>
#include <mutex>
#include "janus_rtcp_processor.h"

// For the defined webrtc constants 'RTCP_NUMBER_OF_SR'
#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_config.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "stream_service/orbit/network_status.h"
#include "stream_service/orbit/media_definitions.h"

namespace orbit {

// Sent report history and corresponding rtcp_time.
typedef struct _SrSendHistory {
  uint32_t last_send_report_[2 * webrtc::RTCP_NUMBER_OF_SR];
  int64_t last_rtcp_time_[2 * webrtc::RTCP_NUMBER_OF_SR];
} SrSendHistory;

class TransportDelegate;
class SdpInfo;
class RtcpSender {
public:
  RtcpSender(TransportDelegate *transport_delegate);
  
  ~RtcpSender();
  
  void Init(AudioRtcpContext *audio_rtcp_ctx,
            VideoRtcpContext *video_rtcp_ctx);

  void SetRemoteSdp(const SdpInfo* remote_sdp);

  void InitSrSendHistories(std::vector<uint32_t> extended_video_ssrcs);

  /**
   * Method used to get the send time of the SR indicated by 'sendReport' and
   * 'ssrc' input.
   * @param[in] sendReport The sr want checked
   * @param[in] ssrc       The local ssrc that the sr sent by
   * @return the time of SR send
   */
  int64_t GetSendTimeOfSendReport(uint32_t sendReport, uint32_t ssrc);

  void SendRTCP(int rtcp_type, packetType media_type, char* rtcp_buf, int len);

private:
  /**
   * Method used to construct and send a SR packet 
   * 
   * @param[in] packet_type  indicator video or audio packet ?
   * @param[in] send_packets the count of packets be sent by server
   * @param[in] send_bytes   the count of bytes be sent by server
   * @param[in] last_ts      the timstamp of the last rtp packet sent
   * @param[in] local_ssrc   the ssrc of server(sender) side 
   * @param[in] remote_ssrc  the ssrc of client(receiver) side 
   * @param[in] ntp_sec      the NTP timestamp when the SR was sending
   * @param[in] ntp_frac  
   */
  void ConstructAndRelaySrPacket(packetType packet_type,
                                 uint32_t send_packets, 
                                 uint32_t send_bytes,
                                 uint32_t last_ts,
                                 uint32_t local_ssrc,
                                 uint32_t remote_ssrc,
                                 uint32_t ntp_sec, 
                                 uint32_t ntp_frac);

  /* Methods for construct and send RTCP packets */
  void SendBYE();

  void SendSDES(packetType media_type);

  void SendRtpFeedback(packetType media_type, char* rtcp_buf, int len);

  void RelayRtcpPacket(packetType media_type, char* buf, int len);

  enum ReportPacketType {
    AUDIO_SR = 0,
    VIDEO_SR = 1,
    AUDIO_RR = 2,
    VIDEO_RR = 3
  };
  void SendSrPacketInternal(ReportPacketType type);
  bool SendRrPacketInternal(ReportPacketType type);

  // The thread loop to send RR and SR packets regularly.
  void SendRrSrPacketLoop(); 
  // Thread of sending RR/SR periodly.
  // TODO(chengteng): fix this to use std:thread.
  boost::scoped_ptr<boost::thread> rrsr_send_thread_;

  /**
   * Method used to update the SendReport message sent history, the histoies are
   * used for RTT calculate when the remote ReceiverReport message arrived.
   *
   * @param[in] ntp_sec     The NTP timestamp (combined by ntp_sec and ntp_frac)
   * @param[in] ntp_frac
   * @param[out] local_ssrc The local ssrc (server side) that SR send history need 
   *                        to be updated.
   */
  void UpdateLastSendReportAndRtcpTime(uint32_t ntp_sec,
                                       uint32_t ntp_frac,
                                       uint32_t local_ssrc);

  TransportDelegate *trans_delegate_;
  AudioRtcpContext *audio_rtcp_ctx_;
  VideoRtcpContext *video_rtcp_ctx_;

  signed long audio_rtcp_last_rr_;
  signed long video_rtcp_last_rr_;
  signed long audio_rtcp_last_sr_;
  signed long video_rtcp_last_sr_;
  signed long last_remb_;

  unsigned int recv_video_fraction_lost_{0};
  unsigned int recv_audio_fraction_lost_{0};
  unsigned int server_report_video_jitter_{0};
  unsigned int server_report_audio_jitter_{0};
  unsigned int client_report_video_jitter_{0};
  unsigned int client_report_audio_jitter_{0};

  unsigned int remote_audio_ssrc_{0};
  unsigned int remote_video_ssrc_{0};

  std::mutex sr_send_history_mtx_;
  std::map<uint32_t , SrSendHistory> sr_send_histories_;
 
  webrtc::Clock* clock_;
  std::shared_ptr<NetworkStatus> network_status_;
};

} /* namespace orbit */

