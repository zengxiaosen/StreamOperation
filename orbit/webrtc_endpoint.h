/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * webrtc_endpoint.h
 * ---------------------------------------------------------------------------
 * Defines the abstract interface for WebRTC endpoint.
 * Common usage:
 * ---------------------------------------------------------------------------
 */

#ifndef WEBRTC_ENDPOINT_H__
#define WEBRTC_ENDPOINT_H__

#include <string>
#include "sdp_info.h"
#include "nice_connection.h"

#include "common_def.h"

#include "transport_delegate.h"

namespace orbit {

/**
 * WebRTC Events
 */
enum WebRTCEvent {
  CONN_INITIAL = 101,
  CONN_STARTED = 102,
  CONN_GATHERED = 103,
  CONN_READY = 104,
  CONN_FINISHED = 105,
  CONN_CANDIDATE = 201,
  CONN_SDP = 202,
  CONN_FAILED = 500
};

std::string WebRTCEventToString(WebRTCEvent event);

class WebRtcEndpointEventListener {
public:
  virtual ~WebRtcEndpointEventListener() {};
  virtual void NotifyEvent(WebRTCEvent newEvent, const std::string& message)=0;
};

class TransportDelegate;
class TransportPlugin;

// Defines the abstract interface for WebRTC endpoint.
class WebRtcEndpoint {
 public:
  WebRtcEndpoint(bool audio_enabled, bool video_enabled,
                 bool trickle_enabled, const IceConfig& ice_config,
                 int session_id, int stream_id);
  ~WebRtcEndpoint();

  /**
   * Process the sdp_offer and get the answer
   * @param offer_sdp The SDP.
   * @param answer_sdp The answer SDP to be sent back to the offerer.
   * @return true if the SDP was processed correctly.
   */
  bool ProcessOffer(const std::string& offer_sdp,
                    std::string* answer_sdp);

  /**
   * Create the sdp_offer
   * @return the offer_sdp to be sent to the client.
   */
  std::string CreateOffer();

  /**
   * Process the sdp_answer
   * @param answer_sdp The SDP.
   * @return true if the SDP was processed correctly.
   */
  bool ProcessAnswer(const std::string& answer_sdp);

  /**
   * Add new remote candidate (from remote peer).
   * @param mid the media identifier.
   * @param mLineIndex the media line index in the sdp.
   * @param sdp the ice candidate string in sdp format.
   * @return true if the SDP was received correctly.
   */
  bool AddRemoteCandidate(const std::string &mid,
                          int mLineIndex,
                          const std::string &sdp);

  void setWebRtcEndpointEventListener(
     WebRtcEndpointEventListener* listener);

  WebRtcEndpointEventListener* getWebRtcEndpointEventListener() {
    return event_listener_;
  }
  
  void set_target_bandwidth(unsigned int target_bandwidth) {
    this->transport_delegate_->set_target_bandwidth(target_bandwidth);
  }

  void set_video_encoding(olive::VideoEncoding encoding) {
    this->transport_delegate_->set_video_encoding(encoding);
  }

  void set_min_encoding_bitrate(unsigned int min_encoding_bitrate) {
    this->transport_delegate_->set_min_encoding_bitrate(min_encoding_bitrate);
  }

  inline void set_plugin(TransportPlugin* plugin) {
    this->transport_delegate_->setPlugin(plugin);
  }

  inline TransportPlugin* get_plugin() {
    return this->transport_delegate_->getPlugin();
  }

  void set_should_rewrite_ssrc(bool setting) {
    this->transport_delegate_->should_rewrite_ssrc_ = setting;
  }

  olive::ConnectStatus GetConnectStatusInfo();

  inline void Stop() {
    this->transport_delegate_->Stop();
  }

 private:
  std::shared_ptr<TransportDelegate> transport_delegate_;
  WebRtcEndpointEventListener* event_listener_;
};

}  // namespace orbit

#endif  // WEBRTC_ENDPOINT_H__

