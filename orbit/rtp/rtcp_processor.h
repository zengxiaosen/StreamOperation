/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * rtcp_processor.h
 * ---------------------------------------------------------------------------
 * Defines the interface for processing the rtcp packets.
 * ---------------------------------------------------------------------------
 */

#pragma once

#include <mutex>
#include <memory>
#include <vector>

#include "rtp_headers.h"
#include "janus_rtcp_processor.h"
#include "stream_service/orbit/media_definitions.h"

#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_header_parser.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/rtp_rtcp/include/remote_ntp_time_estimator.h"
#include "stream_service/orbit/network_status.h"

namespace orbit {

class TransportDelegate;
class RtcpSender;
class RtcpReceiver;
class NackProcessor;
class RembProcessor;
class SdpInfo;

typedef struct RtcpSyncContext {
  RtcpSyncContext() {
    webrtc::Clock* clock = webrtc::Clock::GetRealTimeClock();
    ntp_estimator_.reset(new webrtc::RemoteNtpTimeEstimator(clock));
    // new SSRC reset old reports
    memset(&_remoteSenderInfo, 0, sizeof(_remoteSenderInfo));
    _lastReceivedSRNTPsecs = 0;
    _lastReceivedSRNTPfrac = 0;
  }

  std::unique_ptr<webrtc::RemoteNtpTimeEstimator> ntp_estimator_;
  // Received send report
  webrtc::RTCPSenderInfo _remoteSenderInfo;
  // when did we receive the last send report
  uint32_t _lastReceivedSRNTPsecs;
  uint32_t _lastReceivedSRNTPfrac;
  std::mutex ntp_mtx_;

  // Get remote NTP.
  bool RemoteNTP(uint32_t* ReceivedNTPsecs,
                 uint32_t* ReceivedNTPfrac,
                 uint32_t* RTCPArrivalTimeSecs,
                 uint32_t* RTCPArrivalTimeFrac,
                 uint32_t* rtcp_timestamp) {
    std::lock_guard<std::mutex> lock(ntp_mtx_);
    if(ReceivedNTPsecs) {
      *ReceivedNTPsecs = _remoteSenderInfo.NTPseconds; // NTP from incoming SendReport
    }
    if(ReceivedNTPfrac) {
      *ReceivedNTPfrac = _remoteSenderInfo.NTPfraction;
    }
    if(RTCPArrivalTimeFrac) {
      *RTCPArrivalTimeFrac = _lastReceivedSRNTPfrac; // local NTP time when we received a RTCP packet with a send block
    }
    if(RTCPArrivalTimeSecs) {
      *RTCPArrivalTimeSecs = _lastReceivedSRNTPsecs;
    }
    if (rtcp_timestamp) {
      *rtcp_timestamp = _remoteSenderInfo.RTPtimeStamp;
    }
    return true;
  }

} RtcpSyncContext;

typedef struct RttContext {
  /* Estimated RTT */
  int64_t  RTT = 0;
  int64_t  minRTT = 0;
  int64_t  maxRTT = 0;
  int64_t  avgRTT = 0;
  uint32_t numAverageCalcs = 0;
} RttContext;

#define AudioRtcpSyncContext RtcpSyncContext
#define VideoRtcpSyncContext RtcpSyncContext

class RtcpProcessorRemoteBitrateObserver : public webrtc::RemoteBitrateObserver {
 public:
   explicit RtcpProcessorRemoteBitrateObserver(webrtc::Clock* clock) : clock_(clock) {}
   
   // Called when a receive channel group has a new bitrate estimate for the
   // incoming streams.
   virtual void OnReceiveBitrateChanged(const std::vector<unsigned int>& ssrcs,
                                        unsigned int bitrate) {
     VLOG(3) << "[" << static_cast<uint32_t>(clock_->TimeInMilliseconds()) << "]"
             << " Num SSRCs:" << static_cast<int>(ssrcs.size())
             << " bitrate:" << bitrate;
   }
   
   virtual ~RtcpProcessorRemoteBitrateObserver() {}
   
 private:
   webrtc::Clock* clock_;
};

class RtcpProcessor {
  public:
    RtcpProcessor(TransportDelegate *transport_delegate);
    ~RtcpProcessor();

    /*
     * Set the remote sdp to RtcpSender object, and initate RtcpSender.
     */ 
    void SetRemoteSdp(const SdpInfo* remote_sdp);

    void InitWithSsrcs(std::vector<uint32_t> extended_video_ssrcs,
                       uint32_t default_video_ssrc,
                       uint32_t default_audio_ssrc);

    /*
     * Process the input rtp packet in RtcpProcessor module, because we should 
     * collect some information to determine whether should to send an NACK 
     */
    void RtcpProcessIncomingRtp(packetType packet_type, char* buf, int len);

