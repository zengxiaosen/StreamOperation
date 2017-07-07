/*
 * Copyright 2016 All Rights Reserved.
 * Author: chenteng@orangelab.cn (Chen teng)
 *
 * rtcp_processor.cc
 * ---------------------------------------------------------------------------
 * Implements the Rtcp processor to process all the Rtcp message.
 * ---------------------------------------------------------------------------
 */

#include <memory>
#include "glog/logging.h"
#include "rtcp_processor.h"
#include "rtcp_sender.h"
#include "rtp_format_util.h"
#include "rtcp_receiver.h"
#include "nack_processor.h"
#include "remb_processor.h"
#include "rtp_headers.h"
#include "janus_rtcp_processor.h"
#include "stream_service/orbit/sdp_info.h"
#include "stream_service/orbit/base/timeutil.h"
#include "stream_service/orbit/transport_delegate.h"
#include "stream_service/orbit/webrtc/modules/rtp_rtcp/source/rtcp_utility.h"
#include "webrtc/modules/remote_bitrate_estimator/remote_bitrate_estimator_single_stream.h"

#define FIR_PLI_WAIT       1000      // in milliseconds 

namespace orbit {

  RtcpProcessor::RtcpProcessor(TransportDelegate *transport_delegate) {
    InitializeAudioAndVideoContext();
    rtcp_sender_ = new RtcpSender(transport_delegate);
    rtcp_sender_->Init(&audio_rtcp_ctx_, &video_rtcp_ctx_); 

    rtcp_receiver_ = new RtcpReceiver(&audio_rtcp_ctx_, &video_rtcp_ctx_);
    audio_nack_processor_ = new NackProcessor(transport_delegate, this);
    default_video_nack_processor_.reset(new NackProcessor(transport_delegate, this));

    remb_processor_ = new RembProcessor(transport_delegate);

    clock_ = webrtc::Clock::GetRealTimeClock();
    observer_.reset(new RtcpProcessorRemoteBitrateObserver(clock_));
    rtp_header_parser_.reset(webrtc::RtpHeaderParser::Create());
    video_remote_bitrate_estimator_.reset(
        new webrtc::RemoteBitrateEstimatorSingleStream(observer_.get(), 
                                                       clock_));
    audio_remote_bitrate_estimator_.reset(
        new webrtc::RemoteBitrateEstimatorSingleStream(observer_.get(), 
                                                       clock_));
    network_status_ = NetworkStatusManager::Get(transport_delegate->session_id(),
                                                transport_delegate->stream_id());
    trans_delegate_ = transport_delegate;
  }

  void RtcpProcessor::InitializeAudioAndVideoContext() {
    /* We still need an RTCP context here, create it now */
    rtcp_context *rtcp_ctx = (rtcp_context*)malloc(sizeof(rtcp_context));
    /* init */
    rtcp_ctx->base_seq = 0;
    rtcp_ctx->enabled = 0;
    rtcp_ctx->expected = 0;
    rtcp_ctx->expected_prior = 0;
    rtcp_ctx->jitter = 0;
    rtcp_ctx->last_sent = 0;
    rtcp_ctx->last_seq_nr = 0;
    rtcp_ctx->lost = 0;
    rtcp_ctx->lsr = 0;
    rtcp_ctx->lsr_ts = 0;
    rtcp_ctx->pt = 0;
    rtcp_ctx->received = 0;
    rtcp_ctx->received_prior = 0;
    rtcp_ctx->seq_cycle = 0;
    rtcp_ctx->tb = 0;
    rtcp_ctx->transit = 0;
    
    rtcp_ctx->tb = 90000; /* FIXME */
    
    video_rtcp_ctx_.rtcp_ctx = rtcp_ctx;

    /* We still need an RTCP context here, create it now */
    rtcp_ctx = (rtcp_context*)malloc(sizeof(rtcp_context));
    /* init */
    rtcp_ctx->base_seq = 0;
    rtcp_ctx->enabled = 0;
    rtcp_ctx->expected = 0;
    rtcp_ctx->expected_prior = 0;
    rtcp_ctx->jitter = 0;
    rtcp_ctx->last_sent = 0;
    rtcp_ctx->last_seq_nr = 0;
    rtcp_ctx->lost = 0;
    rtcp_ctx->lsr = 0;
    rtcp_ctx->lsr_ts = 0;
    rtcp_ctx->pt = 0;
    rtcp_ctx->received = 0;
    rtcp_ctx->received_prior = 0;
    rtcp_ctx->seq_cycle = 0;
    rtcp_ctx->tb = 0;
    rtcp_ctx->transit = 0;
      
    rtcp_ctx->tb = 48000; /* FIXME */
    audio_rtcp_ctx_.rtcp_ctx = rtcp_ctx;
  }

