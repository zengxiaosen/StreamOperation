/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * transport_delegate.cc
 * ---------------------------------------------------------------------------
 * Implements the TransportDelegate, the webrtc endpoint is going to delegate
 * the operations of managing the  network transportations to this class.
 * ---------------------------------------------------------------------------
 */

#include "transport_delegate.h"
#include "webrtc_endpoint.h"
#include "dtls_transport.h"
#include "network_status.h"
#include "rtp/rtcp_processor.h"
#include "rtp/janus_rtcp_processor.h"

#include "stream_service/orbit/logger_helper.h"
#include "transport_plugin.h"
#include "stream_service/orbit/base/session_info.h"
#include "stream_service/orbit/base/singleton.h"
#include "stream_service/orbit/base/timeutil.h"
#include "stream_service/orbit/stun_server_prober.h"
#include "stream_service/orbit/network_status_common.h"
#include "stream_service/orbit/debug_server/rtp_capture.h"
#include "stream_service/orbit/rtp/janus_rtcp_processor.h"
#include "stream_service/orbit/rtp_sender.h"

#include "gflags/gflags.h"
#include "webrtc/modules/include/module_common_types.h"

#include <glib.h>
#include <string>

using namespace std;

#define UPDATE_BITRATE_STAT_TIMEOUT 1000 // in MS, i.e. 1000ms = 1s

DECLARE_bool(only_public_ip);
DECLARE_bool(enable_red_fec);
DECLARE_string(packet_capture_directory);
DECLARE_string(packet_capture_filename);
DEFINE_bool(setup_passive_server, true,
            "If set, we will act as server and passive method. "
            "Right not it is setup:actpass by default.");
DEFINE_bool(debug_display_rtt, true,
            "If set, we will display RTT every second. For debug.");
namespace orbit {
  TransportDelegate::TransportDelegate(bool audio_enabled, bool video_enabled,
                                       bool trickle_enabled, const IceConfig& ice_config,
                                       int session_id, int stream_id)
    : is_server_mode_(true) {
    audio_enabled_ = audio_enabled;
    video_enabled_ = video_enabled;
    trickle_enabled_ = trickle_enabled;
    ice_config_ = ice_config;
    session_id_ = session_id;
    stream_id_ = stream_id;

    bundle_ = true;

    // FEC related code.
    fec_receiver_.reset(new webrtc::FecReceiverImpl(this));
    fec_.reset(new webrtc::ForwardErrorCorrection());
    producer_.reset(new webrtc::ProducerFec(fec_.get()));
    webrtc::FecProtectionParams params = {15, false, 3, webrtc::kFecMaskRandom};
    producer_->SetFecParameters(&params, 0);  // Expecting one FEC packet.

    current_fec_seqnumber_ = 0;
    fec_send_count_ = 0;
    recv_next_sequence_number_ = 0;

    NetworkStatusManager::Init(session_id, stream_id);
    network_status_ = NetworkStatusManager::Get(session_id, stream_id);
    Init();
  };

  TransportDelegate::TransportDelegate(int session_id, int stream_id)
    : is_server_mode_(false) {
    audio_enabled_ = true;
    video_enabled_ = true;
    trickle_enabled_ = true;
    plugin_ = nullptr;
    video_transport_ = nullptr;
    audio_transport_ = nullptr;
    event_listener_ = nullptr;
    bundle_ = 1;
    session_id_ = session_id;
    stream_id_ = stream_id;
    NetworkStatusManager::Init(session_id, stream_id);
    network_status_ = NetworkStatusManager::Get(session_id, stream_id);
  }

  TransportDelegate::~TransportDelegate() {
    LOG(INFO) <<"Enter into ~TransportDelegate";
    Destroy();
  }

  void TransportDelegate::Destroy() {
    LOG(INFO) <<"Enter into TransportDelegate::Destroy";
    running_ = false;
    if (rtp_sender_ != NULL) {
      delete rtp_sender_;
      rtp_sender_ = NULL;
    }
    if (rtp_capture_ != NULL) {
      delete rtp_capture_;
      rtp_capture_ = NULL;
    }

    if (video_transport_) {
      delete video_transport_;
      video_transport_ = NULL;
    }
    if (audio_transport_) {
      delete audio_transport_;
      audio_transport_ = NULL;
    }
    NetworkStatusManager::Destroy(session_id_, stream_id_);
    LOG(INFO) << "Leave TransportDelegate::Destroy().";
  }

  void TransportDelegate::Init() {
    rtcp_processor_.reset(new RtcpProcessor(this));
    rtp_sender_ = new RtpSender(this);

    if (!FLAGS_packet_capture_filename.empty()) {
      rtp_capture_ = new RtpCapture(FLAGS_packet_capture_directory + FLAGS_packet_capture_filename);
    }


    if (FLAGS_only_public_ip) {
      StunServerProber* prober = Singleton<StunServerProber>::get();
      public_ip_ = prober->GetPublicIP(ice_config_.stunServer,
                                       ice_config_.stunPort);
      ice_config_.publicIp = public_ip_;
      LOG(INFO) << "public_ip=" << public_ip_;
    }
    running_ = true;
  }

  std::vector<uint32_t> TransportDelegate::GetExtendedVideoSsrcs() {
    return local_sdp_.getExtendedVideoSsrcs();  
  }

  void TransportDelegate::InitWithSsrcs(
      std::vector<uint32_t> extended_video_ssrcs,
      uint32_t default_video_ssrc,
      uint32_t default_audio_ssrc) {
    rtcp_processor_->InitWithSsrcs(extended_video_ssrcs,
                                   default_video_ssrc,
                                   default_audio_ssrc);
    network_status_->InitWithSsrcs(extended_video_ssrcs,
                                   default_video_ssrc,
                                   default_audio_ssrc);
  }