    /*
     * Process the incoming rtcp packet by packet type.
     */
    void RtcpProcessIncomingRtcp(packetType packet_type, char* buf, int len);

    void NackPushVideoPacket(char* buf, int len);

    void NackPushAudioPacket(char* buf, int len);

    void SendRembPacket(uint64_t bitrate);

    void UpdateBitrate(uint64_t bitrate);

    void SlowLinkUpdate(unsigned int nacks, int video, int uplink, uint64_t now);

    void SendVideoNackByCondition(uint32_t ssrc, uint16_t seqnumber);

    void SendAudioNackByCondition(uint16_t seqnumber);

    void SendRTCP(int rtcp_type, packetType media_type = VIDEO_PACKET,
                  char* rtcp_buf = NULL, int len = 0);

    // Get RTT(RoundTripTime).
    bool RTT(uint32_t ssrc,
             int64_t* rtt,
             int64_t* avg_rtt,
             int64_t* min_rtt,
             int64_t* max_rtt);

    // Get the estimated bitrate.
    bool GetBitrateEstimate(packetType packet_type,
                            unsigned int* target_bitrate,
                            std::vector<unsigned int>* ssrcs);

    // Estimate the RemoteNTPTimeMS for the given rtp timestamp.
    // Returns -1 if the remote_ntp_estimator has not been given enough data.
    // (Note that the remote_ntp_estimator must be fed with at least
    //  2 SR for enough information to estimate the NTP time).
    int64_t EstimateRemoteNTPTimeMS(packetType packet_type,
                                    uint32_t timestamp);
  private:
    /**
     * The function used to update the RTT value about the stream 'local_ssrc'
     * when one RR packet received.
     *
     * @param[in] buf  Incoming Receiver Report packet buf.
     * @param[in] len  The length of buf
     * @param[in] local_ssrc The stream indicator, to indicate which stream's
     *                       RTT need to be updated.
     */
    void UpdateRTT(char* buf, int len, uint32_t local_ssrc);

    void UpdateRemoteNTPEstimator(packetType packet_type,
                                  RtcpHeader* rtcp_header);

    void LogRtcpReceiverPT(RtcpHeader* rtcp_header, packetType packet_type);

    void GenerateNackProcessorsByExtendedSsrcs(
        std::vector<uint32_t> extended_video_ssrcs);

    std::shared_ptr<NackProcessor> GetNackProcessor(uint32_t media_ssrc);
    uint32_t GetRtpfbNackMediaSSRC(char* buf, int len);

    void HandlePLI();
    void HandleSLI();
    void HandleFIR();
    void HandleSDES(packetType media_type, char* buf, int len);
    void HandleNackItem(packetType packet_type, char* buf, int len);
    void HandlePSFB(char* buf, int len);
    void HandleRTPFB(char* buf, int len);
    void HandleSenderReport(packetType packetType, char* buf, int len);
    void HandleReceiverReport(packetType packet_type, char* buf, int len); 

    void InitializeAudioAndVideoContext();

    void InitRttContextsWithSsrcs(std::vector<uint32_t> ext_video_ssrcs,
                                  uint32_t default_video_ssrc,
                                  uint32_t default_audio_ssrc);
    
    RttContext* GetRttContext(uint32_t ssrc);

    RtcpSender *rtcp_sender_ = NULL;
    RtcpReceiver *rtcp_receiver_ = NULL;
    NackProcessor *audio_nack_processor_ = NULL;
    std::shared_ptr<NackProcessor> default_video_nack_processor_;
    std::map<uint32_t, std::shared_ptr<NackProcessor>> extended_video_nack_processors_;
    RembProcessor *remb_processor_ = NULL;
    TransportDelegate *trans_delegate_ = NULL;

    AudioRtcpContext audio_rtcp_ctx_;
    VideoRtcpContext video_rtcp_ctx_;

    std::map<uint32_t, RttContext> rtt_ctxs_;
    std::mutex rtt_ctx_mtx_;

    VideoRtcpSyncContext video_sync_ctx_;
    AudioRtcpSyncContext audio_sync_ctx_;

    webrtc::Clock* clock_;
    std::unique_ptr<RtcpProcessorRemoteBitrateObserver> observer_;
    std::unique_ptr<webrtc::RemoteBitrateEstimator> 
        audio_remote_bitrate_estimator_;
    std::unique_ptr<webrtc::RemoteBitrateEstimator> 
        video_remote_bitrate_estimator_;
    std::unique_ptr<webrtc::RtpHeaderParser> rtp_header_parser_;
    std::shared_ptr<NetworkStatus> network_status_ = NULL;
};
} /* namespace orbit */
