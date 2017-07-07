/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * webrtc_endpoint.cc
 * ---------------------------------------------------------------------------
 * Implements the WebRtcEndpoint
 * ---------------------------------------------------------------------------
 */
#include "webrtc_endpoint.h"

#include <string>
#include "transport_plugin.h"
#include "transport_delegate.h"
#include "stream_service/orbit/logger_helper.h"
#include "stream_service/orbit/network_status_common.h"

namespace orbit {
  std::string WebRTCEventToString(WebRTCEvent event) {
    switch (event) {
    case CONN_INITIAL:
      return "CONN_INITIAL";
    case CONN_STARTED:
      return "CONN_STARTED";
    case CONN_GATHERED:
      return "CONN_GATHERED";
    case CONN_READY:
      return "CONN_READY";
    case CONN_FINISHED:
      return "FINISHED";
    case CONN_CANDIDATE:
      return "CONN_CANDIDATE";
    case CONN_SDP:
      return "CONN_SDP";
    case CONN_FAILED:
      return "CONN_FAILED";
    default:
      LOG(ERROR) << "Event:" << event << " not recognized.";
    }
    return "";
  }

  WebRtcEndpoint::WebRtcEndpoint(bool audio_enabled, 
                                 bool video_enabled,
                                 bool trickle_enabled, 
				 const IceConfig& ice_config, 
				 int session_id,
				 int stream_id) {
    transport_delegate_.reset(
       new TransportDelegate(audio_enabled, video_enabled,
                             trickle_enabled, ice_config,
                             session_id, stream_id));
    event_listener_ = nullptr;
  }

  WebRtcEndpoint::~WebRtcEndpoint() {
	  LOG(INFO) <<"Enter into ~WebRtcEndpoint()";
    if (event_listener_) {
      delete event_listener_;
      event_listener_ = NULL;
    }
  }

  bool WebRtcEndpoint::ProcessOffer(const std::string& offer_sdp,
                                    std::string* answer_sdp) {
    
    return transport_delegate_->ProcessOffer(offer_sdp, answer_sdp);
  }

  bool WebRtcEndpoint::AddRemoteCandidate(const std::string &mid,
                                          int mLineIndex,
                                          const std::string &sdp) {
    return transport_delegate_->AddRemoteCandidate(mid, mLineIndex, sdp);
  }

  std::string WebRtcEndpoint::CreateOffer() {
    // Not implement yet.
    return "";
  }

  bool WebRtcEndpoint::ProcessAnswer(const std::string& answer_sdp) {
    // Not implement yet.
    return true;
  }

  void WebRtcEndpoint::setWebRtcEndpointEventListener(
     WebRtcEndpointEventListener* listener) {
    this->event_listener_ = listener;
    transport_delegate_->setWebRtcEndpointEventListener(listener);
  }

  olive::ConnectStatus WebRtcEndpoint::GetConnectStatusInfo() {
    return this->transport_delegate_->GetConnectStatusInfo();  
  }

}  // namespace orbit