  bool TransportDelegate::ProcessOffer(const std::string& offer_sdp,
                                       std::string* answer_sdp) {
    // Extract the video number from client's sdp.
    local_sdp_.ExtractNewCodes(offer_sdp);
    remote_sdp_.ExtractNewCodes(offer_sdp);

    // Before parsing the remote_sdp_, we restricted the video_encoding
    // to those passing from the CreateStreamOption
    remote_sdp_.set_video_encoding(video_encoding_);

    if (!SetRemoteSdp(offer_sdp)) {
      LOG(ERROR) << "ProcessOffer: SetRemoteSdp failed..";
      return false;
    }

    if (min_encoding_bitrate_ != 0) {
      local_sdp_.set_min_bitrate(min_encoding_bitrate_);
    }

    local_sdp_.setOfferSdp(remote_sdp_);
  
    {
      boost::mutex::scoped_lock lock_plugin(plugin_mutex_);
      if (plugin_ != NULL) {
        local_sdp_.initExtendedVideoSsrc(plugin_->GetExtendedVideoSsrcNumber());
        plugin_->SetDownstreamSsrc(local_sdp_.getVideoSsrc());
        plugin_->SetUpstreamSsrc(remote_sdp_.getVideoSsrc());
        plugin_->SetExtendedVideoSsrcs(local_sdp_.getExtendedVideoSsrcs());
      }
    }
    LOG(INFO) << "local videossrc=" << local_sdp_.getVideoSsrc()
              << "local audiossrc=" << local_sdp_.getAudioSsrc()
              << "local bundle=" << local_sdp_.getIsBundle();
    this->AUDIO_SSRC = local_sdp_.getAudioSsrc();
    this->VIDEO_SSRC = local_sdp_.getVideoSsrc();;
    this->VIDEO_RTX_SSRC = local_sdp_.getVideoRtxSsrc();

    InitWithSsrcs(local_sdp_.getExtendedVideoSsrcs(),
                  this->VIDEO_SSRC,
                  this->AUDIO_SSRC);
    rtcp_processor_->SetRemoteSdp(&remote_sdp_);

    bool isServer = false;
    if (FLAGS_setup_passive_server) {
      isServer = true;
      // Setup:passive as server.
      local_sdp_.set_setup("passive");
    }
    
    if (remote_sdp_.getProfile() == SAVPF) {
      if (remote_sdp_.getIsFingerprint()) {
        if (remote_sdp_.getHasVideo() || bundle_) {
          std::string username, password;
          remote_sdp_.getCredentials(username, password, VIDEO_TYPE);
          if (!video_transport_){
            ELOG_DEBUG("Creating videoTransport with creds %s, %s", username.c_str(), password.c_str());
            TransportListener* video_receiver = this;
            video_transport_ = new DtlsTransport(VIDEO_TYPE, "video",
                                                 bundle_, remote_sdp_.getIsRtcpMux(),
                                                 video_receiver, ice_config_ ,
                                                 username, password, isServer);
          }else{ 
            ELOG_DEBUG("UPDATING videoTransport with creds %s, %s", username.c_str(), password.c_str());
            video_transport_->getNiceConnection()->setRemoteCredentials(username, password);
          }
        }
        assert(bundle_);
        if (!bundle_ && remote_sdp_.getHasAudio()) {
          std::string username, password;
          remote_sdp_.getCredentials(username, password, AUDIO_TYPE);
          if (!audio_transport_){
            ELOG_DEBUG("Creating audioTransport with creds %s, %s",
                       username.c_str(), password.c_str());
            TransportListener* audio_receiver = this;
            audio_transport_ = new DtlsTransport(AUDIO_TYPE, "audio",
                                                 bundle_, remote_sdp_.getIsRtcpMux(),
                                                audio_receiver, ice_config_,
                                                username, password, isServer);
          }else{
            ELOG_DEBUG("UPDATING audioTransport with creds %s, %s",
                       username.c_str(), password.c_str());
            audio_transport_->getNiceConnection()->setRemoteCredentials(username, password);
          }
        }
      }
    }

    // The remote_sdp_ may have the ice candidates in the offer_sdp. Set them correctly.
    if (!remote_sdp_.getCandidateInfos().empty()){
      ELOG_DEBUG("There are candidate in the SDP: Setting Remote Candidates");
      if (remote_sdp_.getHasVideo()) {
        video_transport_->setRemoteCandidates(remote_sdp_.getCandidateInfos(), bundle_);
      }
      if (!bundle_ && remote_sdp_.getHasAudio()) {
        audio_transport_->setRemoteCandidates(remote_sdp_.getCandidateInfos(), bundle_);
      }
    }

    /* set varz & statusz */
    if (remote_sdp_.getHasVideo()) {
        // set video ssrc to statusz
      orbit::SessionInfoManager* session_info = Singleton<orbit::SessionInfoManager>::GetInstance();
      orbit::SessionInfoPtr session = session_info->GetSessionInfoById(session_id_);
      if(session != NULL) {
      session->SetStreamCustomInfo(stream_id_,"video_ssrc", remote_sdp_.getVideoSsrc());
      }
    }
    if (remote_sdp_.getHasAudio()) {
      // set audio ssrc to statusz
      orbit::SessionInfoManager* session_info = Singleton<orbit::SessionInfoManager>::GetInstance();
      orbit::SessionInfoPtr session = session_info->GetSessionInfoById(session_id_);
      if(session != NULL) {
        session->SetStreamCustomInfo(stream_id_,"audio_ssrc", remote_sdp_.getAudioSsrc());
      }
    }

    *answer_sdp = GetLocalSdp();
    return true;
  }