  void RtcpProcessor::InitRttContextsWithSsrcs(
      std::vector<uint32_t> ext_video_ssrcs, 
      uint32_t default_video_ssrc,
      uint32_t default_audio_ssrc) {
    if (!ext_video_ssrcs.empty()) {
      // construct and initiate rtt_contexts with extend video ssrcs
      for (auto ssrc : ext_video_ssrcs) {
        RttContext rtt_ctx;
        std::lock_guard<std::mutex> lock(rtt_ctx_mtx_);
        rtt_ctxs_[ssrc] = rtt_ctx;
      }
    }
    // construct and initiate rtt_context with default video and audio ssrc 
    RttContext rtt_context;
    rtt_ctxs_[default_audio_ssrc] = rtt_context; 
    rtt_ctxs_[default_video_ssrc] = rtt_context; 
  }

  RttContext* RtcpProcessor::GetRttContext(uint32_t ssrc) {
    auto iter = rtt_ctxs_.find(ssrc); 
    if (iter != rtt_ctxs_.end()) {
      RttContext* rtt_ctx = &(iter->second);  
      return rtt_ctx;
    }
    return NULL;
  }

  RtcpProcessor::~RtcpProcessor() {
    if (rtcp_sender_ != NULL) {
      delete rtcp_sender_;
      rtcp_sender_ = NULL;
    }
    if (rtcp_receiver_ != NULL) {
      delete rtcp_receiver_;
      rtcp_receiver_ = NULL;
    }
    if (audio_nack_processor_ != NULL) {
      delete audio_nack_processor_;
      audio_nack_processor_ = NULL;
    }
    if (trans_delegate_ != NULL) {
      trans_delegate_ = NULL;
    }
    if (remb_processor_ != NULL) {
      delete remb_processor_;
      remb_processor_ = NULL;
    }
    if (video_rtcp_ctx_.rtcp_ctx) {
      free(video_rtcp_ctx_.rtcp_ctx);
      video_rtcp_ctx_.rtcp_ctx = NULL;
    }
    if (audio_rtcp_ctx_.rtcp_ctx) {
      free(audio_rtcp_ctx_.rtcp_ctx);
      audio_rtcp_ctx_.rtcp_ctx = NULL;
    }  }
 
  void RtcpProcessor::SetRemoteSdp(const SdpInfo* remote_sdp) {
    rtcp_sender_->SetRemoteSdp(remote_sdp); 
  }

  void RtcpProcessor::InitWithSsrcs(
      std::vector<uint32_t> extended_video_ssrcs,
      uint32_t default_video_ssrc,
      uint32_t default_audio_ssrc) {
    GenerateNackProcessorsByExtendedSsrcs(extended_video_ssrcs);
    InitRttContextsWithSsrcs(extended_video_ssrcs,
                             default_video_ssrc,
                             default_audio_ssrc);
    rtcp_sender_->InitSrSendHistories(extended_video_ssrcs);
  }

  void RtcpProcessor::GenerateNackProcessorsByExtendedSsrcs(
      std::vector<uint32_t> extended_video_ssrcs) {
    // Generate nack processors for extended video ssrcs
    if (extended_video_ssrcs.empty())
      return;
    for (auto ex_ssrc : extended_video_ssrcs) {
      // Create a new NackProcessor object insert to map
      std::shared_ptr<NackProcessor> 
        nack_processor(new NackProcessor(trans_delegate_, this)); 
      extended_video_nack_processors_[ex_ssrc] = nack_processor;
    }
  }

