/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * transport_manager.h
 * ---------------------------------------------------------------------------
 * Defines the abstract interface for managing the transportation.
 * ---------------------------------------------------------------------------
 */
#pragma once

#include "stream_service/orbit/rtp/rtp_headers.h"
#include "stream_service/orbit/common_def.h"

#include "webrtc/modules/rtp_rtcp/include/fec_receiver.h"
#include "webrtc/modules/rtp_rtcp/source/fec_receiver_impl.h"
#include "webrtc/modules/rtp_rtcp/source/producer_fec.h"
#include "webrtc/modules/rtp_rtcp/source/forward_error_correction.h"

// For RTP capture class and debug server
#include "stream_service/orbit/debug_server/rtp_capture.h"

#include "sdp_info.h"
#include "nice_connection.h"
#include "transport.h"

#include <vector>
#include <functional>
#include <set>
#include <map>
#include <list>
#include <atomic>
#include <chrono>
#include <mutex>
#include <condition_variable>


namespace orbit {
  // Forward declarations
 class Transport;
 class WebRtcEndpoint;
 class WebRtcEndpointEventListener;
 class RtcpProcessor;
 class RtpSender;
 class TransportPlugin;
 class dataPacket;
 class NetworkStatus;
 class PacketReplayDriver;         // For replaying the rtp packets.

class TransportDelegate : public TransportListener,
                          public webrtc::RtpData {
 public:
  // Construct a server side TransportDelegate.
  TransportDelegate(bool audio_enabled, bool video_enabled,
                    bool trickle_enabled, const IceConfig& ice_config,
                    int session_id, int stream_id);

  // Construct a client side TransportDelegate.
  TransportDelegate(int session_id, int stream_id);

  virtual ~TransportDelegate();

  // Overrides interfaces from TransportListener
  virtual void onTransportData(char* buf, int len, Transport* transport) override;

  // Send some type of data to remote side.
  // This function is normally used by client side.
  void queueData(int comp, const char *data, int len, packetType type);

  // Send RTP or RTCP formated data to remote side.
  virtual void queueData(int comp, const char* data, int len,
                         Transport *transport, packetType type) override;

  /**
   * The method is used to update the nice state of the current stream
   * connection, therefor the transport_delegate and plugins of this stream will
   * do something by condition.
   *
   * @param[in] state : the transport state of the current stream.
   */
  virtual void updateState(TransportState state) override;

  virtual void onCandidate(const CandidateInfo& cand, Transport *transport) override;

  /**
   * The method is used to update the component state of the current stream
   * connection, only used for Statusz page update information purpose.
   *
   * @param[in] component_state : the lastest state change of the connection.
   */
  virtual void updateComponentState(std::string component_state) override;
  virtual void onNewSelectedPair(CandidatePair pair, Transport *transport) override;
  virtual void onCandidateGatheringDone(Transport *transport) override;

  // Overrides interfaces from webrtc::RtpData
  // The callback frunction when the FEC module receives the payload data.
  int32_t OnReceivedPayloadData(const uint8_t* /*payload_data*/,
                                const size_t /*payload_size*/,
                                const webrtc::WebRtcRTPHeader* /*rtp_header*/) override;
  // The callback function when the packet has been recovered.
  bool OnRecoveredPacket(const uint8_t* rtp_packet, size_t rtp_packet_length) override;

  // Interfaces with WebRtcEndpoint class.
  inline void setWebRtcEndpointEventListener(
     WebRtcEndpointEventListener* listener) {
    this->event_listener_ = listener;
  }
  inline WebRtcEndpointEventListener* getWebRtcEndpointEventListener() {
    return event_listener_;
  }

  bool SetRemoteCandidates(std::vector<CandidateInfo> &candidates);

  // Start the transport_delegate as a client side.
  void StartAsClient();

  // Stop the transport_delegate, and will not forward/relay any data further.
  void Stop();
  // Returns if the delegate is still running, not stopped.
  bool isRunning();
  // Returns if the delegate is ready for transportation.
  TransportState getTransportState() {
    return transport_state_;
  }

  // Relay the packet to the endpoint.
  void RelayPacket(const dataPacket& packet);

  // WebRtc connection establishment functions:
  // Typically a webrtc endpoint is created with a transport_delegate. The
  // connection could be established via the following functions:
  //   delegate_->ProcessOffer(offer_sdp, &answer_sdp); // Send the answer_sdp to endpoint.
  //   delegate->AddRemoteCandidate(candidates); // Get the remote candidates and add to delegate.
  // The connection will be established with the Ice and DtlsTransport with a callback above.
  //   callback 'updateState()' will update the state to ICE_CONNECTED, and the callback
  //   'onTransportData()' will start to transport the media and RTP data.
  bool ProcessOffer(const std::string& offer_sdp,
                    std::string* answer_sdp);
  bool AddRemoteCandidate(const std::string &mid,
                          int mLineIndex,
                          const std::string &sdp);
  /*
   * The following fields are related to local/remote SDP.
   *  NOTE(chengxu): note that in our current implementation, webrtc is always established
   *  from the client. And we act as a server as a passive party (answerer side).
   *  Thus, this function is not useful in current case.
   * @return true if success , or false if error.
   */
  bool SetRemoteSdp(const std::string& sdp);


  // Each TransportDelegate is associated with a plugin. The plugin is responsible for 
  //   - Consume the media data from the endpoint.
  //   - Produe the media data to the endpoint.
  // See TransportPlugin (abstract class) and different implementation: EchPlugin,
  //  AudioConferencePlugin and VideoMixerPlugin etc.
  void setPlugin(TransportPlugin* plugin);
  TransportPlugin* getPlugin();

  std::string GetLocalSdp();

  // Get SDP for client side.
  bool GetClientLocalSdp(orbit::SdpInfo *sdp);

  void GetClientLocalCandidateInfos(std::vector<CandidateInfo> *vec) {
    if (vec) {
      local_sdp_.copyCandidateInfos(vec);
    } else {
      LOG(ERROR) << "NULL Pointer";
    }
  }
  // When new ICE candidate found, call listener.
  void AddFindCandidateListener(std::function<void(void)> listener) {
    std::lock_guard<std::mutex> lock(find_candidate_listeners_mutex_);
    find_candidate_listeners_.push_back(listener);
  }
  // When ICE Connect ready, call listener, we can begin to send data to remote side.
  void AddConnectReadyListener(std::function<void(void)> listener) {
    std::lock_guard<std::mutex> lock(connect_ready_listeners_mutex_);
    connect_ready_listeners_.push_back(listener);
  }
  // When a RTP packet received, call listener.
  void AddRecvRtpListener(std::function<void(const dataPacket &)> listener) {
    std::lock_guard<std::mutex> lock(recv_rtp_listeners_mutex_);
    recv_rtp_listeners_.push_back(listener);
  }
  // When a RTCP packet received, call listener.
  void AddRecvRtcpListener(std::function<void(const dataPacket &)> listener) {
    std::lock_guard<std::mutex> lock(recv_rtcp_listeners_mutex_);
    recv_rtcp_listeners_.push_back(listener);
  }

  inline int session_id() const {
    return session_id_;
  };
  
  inline int stream_id() const {
    return stream_id_;
  }

  olive::ConnectStatus GetConnectStatusInfo();

  std::vector<uint32_t> GetExtendedVideoSsrcs();

  inline uint32_t GetDefaultVideoSsrc() {
    return VIDEO_SSRC;  
  }
  uint32_t GetDefaultAudioSsrc() {
    return AUDIO_SSRC;  
  }

 private:
  // Use by WebRtcEndpoint, to set and get the current target bandwidth for the media
  void set_target_bandwidth(unsigned int target_bandwidth) {
    local_sdp_.set_target_bandwidth(target_bandwidth);
  }
  unsigned int get_target_bandwidth() {
    return local_sdp_.get_target_bandwidth();
  }

  void set_video_encoding(olive::VideoEncoding encoding) {
    video_encoding_ = encoding;
  }

  void set_min_encoding_bitrate(unsigned int min_encoding_bitrate) {
    min_encoding_bitrate_ = min_encoding_bitrate;
  }

  void InitWithSsrcs(std::vector<uint32_t> extended_video_ssrcs,
                     uint32_t default_video_ssrc,
                     uint32_t default_audio_ssrc);

  /*
   * Get the total number of packet sent by this stream.
   *
   * @param[in] ssrc : the local ssrc that want to check out
   * @return total number of packets sent, or -1 in error
   */
  int GetSendPackets(uint32_t ssrc);

  /*
   * Get the total number of bytes sent by this stream.
   *
   * @param[in] ssrc : the local ssrc that want to check out
   * @return total number of bytes sent
   */
  int GetSendBytes(uint32_t ssrc);

  /*
   * Get the timestamp of the last packet send.
   *
   * @param[in] ssrc : the local ssrc that want to check out
   * return the timestamp
   */
  uint32_t GetLastTs(uint32_t ssrc);

  std::string GetJSONCandidate(const std::string& mid,
                               const std::string& sdp);

  uint16_t GenerateNextFecSequenceNumber(bool is_fec, char* buf);

  void ResetSequenceNumberNotContainFec(char* buf);

  /*
   * The function to process the packet. place holder right now.
   */
  void ProcessPacket(packetType type, char* buf, int lne);

  /*
   * Initiate the NetworkStatus object , get public ip ,and start the 
   * SendThreadLoop thread
   * for send packet.
   */
  void Init();

  /*
   * Delete objects that created by current thread.
   */
  void Destroy();

  /*
   * Stores the receiver and sender bitrate statisticals.
   */
  void UpdateReceiverBitrate(int len);

  void UpdateSenderBitrate(int len);

  void SendPacketAsFec(char* buf, int len, Transport *transport);

  void WriteAndSend(Transport* transport, int priority, 
                    unsigned char* data, int len);
  /*
   * Update the session or stream info to the singleton class to record the
   * the connection status and etc.
   */
  void UpdateSessionOrStreamInfo(const string& event_key,
                                 const string& event_value);
  
  /*
   * If the FLAG enable_red_fec is set to true, the incoming packet will be 
   * processing as a RED/FEC packet.
   *
   * @param[in] packet the incoming RED packet  
   */ 
  void ReceivePacketAsFec(dataPacket* packet);

  void CallConnectReadyListeners() const {
    std::lock_guard<std::mutex> lock(connect_ready_listeners_mutex_);
    for (auto listener : connect_ready_listeners_) {
      listener();
    }
  }

  void CallFindCandidateListeners() const {
    std::lock_guard<std::mutex> lock(find_candidate_listeners_mutex_);
    for (auto listener : find_candidate_listeners_) {
      listener();
    }
  }

  void CallRecvRtpListeners(const dataPacket &packet) const {
    std::lock_guard<std::mutex> lock(recv_rtp_listeners_mutex_);
    for (auto listener : recv_rtp_listeners_) {
      listener(packet);
    }
  }

  void CallRecvRtcpListeners(const dataPacket &packet) const {
    std::lock_guard<std::mutex> lock(recv_rtcp_listeners_mutex_);
    for (auto listener : recv_rtcp_listeners_) {
      listener(packet);
    }
  }

  // Update the network status: RTT, Bitrate etc.
  long long last_update_ns_time_ = 0;
  void UpdateNetworkStatus();

  void SendPacketToPlugin(dataPacket packet);

  // When sending the packet, we should check the payload of the packet.
  // If this stream has new codec, we should reset the new codec.
  void ResetRelayPacketCodec(char* data, packetType type);

  // mutex to lock the plugins processing module.
  boost::mutex plugin_mutex_;

  // session and stream id.
  int session_id_;
  int stream_id_;

  // The flag of the running status of the class/thread.
  bool running_ = false;
  // The state of the transportation state.
  TransportState transport_state_ = TRANSPORT_INITIAL;

  // The options for a webrtc endpoint
  bool audio_enabled_;    // stream contains audio
  bool video_enabled_;    // stream contains video
  bool trickle_enabled_;  // use trickle ice to connect.
  int bundle_;            // audio and video are bundled.
  IceConfig ice_config_;  // the ICE config parameters for this endpoint.
  std::string public_ip_; // The external/public ip of the server.

  // Transport related fields.
  // local/remote SDP
  SdpInfo remote_sdp_;
  SdpInfo local_sdp_;

  // The corresponding plugin where all the traffic goes through and process
  TransportPlugin* plugin_ = NULL;
  // Dtls transport for video/audio channel.
  Transport* video_transport_ = NULL;
  Transport* audio_transport_ = NULL;

  WebRtcEndpointEventListener* event_listener_;

  // listeners for connect ready.
  std::vector<std::function<void(void)>> connect_ready_listeners_;
  mutable std::mutex connect_ready_listeners_mutex_;

  // listeners for find candidate.
  std::vector<std::function<void(void)>> find_candidate_listeners_;
  mutable std::mutex find_candidate_listeners_mutex_;

  // listeners for receive RTP packet.
  std::vector<std::function<void(const dataPacket &)>> recv_rtp_listeners_;
  mutable std::mutex recv_rtp_listeners_mutex_;

  // listeners for receive RTCP packet.
  std::vector<std::function<void(const dataPacket &)>> recv_rtcp_listeners_;
  mutable std::mutex recv_rtcp_listeners_mutex_;

  // Defines the ssrc for audio/video/video_rtx.
  int AUDIO_SSRC=44444;
  int VIDEO_SSRC=55543;
  int VIDEO_RTX_SSRC=55555;
  // If the plugin is responsible for RTP packet's SSRC, set it to false.
  // Otherwise the transport_delegate will rewrite the packet's SSRC to
  // the main (and the single) SSRC in the SDP.
  bool should_rewrite_ssrc_ = true;
  // The preferred video encoding from the stream.
  olive::VideoEncoding video_encoding_;
  // The minimal encoding bitrate
  unsigned int min_encoding_bitrate_ = 0;

  // Transport related fields
  // The transport statisiticals data.
  int receiver_bitrate_last_ = 0;
  int receiver_bitrate_ = 0;
  int sender_bitrate_ = 0;
  int sender_bitrate_last_ = 0;
  int audio_send_packets_ = 0;

  long rb_last_time_ = 0;
  long sb_last_time_ = 0;

  // All the processor modules (rtcp processing, fec modules etc.)
  boost::scoped_ptr<RtcpProcessor> rtcp_processor_;          // The Rtcp processor module.
  boost::scoped_ptr<webrtc::FecReceiverImpl> fec_receiver_;  // FEC receiver
  boost::scoped_ptr<webrtc::ForwardErrorCorrection> fec_;
  boost::scoped_ptr<webrtc::ProducerFec> producer_;          // FEC producer

  RtpSender* rtp_sender_;  // The RTP sender module.

  RtpCapture* rtp_capture_ = NULL; // The RTP capture module.

  std::shared_ptr<NetworkStatus> network_status_ = nullptr;

  // sequence numbers.
  uint16_t recv_next_sequence_number_;
  uint16_t current_fec_seqnumber_;
  uint16_t fec_send_count_;

  // This will never be changed after constructor.
  const bool is_server_mode_;

  // Friend classes
  friend class WebRtcEndpoint;
  friend class RtcpSender;
  friend class RtpSender;
  friend class PacketReplayDriver;
};
}  // namespace orbit