  std::string TransportDelegate::GetLocalSdp() {
    ELOG_DEBUG("Getting SDP");
    if (video_transport_ != NULL) {
      video_transport_->processLocalSdp(&local_sdp_);
    }
    if (!bundle_ && audio_transport_ != NULL) {
      audio_transport_->processLocalSdp(&local_sdp_);
    }
    local_sdp_.setProfile(remote_sdp_.getProfile());
    return local_sdp_.getSdp();
  }

  bool TransportDelegate::GetClientLocalSdp(orbit::SdpInfo *sdp) {
    if (!sdp) {
      LOG(ERROR) << "NULL Pointer";
      return false;
    }
    if (is_server_mode_) {
      LOG(ERROR) << "This function is only useable when client mode.";
      return false;
    }
    if (video_transport_ == nullptr && audio_transport_ == nullptr) {
      LOG(ERROR) << "This function is only useable after StartAsClient().";
      return false;
    }

    sdp->createOfferSdp();
    sdp->AddAudioBundleTag();
    sdp->AddVideoBundleTag();
    sdp->setIsFingerprint(true);
    sdp->setFingerprint("58:8B:E5:05:5C:0F:B6:38:28:F9:DC:24:00:8F:E2:A5:52:B6:92:E7:58:38:53:6B:01:1A:12:7F:EF:55:78:6E");
    sdp->set_setup("active");

    std::string video_username, video_password;

    if (video_transport_) {
      video_transport_->GetLocalCredentials(&video_username, &video_password);
      sdp->setCredentials(video_username, video_password, VIDEO_TYPE);
    }

    std::string audio_username, audio_password;

    if (audio_transport_) {
      audio_transport_->GetLocalCredentials(&audio_username, &audio_password);
      sdp->setCredentials(audio_username, audio_password, AUDIO_TYPE);
    }

    return true;
  }

  void TransportDelegate::setPlugin(TransportPlugin* plugin) {
    plugin->SetGateway(this);
    this->plugin_ = plugin;
  }
 
  TransportPlugin* TransportDelegate::getPlugin() {
    return plugin_;
  }

  bool TransportDelegate::AddRemoteCandidate(const std::string &mid,
                                             int mLineIndex,
                                             const std::string &sdp) {
    MediaType theType;
    std::string theMid;
    if ((!mid.compare("video"))||(mLineIndex ==remote_sdp_.getVideoSdpMLine())){
      theType = VIDEO_TYPE;
      theMid = "video";
    }else{
      theType = AUDIO_TYPE;
      theMid = "audio";
    }
    SdpInfo tempSdp;
    std::string username, password;
    remote_sdp_.getCredentials(username, password, theType);
    tempSdp.setCredentials(username, password, OTHER);
    bool res = false;
    if(tempSdp.initWithSdp(sdp, theMid)){
      if (theType == VIDEO_TYPE||bundle_){
        ELOG_DEBUG("Setting VIDEO CANDIDATE" );
        res = video_transport_->setRemoteCandidates(tempSdp.getCandidateInfos(), bundle_);
      } else if (theType==AUDIO_TYPE){
        ELOG_DEBUG("Setting AUDIO CANDIDATE");
        res = audio_transport_->setRemoteCandidates(tempSdp.getCandidateInfos(), bundle_);
      }else{
        ELOG_ERROR("Cannot add remote candidate with no Media (video or audio)");
      }
    }

    for (uint8_t it = 0; it < tempSdp.getCandidateInfos().size(); it++){
      remote_sdp_.addCandidate(tempSdp.getCandidateInfos()[it]);
    }

    // Output the setRemoteIce message to NetworkStatus.
    for (uint8_t it = 0; it < tempSdp.getCandidateInfos().size(); it++) {
      string event_value = orbit::GetCurrentTimeWithMillisecond() + " remoteIce:"+tempSdp.getCandidateInfos()[it].ToDebugString();
      UpdateSessionOrStreamInfo("setRemoteIce", event_value);
    }
    
    return res;
  }

  std::string TransportDelegate::GetJSONCandidate(const std::string& mid,
                                               const std::string& sdp) {
    std::map <std::string, std::string> object;
    object["sdpMid"] = mid;
    object["candidate"] = sdp;
    char lineIndex[1];
    sprintf(lineIndex,"%d",(mid.compare("video")?local_sdp_.getAudioSdpMLine():local_sdp_.getVideoSdpMLine()));
    object["sdpMLineIndex"] = std::string(lineIndex);

    std::ostringstream theString;
    theString << "{";
    for (std::map<std::string, std::string>::iterator it=object.begin(); it!=object.end(); ++it){
      theString << "\"" << it->first << "\":\"" << it->second << "\"";
      if (++it != object.end()){
        theString << ",";
      }
      --it;
    }
    theString << "}";
    return theString.str();
  }

  void TransportDelegate::onNewSelectedPair(CandidatePair pair, Transport *transport) {
    std::ostringstream pairedString;
    pairedString << " orbit ip:" << pair.orbitCandidateIp << " port:" << pair.orbitCandidatePort
                 << "; client ip:" << pair.clientCandidateIp << " port:" << pair.clientCandidatePort;
    UpdateSessionOrStreamInfo("pairedIce",
                              orbit::GetCurrentTimeWithMillisecond() + pairedString.str());
  }

  void TransportDelegate::updateComponentState(std::string component_state) {
    LOG(INFO) << "state:" << component_state;
    if (component_state.compare("FAILED") == 0) {
      LOG(ERROR) << "Component_state=FAILED.....";
    } else if (component_state.compare("READY") == 0) {
      LOG(INFO)  << "Component_state=READY......";
    }
    UpdateSessionOrStreamInfo("candidateStateChanged",
                              orbit::GetCurrentTimeWithMillisecond() + " candidate state change to:"  + component_state);
  }