  void RtcpProcessor::HandleSenderReport(packetType packet_type, 
                                             char* buf, int len) {
    RtcpHeader* rtcp_header = reinterpret_cast<RtcpHeader*>(buf);
    VLOG(3) <<"SR RECV: ssrc = " <<rtcp_header->getSSRC()
            <<" ,type(0=video,1=audio) = " <<packet_type
            <<" ,ntptime = " <<rtcp_header->getNtpTimestamp()
            <<" ,packet send = " <<rtcp_header->getPacketsSent()
            <<" ,bytes send = " <<rtcp_header->getOctetsSent()
  	        <<" ,length = " << rtcp_header->getLength()
            <<" , rc = " << (uint32_t)rtcp_header->getBlockCount();
    // Use SR(sender report) to update the RemoteNTPEstimator.
    UpdateRemoteNTPEstimator(packet_type, rtcp_header);
  }
  
  void RtcpProcessor::LogRtcpReceiverPT(RtcpHeader* rtcp_header, 
                                        packetType packet_type) {
    VLOG(3) 
      <<" **RR RECV: ssrc = " <<rtcp_header->getSSRC()
      <<" ,type(0=video,1=audio) = " <<packet_type
      <<" ,source ssrc = " << rtcp_header->getSourceSSRC()
      <<" ,jitter = " << rtcp_header->getJitter()
      <<" ,lastSr = " << rtcp_header->getLastSr()
      <<" ,DelaySinceLastSr = " << rtcp_header->getDelaySinceLastSr()
      <<" ,fractionLost = " << (int)(rtcp_header->getFractionLost())
      <<" ,lost = " << (unsigned int)rtcp_header->getLostPackets()
      <<" ,highest_seqnumber = " << (unsigned int)rtcp_header->getHighestSeqnum();
  }
  
  std::shared_ptr<NackProcessor>
  RtcpProcessor::GetNackProcessor(uint32_t media_ssrc) {
    auto iter = extended_video_nack_processors_.find(media_ssrc);  
    if (iter != extended_video_nack_processors_.end()) {
      return iter->second;  
    }
    return default_video_nack_processor_;
  }
  
  uint32_t RtcpProcessor::GetRtpfbNackMediaSSRC(char* buf, int len) {
    uint32_t media_ssrc = janus_rtcp_get_fbnack_media_ssrc(buf, len);
    return media_ssrc;
  }
  
  void RtcpProcessor::HandleReceiverReport(packetType packet_type,
                                           char* buf, int len) {
    RtcpHeader* rtcp_header = reinterpret_cast<RtcpHeader*>(buf);

    uint32_t local_ssrc = rtcp_header->getSourceSSRC();

    unsigned int cumulative_lost = 
        (unsigned int)rtcp_header->getLostPackets();
    unsigned int highest_seqnumber = 
        (unsigned int)rtcp_header->getHighestSeqnum();
    unsigned int jitter = (unsigned int)rtcp_header->getJitter();
  
    int64_t rtt;
    int64_t avg_rtt;
    int64_t min_rtt;
    int64_t max_rtt;

    UpdateRTT(buf, len, local_ssrc);
    /* update network status */
    RTT(local_ssrc, &rtt, &avg_rtt, &min_rtt, &max_rtt);
    network_status_->UpdateRTT(rtt, local_ssrc);
  
    switch (packet_type) {
    case VIDEO_PACKET: {
        LogRtcpReceiverPT(rtcp_header, packet_type);
        network_status_->UpdateJitters(CLIENT_REPORT,
                                       true,
                                       jitter,
                                       local_ssrc);
        network_status_->StatisticSendFractionLost(true, 
                                                   highest_seqnumber, 
                                                   cumulative_lost,
                                                   local_ssrc);
        
      }
      break;
    case AUDIO_PACKET: {
      network_status_->UpdateJitters(CLIENT_REPORT,
                                     false,
                                     jitter,
                                     local_ssrc);
      network_status_->StatisticSendFractionLost(false, 
                                                 highest_seqnumber, 
                                                 cumulative_lost,
                                                 local_ssrc);
      }
      break;
    default:
      LOG(INFO) <<" Invalid packet type " << packet_type;
      break;
    }
  }


