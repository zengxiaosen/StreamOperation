/*
 * Copyright 2016 All Rights Reserved.
 * Author: caixinyu@orangelab.cn (Cai Xinyu)
 *
 * client.cc
 * ---------------------------------------------------------------------------
 * A client by which one can easily communicate with server.
 * ---------------------------------------------------------------------------
 */

#include "client.h"

#include <unistd.h>

namespace orbit {
namespace client {

static const useconds_t CONNECT_TRY_INTERVAL  = (1000 * 1000); // 1s
static const int MAX_CONNECT_TRY_COUNT = 5;

using namespace olive::client;

bool OrbitClientStream::Connect() {
  std::lock_guard<std::mutex> lock(mutex_connect_);
  if (is_connect_ready_) {
    LOG(ERROR) << "Already connect ready";
    return false;
  }

  transport_delegate_.setPlugin(&plugin_);

  transport_delegate_.AddFindCandidateListener([this](){
    std::vector<orbit::CandidateInfo> candidate_infos;
    transport_delegate_.GetClientLocalCandidateInfos(&candidate_infos);

    for (CandidateInfo &info : candidate_infos) {
      olive::IceCandidate ice_candidate;
      Convert(info, &ice_candidate);
      grpc_client_.SendIceCandidate(session_id_, stream_id_,
                                    ice_candidate);
    }
  });

  transport_delegate_.AddConnectReadyListener([this](){
    is_connect_ready_ = true;
  });
  transport_delegate_.AddRecvRtpListener([this](const dataPacket &packet){
    CallRecvRtpListeners(packet);
  });
  transport_delegate_.AddRecvRtcpListener([this](const dataPacket &packet){
    CallRecvRtcpListeners(packet);
  });

  // if you want to move this line, think again.
  transport_delegate_.StartAsClient();

  transport_delegate_.GetClientLocalSdp(&client_sdp_);
  std::string sdp_answer = grpc_client_.SendSdpOffer(session_id_,
                                                     stream_id_,
                                                     client_sdp_.getSdp());

  transport_delegate_.SetRemoteSdp(sdp_answer);

  server_sdp_.initWithSdp(sdp_answer, "video");
  server_sdp_.getCredentials(ice_username_, ice_password_,
                             orbit::VIDEO_TYPE);

  // now, we will get candidates of server and try to connect to server.
  int try_count = 0;
  while (!is_connect_ready_) {
    std::vector<CandidateInfo> server_candidate_infos;
    std::vector<olive::IceCandidate> server_ice_candidates;

    grpc_client_.RecvIceCandidate(session_id_, stream_id_,
                                  &server_ice_candidates);

    for (olive::IceCandidate &ice_candidate : server_ice_candidates) {
      orbit::CandidateInfo candidate_info;
      Convert(ice_candidate, &candidate_info);
      server_candidate_infos.push_back(candidate_info);
    }

    transport_delegate_.SetRemoteCandidates(server_candidate_infos);

    ++try_count;
    if (try_count > MAX_CONNECT_TRY_COUNT) {
      return false;
    }

    usleep(CONNECT_TRY_INTERVAL);
  }

  return true;
}

void OrbitClientStream::Convert(const olive::IceCandidate &ice_candidate,
                                orbit::CandidateInfo *candidate_info) const {
  if (!candidate_info) {
    LOG(ERROR) << "NULL Pointer";
    return;
  }

  std::string ice_str = ice_candidate.ice_candidate();
  std::string sdp_mid = ice_candidate.sdp_mid();

  candidate_info->FromString(ice_str);
  candidate_info->mediaType = (sdp_mid == "video" ? orbit::VIDEO_TYPE : orbit::AUDIO_TYPE);
  candidate_info->username = ice_username_;
  candidate_info->password = ice_password_;
}

void OrbitClientStream::Convert(const orbit::CandidateInfo &candidate_info,
                                olive::IceCandidate *ice_candidate) const {
  if (!ice_candidate) {
    LOG(ERROR) << "NULL Pointer";
    return;
  }

  ice_candidate->set_ice_candidate(candidate_info.ToString());
  ice_candidate->set_sdp_mid(candidate_info.mediaType == AUDIO_TYPE ? "audio" : "video");
  ice_candidate->set_sdp_m_line_index(candidate_info.mediaType == AUDIO_TYPE ? 0 : 1);
}


}  // namespace client
}  // namespace orbit