  void TransportDelegate::onCandidateGatheringDone(Transport *transport) {
    UpdateSessionOrStreamInfo("candidateStateChanged",
                              orbit::GetCurrentTimeWithMillisecond()+" candidate gathering done");
  }

  void TransportDelegate::updateState(TransportState state) {
    LOG(INFO) <<"Enter updateState ------state shoud be 3--,----real value is --" 
              <<state;
    transport_state_ = state;

    if (plugin_) {
      plugin_->OnTransportStateChange(state);
    }

    switch(state) {
    case TRANSPORT_INITIAL:
    case TRANSPORT_STARTED:
      break;
    case TRANSPORT_GATHERED:
      break;
    case TRANSPORT_READY:
      if (plugin_) {
        plugin_->set_active(true);
      }
      CallConnectReadyListeners();
      break;
    case TRANSPORT_FINISHED:
    case TRANSPORT_FAILED:
      break;
    default:
      break;
    }
  }

  void TransportDelegate::onCandidate(const CandidateInfo& cand, Transport *transport) {
    LOG(INFO) << "CandidateReceived." << cand.ToDebugString();
    if (ice_config_.use_relay_mode) {
      if (cand.hostType != RELAY) {
        LOG(INFO) << "Use_relay_mode_ : Filter cand != RELAY.";
        return;
      }
    }
    std::string sdp = local_sdp_.addCandidate(cand);
    CallFindCandidateListeners();
    ELOG_DEBUG("On Candidate %s", sdp.c_str());

    UpdateSessionOrStreamInfo("candidateStateChanged",
                              orbit::GetCurrentTimeWithMillisecond()+" get new candidate");

    if(trickle_enabled_){
      if (event_listener_ != NULL) {
        if (!bundle_) {
          std::string object = this->GetJSONCandidate(transport->transport_name, sdp);
          event_listener_->NotifyEvent(CONN_CANDIDATE, object);
        } else {
          if (remote_sdp_.getHasAudio()){
            std::string object = this->GetJSONCandidate("audio", sdp);
            event_listener_->NotifyEvent(CONN_CANDIDATE, object);
          }
          if (remote_sdp_.getHasVideo()){
            std::string object2 = this->GetJSONCandidate("video", sdp);
            event_listener_->NotifyEvent(CONN_CANDIDATE, object2);
          }
        }
        UpdateSessionOrStreamInfo("collectedIce",
                                  orbit::GetCurrentTimeWithMillisecond() +
                                  " collected ice:"+cand.ToDebugString());
      }
    }
  }

  bool TransportDelegate::isRunning() {
    return running_;
  }

  int TransportDelegate::GetSendPackets(uint32_t ssrc) {
    return network_status_->GetSendPackets(ssrc);
  }

  int TransportDelegate::GetSendBytes(uint32_t ssrc) {
    return network_status_->GetSendBytes(ssrc);
  }

  uint32_t TransportDelegate::GetLastTs(uint32_t ssrc) {
    return network_status_->GetLastTs(ssrc);
  }
  
  olive::ConnectStatus TransportDelegate::GetConnectStatusInfo() {
    return network_status_->GetConnectStatus();  
  }

  void TransportDelegate::onTransportData(char* buf, int len,
                                          Transport *transport) {
    // update stream incomming packet time
    NetworkStatusManager::UpdateStreamIncommingTime(session_id_, stream_id_);
    
    uint32_t recvSSRC;
    // PROCESS RTCP
    RtcpHeader* chead = reinterpret_cast<RtcpHeader*>(buf);

    if (chead->isRtcp()) {
      // RTP or RTCP Sender Report
      recvSSRC = chead->getSSRC();
      boost::mutex::scoped_lock lock_plugin(plugin_mutex_);
      {
        if (plugin_ != NULL) {
          dataPacket packet;
          packet.comp = 0;
          memcpy(&(packet.data[0]), buf, len);
          packet.length = len;
          if (recvSSRC == remote_sdp_.getAudioSsrc()) {
            packet.type = AUDIO_PACKET;
            network_status_->ReceivingRTCPPacket(false);
          } else if (recvSSRC == remote_sdp_.getVideoSsrc()) {
            packet.type = VIDEO_PACKET;
            network_status_->ReceivingRTCPPacket(true);
          } else if (recvSSRC == remote_sdp_.getVideoRtxSsrc()) {
            packet.type = VIDEO_RTX_PACKET;
          } else {
            // HACK(zhanghao): this fix is for supervise class page, because the sdp
            // of supervise class page has no ssrc, we could not distinguish the stream.
            // So if the stream is RECVONLY, there is no SSRC, But the RTCP packet
            // is valid. So we should not return.
            if (remote_sdp_.GetVideoDirection() == RECVONLY) {
              packet.type = VIDEO_PACKET;
            } else {
              LOG(ERROR) << "recvSSRC=" << recvSSRC << " invalid SSRC...----- RTCP";
              return;
            }
          }
          rtcp_processor_->RtcpProcessIncomingRtcp(packet.type, buf, len);
          plugin_->IncomingRtcpPacket(packet);
          CallRecvRtcpListeners(packet);

          if (rtp_capture_ != NULL) {
            intptr_t transport_address = reinterpret_cast<intptr_t>(this);
            rtp_capture_->CapturePacket(transport_address, packet);
          }
        }
      }
    } else {
      VLOG(3) << "This is a RTP data packets.....";
      if (bundle_) {
        RtpHeader *head = reinterpret_cast<RtpHeader*> (buf);
        recvSSRC = head->getSSRC();
      }
      boost::mutex::scoped_lock lock_plugin(plugin_mutex_);
      if (plugin_ != NULL) {
        dataPacket packet;
        packet.comp = 0;
        memcpy(&(packet.data[0]), buf, len);
        packet.length = len;
        RtpHeader* rtp_header = reinterpret_cast<RtpHeader*> (buf); 
        uint16_t seqnumber = rtp_header->getSeqNumber();
        if (recvSSRC == remote_sdp_.getAudioSsrc()) {
          packet.type = AUDIO_PACKET;
          network_status_->ReceivingPacket(false);
          network_status_->StatisticRecvFractionLost(false, seqnumber);
        } else if (recvSSRC == remote_sdp_.getVideoSsrc()) {
          packet.type = VIDEO_PACKET;
          network_status_->ReceivingPacket(true);
          network_status_->StatisticRecvFractionLost(true, seqnumber);
        } else if (recvSSRC == remote_sdp_.getVideoRtxSsrc()) {
          packet.type = VIDEO_RTX_PACKET;
        } else {
          LOG(ERROR) << "recvSSRC=" << recvSSRC << " invalid SSRC...----- RTP";
        }
        
        if (FLAGS_enable_red_fec) {
          ReceivePacketAsFec(&packet);
        } else {            //enable_red_fec == false
          if (packet.type == VIDEO_PACKET) {
            rtcp_processor_->SendVideoNackByCondition(recvSSRC, seqnumber);
          }
          ProcessPacket(packet.type, buf, len);
          SendPacketToPlugin(packet);
          CallRecvRtpListeners(packet);
        }

        if (packet.type == AUDIO_PACKET) {
          rtcp_processor_->SendAudioNackByCondition(seqnumber);
        }
      } // end of (plugin_ != NULL)
    } // end of else

    // The data has available. should update the network status etc.
    UpdateNetworkStatus();
  }