  /*
   * To check whether the received packet is a key frame ? if it is ,we'll
   * update the nack_processor's current key sequence number value.
   * Update the RTCP context as well.
   */
  void RtcpProcessor::RtcpProcessIncomingRtp(packetType packet_type, 
                                             char* buf, int len) {
    bool is_video = (packet_type == VIDEO_PACKET);
    if (is_video) {
      dataPacket packet;
      packet.comp = 0;
      memcpy(&(packet.data[0]), buf, len);
      packet.length = len;
      RtpHeader* rtp_header = reinterpret_cast<RtpHeader*>(buf);
      uint32_t ssrc = rtp_header->getSSRC();
      uint16_t seqn = rtp_header->getSeqNumber();
      std::shared_ptr<NackProcessor> 
        video_nack_processor = GetNackProcessor(ssrc);
      if (video_nack_processor.get() != NULL) {
        if (IsKeyFramePacket(std::make_shared<dataPacket> (packet))) {
          VLOG(3) <<"-----------key frame seq = " << (int)seqn <<" len =" <<len;
          video_nack_processor->set_get_fir();
        } else {
          bool get_fir = video_nack_processor->get_fir();
          if (!get_fir) {
            long long now = getTimeMS();
            long long last_fir_request = video_nack_processor->last_fir_request();
            if (now - last_fir_request > FIR_PLI_WAIT) {
              // if we have sent a key frame request and not receive any key frame 
              // more than one second , we'll send a FIR/PLI packet again.
              VLOG(3) <<"Resend FIR/PLI packet to remote";
              video_nack_processor->SendFirPacket();
              video_nack_processor->set_last_fir_request(now);
            }
          } // end of if (!get_fir)
        }
      }
    } // end of if (is_video)
    rtcp_receiver_->RtcpProcessIncomingRtp(packet_type, buf,len);
    
    // Parse the RTP header using webrtc utils
    webrtc::WebRtcRTPHeader rtp_header;
    rtp_header_parser_->Parse((const unsigned char*)buf,
                              len, &(rtp_header.header));

    rtp_header.ntp_time_ms = EstimateRemoteNTPTimeMS(packet_type,
                                                     rtp_header.header.timestamp);
    VLOG(4) << "packet_type(0=video,1=audio)=" <<packet_type
            << " seq=" << rtp_header.header.sequenceNumber
            << " ssrc=" << rtp_header.header.ssrc
            << " headerLength=" << rtp_header.header.headerLength
            << " timestamp=" << rtp_header.header.timestamp
            << " ntp_timestamp=" << rtp_header.ntp_time_ms;

    switch(packet_type) {
    case VIDEO_PACKET: {
      video_remote_bitrate_estimator_->IncomingPacket(
          clock_->TimeInMilliseconds(),
          len - rtp_header.header.headerLength,
          rtp_header.header,
          false);
      int64_t time_until_process_ms = 
          video_remote_bitrate_estimator_->TimeUntilNextProcess();
      if (time_until_process_ms <= 0) {
        video_remote_bitrate_estimator_->Process();
      }
      break;
    }
    case AUDIO_PACKET: {
      audio_remote_bitrate_estimator_->IncomingPacket(
          clock_->TimeInMilliseconds(),
          len - rtp_header.header.headerLength,
          rtp_header.header,
          false);
      int64_t time_until_process_ms = 
          audio_remote_bitrate_estimator_->TimeUntilNextProcess();
      if (time_until_process_ms <= 0) {
        audio_remote_bitrate_estimator_->Process();
      }
      break;
    }
    default:
      break;
    }
  }

  void RtcpProcessor::SendVideoNackByCondition(uint32_t ssrc,
                                               uint16_t seqnumber) {
    std::shared_ptr<NackProcessor> video_nack_processor = 
      GetNackProcessor(ssrc);
    if (video_nack_processor.get() == NULL)
      return;
    int res = video_nack_processor->SendNackByCondition(seqnumber, VIDEO_PACKET);
    if (res) {
      network_status_->IncreaseNackCount(true);
    }
  }

  void RtcpProcessor::SendAudioNackByCondition(uint16_t seqnumber) {
    int res = audio_nack_processor_->SendNackByCondition(seqnumber, AUDIO_PACKET);
    if (res) {
      network_status_->IncreaseNackCount(true);
    }
  }

  void RtcpProcessor::SendRTCP(int rtcp_type, packetType media_type,
                               char* rtcp_buf, int len) {
    rtcp_sender_->SendRTCP(rtcp_type, media_type, rtcp_buf, len);
  }

void RtcpProcessor::HandleNackItem(packetType packet_type, char* buf, int len) {
  switch(packet_type) {
  case VIDEO_PACKET: {
    RtcpHeader* rtcp = reinterpret_cast<RtcpHeader*>(buf);
    // Parse RTP-FB header, get media ssrc
    uint32_t media_ssrc = GetRtpfbNackMediaSSRC(buf, len);
    std::shared_ptr<NackProcessor> video_nack_processor = 
      GetNackProcessor(media_ssrc);
    
    if (video_nack_processor.get() == NULL)
      break;
    int nacks_count = video_nack_processor->GetRtcpNackPackets(buf, len);
    if (nacks_count) {
      video_nack_processor->ProcessRtcpNackPackets(buf, len);
      network_status_->IncreaseNackCount(false);
    }
    }
    break;
  case AUDIO_PACKET: {
    int nacks_count = audio_nack_processor_->GetRtcpNackPackets(buf, len);
    if (nacks_count) {
      audio_nack_processor_->ProcessRtcpNackPackets(buf, len);
      network_status_->IncreaseNackCount(false);
    }
    }
    break;
  default:
    LOG(ERROR) <<"Unkown packet type " << packet_type;
    break;
  }
}

void RtcpProcessor::HandlePLI() {
  //network_status_->increase_recv_pli_count();
}

void RtcpProcessor::HandleSLI() {
  //network_status_->increase_recv_sli_count();
}

void RtcpProcessor::HandleFIR() {
  //network_status_->increase_recv_fir_count();
}

void RtcpProcessor::HandleSDES(packetType media_type, char* buf, int len) {
  RtcpHeader* rtcp = reinterpret_cast<RtcpHeader*>(buf);
  uint32_t ssrc = rtcp->getSSRC();
  if (media_type == VIDEO_PACKET) {
    VLOG(3) <<"..SDES, SSRC = " << ssrc;
  }
}

void RtcpProcessor::HandlePSFB(char* buf, int len) {
  RtcpHeader* rtcp_header = reinterpret_cast<RtcpHeader*>(buf);
  int format = rtcp_header->getBlockCount();
  switch(format) {
  case RTCP_PLI_FMT:
    VLOG(3) <<" -----------------------------------------\n"
              <<" received PLI request --------------------";
  break;
  case RTCP_SLI_FMT:
    VLOG(3) <<" -----------------------------------------\n"
              <<" received SLI request --------------------";
  break;
  case RTCP_FIR_FMT:
    VLOG(3) <<" -----------------------------------------\n"
              <<" received FIR request --------------------";
  break;
  default:
    LOG(INFO) <<" Not supported FMT type of PS Feedback packet. " << format;
  break;
  } // end of switch
}

/*
 * Let's process this RTCP (compound?) packet, and update the RTCP context
 * for this stream in case
 */
void RtcpProcessor::RtcpProcessIncomingRtcp(packetType packet_type, 
                                            char* buf,
                                            int len) {
  // Allow receive of non-compound RTCP packets.
  webrtc::RTCPUtility::RTCPParserV2 rtcp_parser((uint8_t*)buf, len, true);

  const bool valid_rtcpheader = rtcp_parser.IsValid();
  if (!valid_rtcpheader) {
    LOG(WARNING) << "Incoming invalid RTCP packet";
    return; 
  }

  webrtc::RTCPUtility::RTCPPacketTypes pktType = rtcp_parser.Begin();
  while (pktType != webrtc::RTCPUtility::RTCPPacketTypes::kInvalid) {
    // Each "case" is responsible for iterate the parser to the
    // next top level packet.
    // TODO there are a lot of RTCP message we're not handle currently.
    switch (pktType) {
    case webrtc::RTCPUtility::RTCPPacketTypes::kSr:
      HandleSenderReport(packet_type, buf, len);
	    break;
    case webrtc::RTCPUtility::RTCPPacketTypes::kRr:
     // HandleSenderReceiverReport(*rtcp_parser, rtcpPacketInformation);
      HandleReceiverReport(packet_type, buf, len);
      break;
    case webrtc::RTCPUtility::RTCPPacketTypes::kSdes:
      VLOG(3) << "received SDES message.";
     // HandleSDES(*rtcp_parser, rtcpPacketInformation);
      HandleSDES(packet_type, buf, len);
      break;
    case webrtc::RTCPUtility::RTCPPacketTypes::kXrHeader:
     // HandleXrHeader(*rtcp_parser, rtcpPacketInformation);
      break;
    case webrtc::RTCPUtility::RTCPPacketTypes::kXrReceiverReferenceTime:
     // HandleXrReceiveReferenceTime(*rtcp_parser, rtcpPacketInformation);
      break;
    case webrtc::RTCPUtility::RTCPPacketTypes::kXrDlrrReportBlock:
     // HandleXrDlrrReportBlock(*rtcp_parser, rtcpPacketInformation);
      break;
    case webrtc::RTCPUtility::RTCPPacketTypes::kXrVoipMetric:
     // HandleXRVOIPMetric(*rtcp_parser, rtcpPacketInformation);
      break;
    case webrtc::RTCPUtility::RTCPPacketTypes::kBye:
      VLOG(3) <<"-----------------Bye Bye Bye Bye ---------------";
     // HandleBYE(*rtcp_parser);
      break;
    case webrtc::RTCPUtility::RTCPPacketTypes::kRtpfbNack:
      // HandleNACK(*rtcp_parser, rtcpPacketInformation);
    case webrtc::RTCPUtility::RTCPPacketTypes::kRtpfbNackItem:
      HandleNackItem(packet_type, buf, len);
      break;
    case webrtc::RTCPUtility::RTCPPacketTypes::kRtpfbTmmbr:
     // HandleTMMBR(*rtcp_parser, rtcpPacketInformation);
      break;
    case webrtc::RTCPUtility::RTCPPacketTypes::kRtpfbTmmbn:
     // HandleTMMBN(*rtcp_parser, rtcpPacketInformation);
      break;
    case webrtc::RTCPUtility::RTCPPacketTypes::kRtpfbSrReq:
     // HandleSR_REQ(*rtcp_parser, rtcpPacketInformation);
      break;
    case webrtc::RTCPUtility::RTCPPacketTypes::kPsfbPli:
     // HandlePLI(*rtcp_parser, rtcpPacketInformation);
     //FIXME THE pli ,sli, fir detect and parse is fault.
      HandlePLI();
      break;
    case webrtc::RTCPUtility::RTCPPacketTypes::kPsfbSli:
     // HandleSLI(*rtcp_parser, rtcpPacketInformation);
      HandleSLI();
      break;
    case webrtc::RTCPUtility::RTCPPacketTypes::kPsfbRpsi:
     // HandleRPSI(*rtcp_parser, rtcpPacketInformation);
      break;
    case webrtc::RTCPUtility::RTCPPacketTypes::kExtendedIj:
     // HandleIJ(*rtcp_parser, rtcpPacketInformation);
      break;
    case webrtc::RTCPUtility::RTCPPacketTypes::kPsfbFir:
     // HandleFIR(*rtcp_parser, rtcpPacketInformation);
      HandleFIR();
      break;
    case webrtc::RTCPUtility::RTCPPacketTypes::kPsfbApp:
     // HandlePsfbApp(*rtcp_parser, rtcpPacketInformation);
      break;
    case webrtc::RTCPUtility::RTCPPacketTypes::kApp:
      // generic application messages
     // HandleAPP(*rtcp_parser, rtcpPacketInformation);
      break;
    case webrtc::RTCPUtility::RTCPPacketTypes::kAppItem:
      // generic application messages
     // HandleAPPItem(*rtcp_parser, rtcpPacketInformation);
      break;
    case webrtc::RTCPUtility::RTCPPacketTypes::kTransportFeedback:
     // HandleTransportFeedback(rtcp_parser, &rtcpPacketInformation);
      break;
    default:
      VLOG(3) <<" Invalid RTCP type (defined by webrtc) : " << (int)pktType;
      rtcp_parser.Iterate();
      break;
    }
    rtcp_parser.Iterate();
    pktType = rtcp_parser.PacketType();
  }
  rtcp_receiver_->RtcpProcessIncomingRtcp(packet_type,buf,len);
}