  // This is called by our fec_ object when it recovers a packet.
  bool TransportDelegate::OnRecoveredPacket(const uint8_t* rtp_packet, 
                                            size_t rtp_packet_length) {
    /* Enqueue it */
    dataPacket packet;
    packet.comp = 0;
    packet.length = rtp_packet_length;
    packet.type = VIDEO_PACKET;
    memcpy(&(packet.data[0]), rtp_packet, rtp_packet_length);
    
    RtpHeader* h = reinterpret_cast<RtpHeader*>(&(packet.data[0]));
    uint16_t seqnumber = h->getSeqNumber();
    uint32_t recvSSRC = h->getSSRC();
    VLOG(3) <<" Recovered Video Packet , seq = " << (int)h->getSeqNumber()
            <<" Timestamp = " << (unsigned int)h->getTimestamp()
            <<" rtp_packet_length = " <<(int)rtp_packet_length;

    rtcp_processor_->SendVideoNackByCondition(recvSSRC, seqnumber);

    ProcessPacket(packet.type, &(packet.data[0]), rtp_packet_length);
    SendPacketToPlugin(packet);
    CallRecvRtpListeners(packet);
    return true;
  }

  int32_t TransportDelegate::OnReceivedPayloadData(
      const uint8_t* /*payload_data*/, 
      const size_t /*payload_size*/, 
      const webrtc::WebRtcRTPHeader* /*rtp_header*/) {
    // Unused by WebRTC's FEC implementation; just something we have to implement.
    return 0;
  }

  void TransportDelegate::ProcessPacket(packetType packet_type, 
                                        char* buf, int len) {
    UpdateReceiverBitrate(len);
    rtcp_processor_->RtcpProcessIncomingRtp(packet_type, buf, len);
  }

  void TransportDelegate::RelayPacket(const dataPacket& packet) {
    assert(bundle_);
    if (video_transport_ != NULL) {
      queueData(0, &(packet.data[0]), packet.length, video_transport_, packet.type);
    }
  }

  void TransportDelegate::SendPacketToPlugin(dataPacket packet) {
    RtpHeader* rtp_header = reinterpret_cast<RtpHeader*>(&(packet.data[0])); 
    packet.rtp_timestamp = rtp_header->getTimestamp();
    packet.arrival_time_ms = GetCurrentTime_MS();
    packet.remote_ntp_time_ms =
        rtcp_processor_->EstimateRemoteNTPTimeMS(packet.type, packet.rtp_timestamp);
    plugin_->IncomingRtpPacket(packet);

    if (rtp_capture_ != NULL) {
      intptr_t transport_address = reinterpret_cast<intptr_t>(this);
      rtp_capture_->CapturePacket(transport_address, packet);
    }
  }

  /*
   * If the enable_red_fec flag is set to true, and the incoming packet is a RED
   * packet, we'll put it into the red/fec processor. Otherwise, if the incoming
   * packet is audio or just a vp8 video packet, we'll send it to plgins
   * directly. And in this function , we also send the packet into nack
   * processor.
   */
  void TransportDelegate::ReceivePacketAsFec(dataPacket* packet) {
    char* buf = packet->data;
    int len = packet->length;
    packetType packet_type = packet->type;
    RtpHeader* h = reinterpret_cast<RtpHeader*>(buf);
    uint16_t seqnumber = h->getSeqNumber();   
    uint32_t recvSSRC = h->getSSRC();
 
    switch(packet_type) {
    case VIDEO_PACKET: {
      switch(h->getPayloadType()) {
      case RED_90000_PT: {
        // put fec packet into nack processor
        RedHeader* red_header = reinterpret_cast<RedHeader*>(&buf[12]);
        if (red_header->payloadtype == ULP_90000_PT) {
          rtcp_processor_->SendVideoNackByCondition(recvSSRC, seqnumber);
        }
        // This is a RED/FEC payload, but our remote endpoint doesn't support
        // that (most likely because it's firefox :/ )
        // Let's go ahead and run this through our fec receiver to convert it
        // to raw VP8
        webrtc::RTPHeader hackyHeader;
        hackyHeader.headerLength = h->getHeaderLength();
        hackyHeader.sequenceNumber = h->getSeqNumber();
        // FEC copies memory, manages its own memory, including memory passed
        // in callbacks (in the callback, be sure to memcpy out of webrtc's buffers
        if (fec_receiver_->AddReceivedRedPacket(
            hackyHeader, (const uint8_t*)buf, len, ULP_90000_PT) == 0) {
          fec_receiver_->ProcessReceivedFec();
        }
      }
      break;
      case VP8_90000_PT: {
        VLOG(3) <<"###### vp8 packet , seqn = " 
          << (int)h->getSeqNumber() <<" ,pt = " <<(int)h->getPayloadType()
          <<" , len = " << len;
        // send to plugin directly.
        rtcp_processor_->SendVideoNackByCondition(recvSSRC, seqnumber);
        ProcessPacket(packet_type, buf, len);
        SendPacketToPlugin(*packet);
        CallRecvRtpListeners(*packet);
      }
      break;
      default:
      break; 
      }
    }
    break;
    case AUDIO_PACKET: {
      ProcessPacket(packet_type, buf, len);
      SendPacketToPlugin(*packet);
      CallRecvRtpListeners(*packet);
    }
    break;
    default:
    break;
    }
  }