  void RtcpProcessor::NackPushVideoPacket(char* buf, int len) {
    //Use the packet's ssrc identifier to get the relative nack processor
    RtpHeader* rtp_header = reinterpret_cast<RtpHeader*>(buf);
    uint32_t seq = rtp_header->getSeqNumber();
    uint32_t ssrc = rtp_header->getSSRC();
    VLOG(3) <<"Nack push video packet ,seq = " << seq
              <<", ssrc = " << ssrc;
    std::shared_ptr<NackProcessor> video_nack_processor = GetNackProcessor(ssrc);
    if (video_nack_processor.get() != NULL)
      video_nack_processor->PushPacket(buf, len);
  }

  void RtcpProcessor::NackPushAudioPacket(char* buf, int len) {
    audio_nack_processor_->PushPacket(buf, len);
  }

  void RtcpProcessor::SendRembPacket(uint64_t bitrate) {
    remb_processor_->SendRembPacket(bitrate);
  }

  /*
   * Update bitrate to determine whether should to send an remb ?
   */
  void RtcpProcessor::UpdateBitrate(uint64_t bitrate) {
    remb_processor_->SetBitrate(bitrate);
  }

  void RtcpProcessor::SlowLinkUpdate(unsigned int nacks, int video, int uplink,
                                     uint64_t now) {
    remb_processor_->SlowLinkUpdate(nacks, video, uplink, now);
  }