  void TransportDelegate::SendPacketAsFec(char* buf, int len, Transport *transport) {
    // Protect using RED packet
    RtpHeader* rtp = reinterpret_cast<RtpHeader*>(buf);
    VLOG(3) <<"********------- prev seqnumber = " << rtp->getSeqNumber();
    /* Reset the sequence number , to filter the fec packet's sequence number */
    ResetSequenceNumberNotContainFec(buf);
    uint16_t next_seqnumber = GenerateNextFecSequenceNumber(false, buf);
    VLOG(3) <<"**************** next red fec seqnumber = " << next_seqnumber <<" , " << rtp->getSeqNumber();
    
    const uint8_t kRedPayloadType = 116;
    const uint8_t kFecPayloadType = 117;

    rtc::scoped_ptr<webrtc::RedPacket> red_packet;  
    red_packet.reset(producer_->BuildRedPacket(
       reinterpret_cast<const uint8_t*>(buf), len - 12, 12, kRedPayloadType));

    producer_->AddRtpPacketAndGenerateFec(
        reinterpret_cast<const uint8_t*>(buf), len - 12, 12);

    int red_len = red_packet->length();
    unsigned char* red_buf = reinterpret_cast<unsigned char*>(malloc(red_len));
    memcpy(red_buf, red_packet->data(), red_packet->length());

    // Delete the buf
    free(buf);

    buf = (char*) red_buf;
    len = red_len;
    uint16_t num_fec_packets = producer_->NumAvailableFecPackets();
    if (num_fec_packets > 0) {
      uint16_t next_fec_sequence_number = GenerateNextFecSequenceNumber(true, NULL);
      std::vector<webrtc::RedPacket*> fec_packets;
      fec_packets = producer_->GetFecPackets(kRedPayloadType, kFecPayloadType,
                                             next_fec_sequence_number, 12);
      for (webrtc::RedPacket* fec_packet : fec_packets) {
        rtcp_processor_->NackPushVideoPacket((char*)fec_packet->data(), fec_packet->length());
        const RtpHeader* rtp = reinterpret_cast<const RtpHeader*>((unsigned char*)fec_packet->data());
        VLOG(3) <<" fec packet's send seqnumber = " << rtp->getSeqNumber();
        WriteAndSend(transport, DEFAULT_PRIORITY, (unsigned char*)fec_packet->data(), fec_packet->length());
        delete fec_packet;
      }
      fec_send_count_ += num_fec_packets - 1;
      VLOG(3) << "FEC num_fec_packets...." << num_fec_packets <<" ,fec_send_count_ = " << fec_send_count_;
    }
  }

  void TransportDelegate::UpdateNetworkStatus() {
    if (last_update_ns_time_ == 0) {
      last_update_ns_time_ = GetCurrentTime_MS();
    }

    long long now = GetCurrentTime_MS();
    if (now - last_update_ns_time_ > 3000) {
      last_update_ns_time_ = now;
      int64_t rtt = network_status_->GetAllSsrcsAverageRtt();
      if (rtt > 0) {
        if (FLAGS_debug_display_rtt) {
          VLOG(3) << " rtt=" << rtt;
        }
        UpdateSessionOrStreamInfo("rtt_data",
                                  std::to_string(rtt));
      }
      packetType packet_types[] = {VIDEO_PACKET, AUDIO_PACKET};
      for (auto packet_type : packet_types) {
        uint32_t target_bitrate;
        std::vector<unsigned int> ssrcs;
        if (rtcp_processor_->GetBitrateEstimate(packet_type, &target_bitrate, &ssrcs)) {
          if (FLAGS_debug_display_rtt) {
            VLOG(3) << "target_bitrate=" << target_bitrate;
            VLOG(3) << "ssrcs_size=" << ssrcs.size();
            if (ssrcs.size() >= 1) {
              VLOG(3) << "ssrc[0]=" << ssrcs[0];
            }
          }
          switch(packet_type) {
          case VIDEO_PACKET:
            network_status_->UpdateTargetBitrate(true, target_bitrate);
            break;
          case AUDIO_PACKET:
            network_status_->UpdateTargetBitrate(false, target_bitrate);
            break;
          default:
            LOG(ERROR) <<" Invalid packet type !"; 
            break;
          }
        }
      } // end of for (auto packet_type ...)
    }
  }

  void TransportDelegate::queueData(int comp, const char *data, int len, 
                                    packetType type) {
    assert(bundle_);
    if (video_transport_) {
      queueData(comp, data, len, video_transport_, type);
    }
  }