  int64_t RtcpProcessor::EstimateRemoteNTPTimeMS(packetType packet_type,
                                                 uint32_t timestamp) {
    if (packet_type != VIDEO_PACKET && packet_type != AUDIO_PACKET) {
      return -1;
    }
    RtcpSyncContext* ctx = 
      (packet_type == VIDEO_PACKET) ? &video_sync_ctx_ : &audio_sync_ctx_;
    std::lock_guard<std::mutex> lock(ctx->ntp_mtx_);
    return ctx->ntp_estimator_->Estimate(timestamp);
  }

  bool RtcpProcessor::RTT(uint32_t ssrc,
                          int64_t* rtt,
                          int64_t* avg_rtt,
                          int64_t* min_rtt,
                          int64_t* max_rtt) {
    std::lock_guard<std::mutex> lock(rtt_ctx_mtx_);
    RttContext* rtt_ctx = GetRttContext(ssrc);
    if (rtt_ctx == NULL) {
      return false;
    }
    if (rtt) {
      *rtt = rtt_ctx->RTT;
    }
    if (avg_rtt) {
      *avg_rtt = rtt_ctx->avgRTT;
    }
    if (min_rtt) {
      *min_rtt = rtt_ctx->minRTT;
    }
    if (max_rtt) {
      *max_rtt = rtt_ctx->maxRTT;
    }
    return true;
  }

  bool RtcpProcessor::GetBitrateEstimate(packetType packet_type,
                                         unsigned int* target_bitrate,
                                         std::vector<unsigned int>* ssrcs) {
    switch(packet_type) {
    case VIDEO_PACKET: {
      if (video_remote_bitrate_estimator_->LatestEstimate(ssrcs, target_bitrate)) {
        if (!ssrcs->empty()) {
          *target_bitrate = *target_bitrate / ssrcs->size();
        }
        return true;
      }
      return false;
    }
    case AUDIO_PACKET: { 
      if (audio_remote_bitrate_estimator_->LatestEstimate(ssrcs, target_bitrate)) {
        if (!ssrcs->empty()) {
          *target_bitrate = *target_bitrate / ssrcs->size();
        }
        return true;
      }
      return false;
    }
    default:
      break;
    }
    return false;
  }

  void RtcpProcessor::UpdateRemoteNTPEstimator(packetType packet_type, 
                                               RtcpHeader* rtcp_header) {
    if (packet_type != VIDEO_PACKET && packet_type != AUDIO_PACKET) {
      return;
    }
    RtcpSyncContext* ctx = (packet_type == VIDEO_PACKET) ? 
                           &video_sync_ctx_ : &audio_sync_ctx_;
    // Store the remoteSenderInfo and the packetCount etc.
    ctx->_remoteSenderInfo.NTPseconds = 
        htonl((rtcp_header->report.senderReport.ntptimestamp << 32) >> 32);
    ctx->_remoteSenderInfo.NTPfraction = 
        htonl(rtcp_header->report.senderReport.ntptimestamp >> 32);
    ctx->_remoteSenderInfo.RTPtimeStamp = 
        htonl(rtcp_header->report.senderReport.rtprts);
    ctx->_remoteSenderInfo.sendPacketCount = rtcp_header->getPacketsSent();
    ctx->_remoteSenderInfo.sendOctetCount = rtcp_header->getOctetsSent();
    // Store the lastReceivedSR's NTPsecs and NTPfrac
    clock_->CurrentNtp(ctx->_lastReceivedSRNTPsecs, 
                       ctx->_lastReceivedSRNTPfrac);
    
    int64_t rtt = network_status_->GetLastestRttOfSsrcs();
    if (rtt == 0) {
      // Waiting for valid rtt.
      return;
    }

    uint32_t ntp_secs = 0;
    uint32_t ntp_frac = 0;
    uint32_t rtp_timestamp = 0;
    if (ctx->RemoteNTP(&ntp_secs, &ntp_frac, NULL, NULL, &rtp_timestamp)) {
      VLOG(3) << "Update NTP estimator";
      VLOG(3) << "ntp_estimator_->UpdateRtcpTimestamp(rtt, ntp_secs, ntp_frac, "
              << "rtp_timestamp) rtt=" << rtt
              << " ntp_secs=" << ntp_secs
              << " ntp_frac=" << ntp_frac
              << " rtp_timestamp=" << rtp_timestamp;

      std::lock_guard<std::mutex> lock(ctx->ntp_mtx_);
      ctx->ntp_estimator_->UpdateRtcpTimestamp(rtt, ntp_secs, ntp_frac, rtp_timestamp);
    }
  }

  void RtcpProcessor::UpdateRTT(char* buf,
                                int len,
                                uint32_t local_ssrc) {
    RtcpHeader* rtcp_header = reinterpret_cast<RtcpHeader*>(buf);
    if (rtcp_header && rtcp_header->getPacketType() ==  RTCP_Receiver_PT) {
      uint32_t delaySinceLastSendReport = rtcp_header->getDelaySinceLastSr();
      uint32_t last_sr = rtcp_header->getLastSr();
      assert(rtcp_sender_);
      int64_t sendTimeMS = rtcp_sender_->GetSendTimeOfSendReport(last_sr,
                                                                 local_ssrc);
      VLOG(3) << "LOCAL SSRC = " <<local_ssrc 
              <<"sendTimeMS = " <<(int64_t)sendTimeMS;
      // Estimate RTT
      uint32_t d = (delaySinceLastSendReport & 0x0000ffff) * 1000;
      d /= 65536;
      d += ((delaySinceLastSendReport & 0xffff0000) >> 16) * 1000;
      
      // local NTP time when we received this
      uint32_t lastReceivedRRNTPsecs = 0;
      uint32_t lastReceivedRRNTPfrac = 0;
      
      clock_->CurrentNtp(lastReceivedRRNTPsecs, lastReceivedRRNTPfrac);
      
      // time when we received this in MS
      int64_t receiveTimeMS = webrtc::Clock::NtpToMs(lastReceivedRRNTPsecs,
                                                     lastReceivedRRNTPfrac);
      int64_t RTT = 0;

      // get relative rtt context and update RTT information
      {
      std::lock_guard<std::mutex> lock(rtt_ctx_mtx_);
      RttContext* rtt_ctx = GetRttContext(local_ssrc);
      if (rtt_ctx == NULL)
        return;
      if (sendTimeMS > 0) {
        RTT = receiveTimeMS - d - sendTimeMS; // calculate RTT
        if (RTT <= 0) {
          RTT = 1;
        }
        if (RTT > rtt_ctx->maxRTT) {
          // store max RTT
          rtt_ctx->maxRTT = RTT;
        }
        if (rtt_ctx->minRTT == 0) {
          // first RTT
          rtt_ctx->minRTT = RTT;
        } else if (RTT < rtt_ctx->minRTT) {
          // Store min RTT
          rtt_ctx->minRTT = RTT;
        }
        // store last RTT
        rtt_ctx->RTT = RTT;
       
        // store average RTT
        if (rtt_ctx->numAverageCalcs != 0) {
          float ac = static_cast<float>(rtt_ctx->numAverageCalcs);
          float newAverage =
            ((ac / (ac + 1)) * rtt_ctx->avgRTT) + ((1 / (ac + 1)) * RTT);
          rtt_ctx->avgRTT = static_cast<int64_t>(newAverage + 0.5f);
        } else {
          // first RTT
          rtt_ctx->avgRTT = RTT;
        }
        rtt_ctx->numAverageCalcs++;
        VLOG(3) << "RTT=" << RTT << " maxRTT=" << rtt_ctx->maxRTT << "minRTT" 
	          << rtt_ctx->minRTT << "avgRTT=" << rtt_ctx->avgRTT;
      }
      } // end of lock_guard
    }
  }  
}  // namespace orbit