  void TransportDelegate::queueData(int comp, const char* data, int len,
                                    Transport *transport, packetType type) {
    // HACK(chengxu): rewrite the following logic.
    // Rewrite the RTP header.
    char* buf = reinterpret_cast<char*>(malloc(len));
    memcpy(buf, data, len);

    // When relay the packet, we should reset the codec
    ResetRelayPacketCodec(buf, type);

    RtcpHeader* rtcp = reinterpret_cast<RtcpHeader*>(buf);
    RtpHeader* rtp = reinterpret_cast<RtpHeader*>(buf);    
    if (rtcp->isRtcp()) {
      VLOG(3) << "ssrc=" << rtcp->getSSRC();
      VLOG(3) << "source_ssrc=" << rtcp->getSourceSSRC();
      
      if (type == AUDIO_PACKET) {
        janus_rtcp_fix_ssrc(NULL, buf, len, 1, AUDIO_SSRC,
                            remote_sdp_.getAudioSsrc());
        VLOG(3) <<" AUDIO source ssrc = " << remote_sdp_.getAudioSsrc();
        network_status_->SendingRTCPPacket(false);
      } else if (type == VIDEO_PACKET) {
        // FIXME some rtcp packets used the VIDEO_SSRC as local ssrc, like RR
        // messages, but other rtcp packets such as SR do not need to fix ssrc
        // The function 'janus_rtcp_fix_ssrc()' should be called by RTCP packet
        // construct module , not in here.
        uint32_t packet_type = rtcp->getPacketType();
        if (packet_type != RTCP_Sender_PT) {
          janus_rtcp_fix_ssrc(NULL, buf, len, 1, VIDEO_SSRC,
                              remote_sdp_.getVideoSsrc());
        }
        network_status_->SendingRTCPPacket(true);
      } else if (type == VIDEO_RTX_PACKET) {
        janus_rtcp_fix_ssrc(NULL,buf, len, 1, VIDEO_RTX_SSRC,
                            remote_sdp_.getVideoRtxSsrc());
      }
      
      uint32_t packet_type = rtcp->getPacketType();
      if (packet_type == RTCP_Receiver_PT) {
      //check RR content, the source ssrc is 0 is right, because it's not fixed.
      VLOG(3) <<" type = " << type 
              <<" RR Send: ssrc = " <<rtcp->getSSRC()
              <<" ,source ssrc = " << rtcp->getSourceSSRC()
              <<" ,jitter = " << rtcp->getJitter()
              <<" ,lastSr = " << rtcp->getLastSr()
              <<" ,DelaySinceLastSr = " << rtcp->getDelaySinceLastSr()
              <<" ,fractionLost = " << (int)(rtcp->getFractionLost());
      }
      if (packet_type == RTCP_Sender_PT) {
        VLOG(3) <<" type = " << type
            <<" SR send: ssrc = " <<rtcp->getSSRC()
            <<" , ntptimestamp = " << rtcp->getNtpTimestamp()
            <<" , packets sent = " << rtcp->getPacketsSent()
            <<" , bytes sent = " << rtcp->getOctetsSent()
            <<" , rc = " << (uint32_t)rtcp->getBlockCount();
      }

      if (type == REMB_PACKET) {
        WriteAndSend(transport, REMB_PRIORITY, (unsigned char*)buf, len);
      } else {
        WriteAndSend(transport, RTCP_PRIORITY, (unsigned char*)buf, len);
      }
    } else {
      guint32 timestamp = rtp->getTimestamp();

      if (type == AUDIO_PACKET) {
        rtp->setSSRC(AUDIO_SSRC);
        network_status_->SendingPacket(false, AUDIO_SSRC, len, timestamp);
      } else if (type == VIDEO_PACKET) {
        if (should_rewrite_ssrc_) { // for audio_conference_room case
          rtp->setSSRC(VIDEO_SSRC);
        }
        uint32_t local_ssrc = rtp->getSSRC();
        network_status_->SendingPacket(true, local_ssrc, len, timestamp);
      } else if (type == VIDEO_RTX_PACKET) {
        rtp->setSSRC(VIDEO_RTX_SSRC);
        LOG(INFO) <<"it's VIDEO_RTX_PACKET.";
      }
      // manage retransmit of video packets and audio packets
      if (type == VIDEO_PACKET) {    // whether retransmit packet should be send as fec packet too ?
        if (FLAGS_enable_red_fec) {
          // Disable sending FEC for now
          //SendPacketAsFec(buf, len, transport);
        } // FLAGS_enable_red_fec */
        rtcp_processor_->NackPushVideoPacket(buf, len);
      } else if (type == AUDIO_PACKET) {
        rtcp_processor_->NackPushAudioPacket(buf, len);
      }

      // specify priority and send packet
      int priority = DEFAULT_PRIORITY ;
      if (type == AUDIO_PACKET) {
        priority = AUDIO_PRIORITY;
      } else if (type == VIDEO_PACKET) {
        priority = DEFAULT_PRIORITY ;
      } else if (type == VIDEO_RTX_PACKET) {
        priority = VIDEO_RTX_PRIORITY;
      } else if (type == RETRANSMIT_PACKET) {
        priority = RETRANSMIT_PRIORITY;
      }

      WriteAndSend(transport, priority, (unsigned char*)buf, len);
    }
    free(buf);
  }

  void TransportDelegate::WriteAndSend(Transport* transport, int priority, 
                                       unsigned char* data, int len) {
    rtp_sender_->WriteAndSend(transport, priority, data, len);
  } 

  void TransportDelegate::UpdateSessionOrStreamInfo(const string& event_key,
                                                    const string& event_value) {
    int currentSessionId = session_id();
    int currentStreamId = stream_id();
    SessionInfoManager* session_info = Singleton<SessionInfoManager>::GetInstance();
    SessionInfoPtr session = session_info->GetSessionInfoById(currentSessionId);
    
    if(session != NULL) {
      session->AppendStreamCustomInfo(currentStreamId, event_key, event_value);
    }
  }

  void TransportDelegate::UpdateReceiverBitrate(int len) {
    long cur = getTimeMS();
    boost::mutex::scoped_lock(rember_mutex_);
    if (cur - rb_last_time_ > UPDATE_BITRATE_STAT_TIMEOUT) {
      // Update the stats.
      rb_last_time_ = cur;
      receiver_bitrate_last_ = receiver_bitrate_;
      receiver_bitrate_ = 0;
      // Output the debugging information
      VLOG(3) << "rb_last_time=" << cur
              << " receiver_bitrate_last_=" << receiver_bitrate_last_
              << " receiver_bitrate_=" << receiver_bitrate_;
      network_status_->UpdateStatusZ();
    }
    receiver_bitrate_ += len * 8;
    network_status_->UpdateBitrate(false ,receiver_bitrate_last_);
    rtcp_processor_->UpdateBitrate(receiver_bitrate_last_);
  }

  void TransportDelegate::UpdateSenderBitrate(int len) {
    long cur = getTimeMS();
    {
      boost::mutex::scoped_lock(rember_mutex_);
      if (cur - sb_last_time_ > UPDATE_BITRATE_STAT_TIMEOUT) {
        // Update the stats.
        sb_last_time_ = cur;
        sender_bitrate_last_ = sender_bitrate_;
        sender_bitrate_ = 0;
        // Output the debugging information
        VLOG(3) << "sb_last_time_" << cur
                << " sender_bitrate_last_=" << sender_bitrate_last_
                << " sender_bitrate_=" << sender_bitrate_;
        network_status_->UpdateStatusZ();
      }
      sender_bitrate_ += len * 8;
    }
    network_status_->UpdateBitrate(true, sender_bitrate_last_);
  }

  bool TransportDelegate::SetRemoteSdp(const string& sdp) {
    ELOG_DEBUG("Set Remote SDP %s", sdp.c_str());
    bool ret = remote_sdp_.initWithSdp(sdp, "");
    if (ret) {
      bundle_ = remote_sdp_.getIsBundle();
      ELOG_DEBUG("Is bundle? %d", bundle_);
      assert(bundle_);
      
      ELOG_DEBUG("Video %d videossrc %u Audio %d audio ssrc %u Bundle %d",
                 remote_sdp_.getHasVideo(), remote_sdp_.getVideoSsrc(),
                 remote_sdp_.getHasAudio(), remote_sdp_.getAudioSsrc(),
                 bundle_);
    }
    return ret;
  }

  bool TransportDelegate::SetRemoteCandidates(std::vector<CandidateInfo> &candidates) {
    assert(bundle_);
    if (bundle_) {
      if (video_transport_) {
        return video_transport_->setRemoteCandidates(candidates, true);
      }
    } else {
      if (video_transport_) {
        return video_transport_->setRemoteCandidates(candidates, false);
      }
      if (audio_transport_) {
        return audio_transport_->setRemoteCandidates(candidates, false);
      }
    }
    return true;
  }

  void TransportDelegate::StartAsClient() {
    bool is_server = true;
    audio_transport_ = nullptr;
    video_transport_ = new DtlsTransport(VIDEO_TYPE, "video",
                                         true, true, this, ice_config_,
                                         "", "", is_server);

    fec_receiver_.reset(new webrtc::FecReceiverImpl(this));
    fec_.reset(new webrtc::ForwardErrorCorrection());
    producer_.reset(new webrtc::ProducerFec(fec_.get()));
    webrtc::FecProtectionParams params = {15, false, 3, webrtc::kFecMaskRandom};
    producer_->SetFecParameters(&params, 0);  // Expecting one FEC packet.

    current_fec_seqnumber_ = 0;
    fec_send_count_ = 0;
    recv_next_sequence_number_ = 0;

    vector<unsigned int> extended_video_ssrcs;
    Init();
    InitWithSsrcs(extended_video_ssrcs,
                  this->VIDEO_SSRC,
                  this->AUDIO_SSRC);

    LOG(INFO) << "TransportDelegate started as client.";
  }

  void TransportDelegate::Stop() {
    boost::mutex::scoped_lock lock_plugin(plugin_mutex_);
    plugin_ = NULL;
  }

  void TransportDelegate::ResetSequenceNumberNotContainFec(char* buf) {
    /* Set the sequence number of the RTP packet (buf) */
    RtpHeader* h = reinterpret_cast<RtpHeader*>(buf);
    h->setSeqNumber(recv_next_sequence_number_++);
    return ;
  }

  uint16_t TransportDelegate::GenerateNextFecSequenceNumber(bool is_fec, 
                                                            char* buf) {
    if (is_fec) {
      fec_send_count_++;
      return (current_fec_seqnumber_+1);
    } else {
      /* Get the sequence number of the RTP packet (buf) */
      RtpHeader* h = reinterpret_cast<RtpHeader*>(buf);
      uint16_t sequence_number = h->getSeqNumber();
      current_fec_seqnumber_ = sequence_number + fec_send_count_;
      h->setSeqNumber(current_fec_seqnumber_);
      return current_fec_seqnumber_;
    }
  }

  void TransportDelegate::ResetRelayPacketCodec(char* data, packetType type) {
    RtcpHeader* h = reinterpret_cast<RtcpHeader*>(data);
    if (type == VIDEO_PACKET && !h->isRtcp()) {
      RtpHeader* rtp = reinterpret_cast<RtpHeader*>(data);
      int payload = rtp->getPayloadType();
      int new_codec = local_sdp_.GetCodec(payload);
      if (payload != new_codec) {
        rtp->setPayloadType(new_codec);
      }
    }
  }

}  // namespace orbit
