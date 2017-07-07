/*
 * Copyright 2016 All Rights Reserved.
 * Author: chenteng@orangelab.cn
 *
 * network_status.cc
 * ---------------------------------------------------------------------------
 * Implements the network status statistics.
 * ---------------------------------------------------------------------------
 *  Created on: May 9, 2016
 */

#include <sys/time.h>
#include <sstream>
#include <memory>
#include "network_status.h"
#include "glog/logging.h"
#include "gflags/gflags.h"
#include "stream_service/orbit/base/timeutil.h"
#include "stream_service/orbit/base/singleton.h"
#include "stream_service/orbit/base/session_info.h"
#include "stream_service/orbit/http_server/exported_var.h"
#include "stream_service/orbit/rtp/janus_rtcp_defines.h"
#include "stream_service/orbit/network_status_common.h"

DECLARE_int32(leave_time);
DECLARE_int32(mute_time);

#define UPDATE_INTERVAL 3000  // in milliseconds

namespace orbit {

RttStatus::RttStatus() {
  long long current_time = getTimeMS();
  last_time_of_rtt_update_ = current_time;
}

/*
 * Method to update RTT every 3 seconds.
 */
void RttStatus::UpdateRTT(int64_t rtt) {
  if (rtt > 0) {
    total_rtt_ += rtt;
    number_of_rtt_++;
    rtt_ = rtt;
  }
  long long current_time = getTimeMS();
  if (current_time - last_time_of_rtt_update_ > UPDATE_INTERVAL) {
    if (number_of_rtt_ > 0) {
      avg_rtt_ = total_rtt_ / number_of_rtt_;
    } else {
      avg_rtt_ = -1;
    } 
    last_time_of_rtt_update_ = current_time;
    number_of_rtt_ = 0;
    total_rtt_ = 0;
  }
}

void SendPacketsStatus::CalculateSendFractionLost(uint32_t highest_seqnumber,
                                                  uint32_t cumulative_lost) {
  long long current_time = getTimeMS();
  if (last_update_time_ == 0) {
    last_update_time_  = current_time;
  }
  // if it's time to update ?
  if (current_time - last_update_time_ < UPDATE_INTERVAL)
    return;
  // calculate and update fraction lost 
  int seq_duration = highest_seqnumber - current_highest_seqnumber_;
  int lost_count = cumulative_lost - last_cumulative_lost_;
  if (seq_duration > 0 && lost_count >= 0) { 
    send_fraction_lost_ = lost_count * 100 / seq_duration;
  }
  last_update_time_ = current_time;
  current_highest_seqnumber_ = highest_seqnumber;
  last_cumulative_lost_ = cumulative_lost;

  expect_send_packets_ = seq_duration;
  send_lost_ = lost_count;
}

bool operator< (const NetworkStatusManager::Key &a,
                const NetworkStatusManager::Key &b) {
  if (a.session_id() < b.session_id()) {
    return true;
  }
  if (a.session_id() > b.session_id()) {
    return false;
  }
  return a.stream_id() < b.stream_id();
}

std::map<NetworkStatusManager::Key, std::shared_ptr<NetworkStatus>> 
    NetworkStatusManager::map_;
std::mutex NetworkStatusManager::mutex_;

std::map<NetworkStatusManager::Key, long> 
    NetworkStatusManager::stream_update_map_;
  
void NetworkStatusManager::Init(int32_t session_id, int32_t stream_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  Key key(session_id, stream_id);
  std::shared_ptr<NetworkStatus> p = std::make_shared<NetworkStatus>(session_id,
                                                                     stream_id);
  map_[key] = p;

  long current_time = GetCurrentTime_MS();
  stream_update_map_[key] = current_time;
}

// update stream incomming packet time
void NetworkStatusManager::UpdateStreamIncommingTime(int32_t session_id, int32_t stream_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  Key key(session_id, stream_id);
  long current_time = GetCurrentTime_MS();
  stream_update_map_[key] = current_time;
}

long NetworkStatusManager::GetStreamLastTime(int32_t session_id, int32_t stream_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  Key key(session_id, stream_id);
  long time = stream_update_map_[key];

  return time;
}

void NetworkStatusManager::TraverseStreamMap(std::vector<NetworkStatusManager::Key>* remove_stream) {
  std::lock_guard<std::mutex> lock(mutex_);
  long current_time = GetCurrentTime_MS();

  auto ite = stream_update_map_.begin();
  for (; ite != stream_update_map_.end(); ite ++) {
    if (ite->second == 0)
      continue;

    if (current_time - ite->second > FLAGS_leave_time) {
      remove_stream->push_back(ite->first);
    }
  }
}

void NetworkStatusManager::GetProbeNetResponse(const int& session_id,
                                               const int& stream_id,
                                               olive::ProbeNetTestResult* probe_result) {
  std::lock_guard<std::mutex> lock(mutex_);
  Key key(session_id, stream_id);
  auto ite = map_.find(key);
  if (ite != map_.end()) {
    std::shared_ptr<NetworkStatus> network_status = ite->second;
    bool has_video = network_status->CheckHasVideoPacket();
    bool has_audio = network_status->CheckHasAudioPacket(stream_id);

    network_status->SetNetworkStatus(probe_result);
    probe_result->set_has_video(has_video);
    probe_result->set_has_audio(has_audio);
    probe_result->set_network_speed(network_status->GetBitrate(false));
  }
}

void NetworkStatusManager::UpdateTimeOfStreamHighEnergy(const int& session_id, const int& stream_id, opus_int32 audio_energy) {
  std::lock_guard<std::mutex> lock(mutex_);
  Key key(session_id, stream_id);
  auto ite = map_.find(key);
  if (ite != map_.end()) {
    // audio_mixer_element.h : ENERGY_LOW = 1000000
    if (audio_energy > 1000000) {
      (ite->second)->UpdateHighEnergyTime(stream_id);
    }
  }
}

std::shared_ptr<NetworkStatus> NetworkStatusManager::Get(int32_t session_id,
                                                         int32_t stream_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  Key key(session_id, stream_id);
  auto iter = map_.find(key);
  if (iter != map_.end()) {
    return iter->second;
  }
  return nullptr;
}

void NetworkStatusManager::Destroy(int32_t session_id, int32_t stream_id) {
  Key key(session_id, stream_id);
  std::lock_guard<std::mutex> lock(mutex_);
  map_.erase(key);

  stream_update_map_.erase(key);
}

NetworkStatus ::NetworkStatus(unsigned int session_id, unsigned int stream_id) {
  network_status_ = NET_GREAT;
  session_id_ = session_id;
  stream_id_ = stream_id;
}

NetworkStatus ::~NetworkStatus() {
}

uint32_t NetworkStatus::GetSendFractionLost(bool is_audio, uint32_t ssrc) {
  uint32_t find_ssrc = 0;
  if (is_audio) {
    find_ssrc = audio_local_ssrc_;
  } else {
    find_ssrc = ssrc;  
  } 
  auto iter = map_send_packets_status_.find(find_ssrc);
  if (iter != map_send_packets_status_.end()) {
    SendPacketsStatus* send_packets_status = &(iter->second);
    return send_packets_status->send_fraction_lost();
  }
  return 0;
}

void NetworkStatus::ReceivingPacket(bool is_video) {
  recv_packets_++;
  if(is_video) {
    recv_packets_video_++;
  } else {
    recv_packets_audio_++;
  }
  long long current_time = getTimeMS();
  if(current_time - last_recv_update_time_ > UPDATE_INTERVAL) {

    SessionInfoManager *session_info = 
        Singleton<SessionInfoManager>::GetInstance();
    SessionInfoPtr session = session_info->GetSessionInfoById(session_id_);
    if(session != NULL) {
      session->AppendStreamCustomInfo(
          stream_id_,
          "recv_packets_audio", 
          std::to_string(recv_packets_audio_-recv_packets_audio_old_));
      session->AppendStreamCustomInfo(
          stream_id_,
          "recv_packets_video", 
          std::to_string(recv_packets_video_-recv_packets_video_old_));
      session->SetStreamCustomInfo(
          stream_id_,
          "recv_packets_last_update",
          std::to_string(current_time));
    }

    recv_packets_audio_old_ = recv_packets_audio_;
    recv_packets_video_old_ = recv_packets_video_;
    last_recv_update_time_ = current_time;
  }
}

void NetworkStatus::ReceivingRTCPPacket(bool is_video) {
  if(is_video) {
    recv_rtcp_packets_video_++;
  } else {
    recv_rtcp_packets_audio_++;
  }
  long long current_time = getTimeMS();
  if(current_time - last_recv_rtcp_update_time_ > UPDATE_INTERVAL) {
    SessionInfoManager *session_info =
        Singleton<SessionInfoManager>::GetInstance();
    SessionInfoPtr session = session_info->GetSessionInfoById(session_id_);
    if(session != NULL) {
      session->AppendStreamCustomInfo(
          stream_id_,
          "recv_rtcp_packets_audio", 
          std::to_string(recv_rtcp_packets_audio_ - 
	                 recv_rtcp_packets_audio_old_));
      session->AppendStreamCustomInfo(
          stream_id_,
          "recv_rtcp_packets_video", 
          std::to_string(recv_rtcp_packets_video_ -
	                 recv_rtcp_packets_video_old_));
      session->SetStreamCustomInfo(
          stream_id_,
          "recv_rtcp_packets_last_update",
          std::to_string(current_time));
    }
    recv_rtcp_packets_audio_old_ = recv_rtcp_packets_audio_;
    recv_rtcp_packets_video_old_ = recv_rtcp_packets_video_;
    last_recv_rtcp_update_time_ = current_time;
  }
}

int NetworkStatus::GetSendPackets(uint32_t ssrc) {
  {
    std::lock_guard<std::mutex> lock(send_packets_status_mtx_);
    auto iter = map_send_packets_status_.find(ssrc);  
    if (iter != map_send_packets_status_.end()) {
      SendPacketsStatus* send_packets_status = &(iter->second);
      return send_packets_status->send_packets();
    }
    return -1;
  }
}

int NetworkStatus::GetSendBytes(uint32_t ssrc) {
  {
    std::lock_guard<std::mutex> lock(send_packets_status_mtx_);
    auto iter = map_send_packets_status_.find(ssrc);  
    if (iter != map_send_packets_status_.end()) {
      SendPacketsStatus* send_packets_status = &(iter->second);
      return send_packets_status->send_bytes(); 
    }
    return -1;
  }
}

uint32_t NetworkStatus::GetLastTs(uint32_t ssrc) {
  {
    std::lock_guard<std::mutex> lock(send_packets_status_mtx_);
    auto iter = map_send_packets_status_.find(ssrc);  
    if (iter != map_send_packets_status_.end()) {
      SendPacketsStatus* send_packets_status = &(iter->second);
      return send_packets_status->last_ts();
    }
    return -1;
  }
}

void NetworkStatus::SendingPacket(bool is_video, uint32_t ssrc, 
                                  int len, uint32_t timestamp) {
  send_packets_++;
  if (is_video)
    video_send_packets_++;
  else 
    audio_send_packets_++;
  // Update send packets , send bytes and last_ts for all ssrcs
  {
    std::lock_guard<std::mutex> lock(send_packets_status_mtx_);
    auto send_iter = map_send_packets_status_.find(ssrc);
    if (send_iter != map_send_packets_status_.end()) {
      SendPacketsStatus* send_packets_status = &(send_iter->second); 
      send_packets_status->set_send_packets(1);
      send_packets_status->set_send_bytes(len);
      send_packets_status->set_last_ts(timestamp);
    }
  }
  long long current_time = getTimeMS();
  if(current_time - last_send_update_time_ > UPDATE_INTERVAL) {
    SessionInfoManager *session_info =
        Singleton<SessionInfoManager>::GetInstance();
    SessionInfoPtr session = session_info->GetSessionInfoById(session_id_);
    if(session != NULL) {
      session->AppendStreamCustomInfo(
          stream_id_,
          "send_packets_audio", 
          std::to_string(audio_send_packets_-audio_send_packets_old_));
      session->AppendStreamCustomInfo(
          stream_id_,
          "send_packets_video", 
          std::to_string(video_send_packets_-video_send_packets_old_));
      session->SetStreamCustomInfo(
          stream_id_,
          "send_packets_last_update",
          std::to_string(current_time));
    }
    audio_send_packets_old_ = audio_send_packets_;
    video_send_packets_old_ = video_send_packets_;
    last_send_update_time_ = current_time;
  }
}

void NetworkStatus::SendingRTCPPacket(bool is_video) {
  if(is_video) {
    send_rtcp_packets_video_++;
  } else {
    send_rtcp_packets_audio_++;
  }
  long long current_time = getTimeMS();
  if(current_time - last_send_rtcp_update_time_ > UPDATE_INTERVAL) {
    SessionInfoManager *session_info =
        Singleton<SessionInfoManager>::GetInstance();
    SessionInfoPtr session = session_info->GetSessionInfoById(session_id_);
    if(session != NULL) {
      session->AppendStreamCustomInfo(
          stream_id_,
          "send_rtcp_packets_audio", 
          std::to_string(send_rtcp_packets_audio_ - 
	                 send_rtcp_packets_audio_old_));
      session->AppendStreamCustomInfo(
          stream_id_,
          "send_rtcp_packets_video", 
          std::to_string(send_rtcp_packets_video_ - 
	                 send_rtcp_packets_video_old_));
      session->SetStreamCustomInfo(
          stream_id_,
          "send_rtcp_packets_last_update",
          std::to_string(current_time));
    }
    send_rtcp_packets_audio_old_ = send_rtcp_packets_audio_;
    send_rtcp_packets_video_old_ = send_rtcp_packets_video_;
    last_send_rtcp_update_time_ = current_time;
  }
}

void NetworkStatus::IncreaseNackCount(bool is_send) {
  if (is_send) {
    send_nack_count_++;
  } else {
    recv_nack_count_++;
  }
}

void NetworkStatus::UpdateTargetBitrate(bool is_video, 
                                        uint32_t target_bitrate) {
  if (is_video) {
    bitrate_status_.set_video_target_bitrate(target_bitrate);   
  } else {
    bitrate_status_.set_audio_target_bitrate(target_bitrate);		
  }
}

int32_t NetworkStatus::GetTargetBitrate() const {
  int32_t audio_target_bitrate = bitrate_status_.audio_target_bitrate();
  int32_t video_target_bitrate = bitrate_status_.video_target_bitrate();
  if ((audio_target_bitrate == -1) && (video_target_bitrate == -1))
    return -1;
  if (audio_target_bitrate == -1)
    return video_target_bitrate;
  if (video_target_bitrate == -1)
    return audio_target_bitrate;

  return (audio_target_bitrate + video_target_bitrate);
}

/*
 * The return value is one of the send or receive fraction lost, the bigger one
 *  will be choose.
 */
int NetworkStatus::GetNetworkFractionLost() {
  return ((send_fraction_lost_ > recv_fraction_lost_) ? send_fraction_lost_ :
         recv_fraction_lost_); 
}

olive::ConnectStatus NetworkStatus::GetConnectStatus() {
  NETWORK_STATUS network_status = GetNetworkStatus();
  int64_t avg_rtt = GetAllSsrcsAverageRtt();
  int32_t target_bitrate = GetTargetBitrate();
  int fraction_lost = GetNetworkFractionLost();

  olive::ConnectStatus status_info;
  status_info.set_status(network_status);
  status_info.set_rtt(avg_rtt);
  status_info.set_targetbitrate(target_bitrate);
  status_info.set_fractionlost(fraction_lost);

  return status_info;
}

/*
 * Statistic total number of send packets and the total number of send lost, to
 * calculate the total lost rate of packets sent.
 */
void NetworkStatus::UpdateTotalSendFractionLost() {
  SendPacketsStatus* send_frac_status = NULL;
  uint32_t total_send = 0;
  uint32_t total_send_lost = 0;
  { 
    std::lock_guard<std::mutex> lock(send_packets_status_mtx_);
    cumulate_send_lost_ = 0;
    for (auto iter = map_send_packets_status_.begin();
      iter != map_send_packets_status_.end(); iter++) {
      send_frac_status = &(iter->second);
      total_send += send_frac_status->expect_send_packets();
      total_send_lost += send_frac_status->send_lost();
      // cumulate total send lost of all ssrcs
      cumulate_send_lost_ += send_frac_status->last_cumulative_lost();
    }
  }
  // calculate send fraction lost of all ssrcs in uinttime
  if (total_send > 0 && total_send_lost >= 0) {
    send_fraction_lost_ = total_send_lost * 100 / total_send;
  }
}

/*
 * Method to get the network status depends on the fraction lost ,target bitrate
 * and RTT value.
 */
NETWORK_STATUS NetworkStatus::GetNetworkStatus() {
  NETWORK_STATUS net_stats_by_fraction = EstimateNetworkStatusByFractionLost();
  NETWORK_STATUS net_stats_by_rtt = EstimateNetworkStatusByRTT();
  //FIXME Ideally, the overall network status should estimated by target bandwidth ,
  //RTT and lost rate, but as the target bandwidth estimated by webrtc code
  //module is wrong, so we just use RTT and lost rate currently.

  //NETWORK_STATUS net_stats_by_bw = EstimateNetworkStatusByBandwidth();

  return (net_stats_by_fraction > net_stats_by_rtt ? net_stats_by_fraction : 
                                                     net_stats_by_rtt);
}

NETWORK_STATUS NetworkStatus::EstimateNetworkStatusByFractionLost() {
  // Calculate the sender network status by send_fraction_lost_
  NETWORK_STATUS send_net_status = NetworkStatusLevel(send_fraction_lost_);
  // Calculate the receiver network status by recv_fraction_lost_
  NETWORK_STATUS recv_net_status = NetworkStatusLevel(recv_fraction_lost_);
  // Get the unitive　status
  if (send_net_status == NET_CANT_CONNECT || 
      recv_net_status == NET_CANT_CONNECT) {
    return NET_CANT_CONNECT;
  } else if (send_net_status == NET_VERY_BAD || 
      recv_net_status == NET_VERY_BAD) {
    return NET_VERY_BAD;
  } else if (send_net_status == NET_BAD || recv_net_status == NET_BAD) {
    return NET_BAD;
  } else if (send_net_status == NET_NOT_GOOD || 
      recv_net_status == NET_NOT_GOOD) {
    return NET_NOT_GOOD;
  } else if (send_net_status == NET_GOOD || recv_net_status == NET_GOOD) {
    return NET_GOOD;
  } else if (send_net_status == NET_GREAT || recv_net_status == NET_GREAT) {
    return NET_GREAT;
  }
  return NET_GREAT;
}

/*
 * Implement the network status estimate by bandwidth, the bandwidth is the 
 * target bitrate estimated by webrtc's remote_bitrate_estimator module. 
 */
NETWORK_STATUS NetworkStatus::EstimateNetworkStatusByBandwidth() {
  int32_t audio_target_bitrate = bitrate_status_.audio_target_bitrate();
  int32_t video_target_bitrate = bitrate_status_.video_target_bitrate();
  // estimate audio status level
  NETWORK_STATUS audio_status;
  if (audio_target_bitrate == -1) {
    audio_status = NET_UNKNOWN;
  } else {
    if (audio_target_bitrate <= 10 * 1024) {         // 10 kbps
      audio_status = NET_VERY_BAD;
    } else if (audio_target_bitrate <= 15 * 1024) {  // 15 kbps
      audio_status = NET_BAD; 
    } else if (audio_target_bitrate <= 20 * 1024) {  // 20 kbps
      audio_status = NET_NOT_GOOD;
    } else {
      audio_status = NET_GREAT;
    }
  }
  // estimate video status level
  NETWORK_STATUS video_status;
  if (video_target_bitrate == -1) {
    video_status = NET_UNKNOWN;
  } else {
    if (video_target_bitrate <= 64 * 1024) {          // 64 kbps
    		video_status = NET_VERY_BAD;
    } else if (video_target_bitrate <= 128 * 1024) {  // 128 kbps
    		video_status = NET_BAD; 
    } else if (video_target_bitrate <= 256 * 1024) {  // 256 kbps
    		video_status = NET_NOT_GOOD;
    } else {
    		video_status = NET_GREAT;
    }
  }
  return (audio_status > video_status ? audio_status : video_status);
}

NETWORK_STATUS NetworkStatus::EstimateNetworkStatusByRTT() {
  int64_t avg_rtt = GetAllSsrcsAverageRtt();
  if (avg_rtt == -1)
    return NET_UNKNOWN;

  if (avg_rtt >= 400) {                 // in ms
    return NET_VERY_BAD;
  } else if (avg_rtt >= 300) {
    return NET_BAD;
  } else if (avg_rtt >= 150){
    return NET_NOT_GOOD;
  } else if (avg_rtt >= 80) {
    return NET_GOOD;
  } else {
    return NET_GREAT;
  }
}

NETWORK_STATUS NetworkStatus::NetworkStatusLevel(int fraction_lost) {
  if (fraction_lost <= 1) {
    return NET_GREAT;
  } else if ( fraction_lost <= 4) {
    return NET_GOOD;
  } else if ( fraction_lost <= 10) {
    return NET_NOT_GOOD;
  } else if ( fraction_lost <= 50) {
    return NET_BAD;
  } else if ( fraction_lost <= 80) {
    return NET_VERY_BAD;
  }
  return NET_CANT_CONNECT;
}

void NetworkStatus::UpdateStatusZ() {
  SessionInfoManager *session_info = 
      Singleton<SessionInfoManager>::GetInstance();
  SessionInfoPtr session = session_info->GetSessionInfoById(session_id_);
  if(session != NULL) {
    uint32_t send_bitrate = bitrate_status_.send_bitrate();
    uint32_t recv_bitrate = bitrate_status_.recv_bitrate();
    session->SetStreamCustomInfo(stream_id_,"sender_bitrate", send_bitrate);
    session->SetStreamCustomInfo(stream_id_,"receiver_bitrate", recv_bitrate);
    session->SetStreamCustomInfo(stream_id_,"sender_packets", send_packets_);
    session->SetStreamCustomInfo(stream_id_,"receiver_packets", recv_packets_);
    session->SetStreamCustomInfo(stream_id_,"sender_lost", cumulate_send_lost_);
    session->SetStreamCustomInfo(stream_id_,"receiver_lost", cumulate_recv_lost_);
    
    session->SetStreamCustomInfo(stream_id_,"sender_unittime_fraction_lost", 
                                 send_fraction_lost_);
    session->SetStreamCustomInfo(stream_id_,"receiver_unittime_fraction_lost", 
                                 recv_fraction_lost_);

    NETWORK_STATUS network_status = GetNetworkStatus();
    session->SetStreamCustomInfo(stream_id_,"network_status", network_status);
    session->AppendStreamCustomInfo(
          stream_id_,
          "network_state",
          ConvertNetworkStatusToString(network_status));
  }
}

std::string NetworkStatus::ConvertNetworkStatusToString(NETWORK_STATUS network_status){
  std::string str_network_state;
  std::stringstream ss;

  int int_network_state = network_status;
  ss << int_network_state;
  ss >> str_network_state;
  
  return str_network_state;
}

void NetworkStatus::UpdateRTT(int64_t rtt, uint32_t ssrc) {
  {
    std::lock_guard<std::mutex> lock(rtt_status_mtx_);
    auto iter = map_rtt_status_.find(ssrc);
    if (iter != map_rtt_status_.end()) {
      RttStatus* rtt_status = &(iter->second);
      rtt_status->UpdateRTT(rtt); 
    }
  } // end of lock
}

void NetworkStatus::UpdateBitrate(bool is_send, unsigned int bitrate) {
  if (is_send) {
    bitrate_status_.set_send_bitrate(bitrate);
  } else {
    bitrate_status_.set_recv_bitrate(bitrate);
  }
}

unsigned int NetworkStatus::GetBitrate(bool is_send) {
  if (is_send) {
    return bitrate_status_.send_bitrate();
  } else {
    return bitrate_status_.recv_bitrate();
  }
}


/*
 * The jitter information is got from the RR/SR report that generated by server
 * or client side.
 */
void NetworkStatus::UpdateJitters(REPORT_SOURCE report_source,
                                  bool is_video, 
                                  uint32_t jitter_value,
                                  uint32_t local_ssrc
                                  ) {
  if (report_source == SERVER_REPORT) { 
    if (is_video) {
      server_report_video_jitter_ = jitter_value;
    } else {
      server_report_audio_jitter_ = jitter_value;
    }
  } else { 
    if (is_video) {
      {
        std::lock_guard<std::mutex> lock(client_report_video_jitters_mtx_);
        auto iter = client_report_video_jitters_.find(local_ssrc);
        if (iter != client_report_video_jitters_.end()) {
          iter->second = jitter_value;
        }
      }
    } else {
      client_report_audio_jitter_ = jitter_value;
    }
  }
}

/*
 * Statistic the receive fraction lost of video or audio.
 */
void NetworkStatus::StatisticRecvFractionLost(bool is_video, 
                                              unsigned int seqnumber) {
  if (is_video) {
    StatisticRecvFractionOfVideo(seqnumber);
  } else {
    StatisticRecvFractionOfAudio(seqnumber);
  }
  // statistic total number of fraction lost in unit time
  unsigned int lost = video_lost_of_recv_ + audio_lost_of_recv_;
  unsigned int recv_count =
      expect_video_recv_packets_ + expect_audio_recv_packets_; 
  if (recv_count > 0) {
    recv_fraction_lost_ = lost * 100 / recv_count;
  }
  cumulate_recv_lost_ = video_recv_cumulative_lost_ + audio_recv_cumulative_lost_;   
}

/*
 * We'll statistic the fraction lost of packets receiving, and update it every 
 * 3 seconds. We use a vector to maintain the incoming sequence number, and the 
 * retransmit packet's seqnumber and the seqnumber that belongs to last
 * statistical cycle will be ignored.
 * We use a map to record the lost packet's seqnumber and the time of loss
 * found.
 */
void NetworkStatus::StatisticRecvFractionOfVideo(unsigned int seqnumber) {
  // Initialize
  if (!last_time_of_video_recv_fraction_update_) {
    long long current_time = getTimeMS();
    last_time_of_video_recv_fraction_update_ = current_time; 
  }
  if (max_recv_seqn_of_video_ == 0) {
    max_recv_seqn_of_video_ = seqnumber;
  }
  if (min_recv_seqn_of_video_ == 0) {
    min_recv_seqn_of_video_ = seqnumber;
    recv_video_seqn_vector_.push_back(seqnumber);
    number_of_video_received_++;
  }
  // It's a new packet ?
  int new_lost = seqnumber - max_recv_seqn_of_video_;
  if (new_lost > 0) {   // new packet
    if (new_lost > 1) { // there are some packets lost, records to map.
      long long tm = getTimeMS();
      int lost_counter = new_lost - 1;
      while (lost_counter > 0) {
        lost_of_recv_video_map_[seqnumber-lost_counter] = tm;
        lost_counter--;
      }
    } 
    max_recv_seqn_of_video_ = seqnumber;
  } else {            // old packet
    // Ignore duplicate packet. 
    for (auto it=recv_video_seqn_vector_.begin(); 
         it != recv_video_seqn_vector_.end(); it++) {
      if (*it == seqnumber) 
        return;
    }
    // The packets that belongs to privious statistic cycle or retransmit
    // packets will be ignored.
    auto iter = lost_of_recv_video_map_.find(seqnumber);
    if (iter == lost_of_recv_video_map_.end())
      return;
    long long current_time = getTimeMS();
    if (current_time > (iter->second + SEQ_MISSING_WAIT / 1000)) {
      VLOG(3) <<" current_time = " << current_time 
                <<" ,iter->second = " << iter->second
                <<" ,SEQ_MISSING_WAIT = " << SEQ_MISSING_WAIT; 
      return;
    }
  }
  // add to vector
  recv_video_seqn_vector_.push_back(seqnumber);
  number_of_video_received_++;
  // It's time to update the fraction lost ?
  long long current_time = getTimeMS();
  VLOG(3) <<" current_time = " << current_time
            <<" , last_time_of_video_recv_fraction_update_ = " 
            << last_time_of_video_recv_fraction_update_
            <<", duration = " 
            << current_time - last_time_of_video_recv_fraction_update_
            <<" , seqnumber = " << seqnumber;
  if (current_time - last_time_of_video_recv_fraction_update_ > 
      UPDATE_INTERVAL) {
    // update receive fraction lost
    unsigned int expected = 
        max_recv_seqn_of_video_ - min_recv_seqn_of_video_ + 1;
    unsigned int seq_lost = expected - number_of_video_received_;

    SessionInfoManager *session_info = 
        Singleton<SessionInfoManager>::GetInstance();
    SessionInfoPtr session = session_info->GetSessionInfoById(session_id_);
    if(session != NULL) {
      session->AppendStreamCustomInfo(
          stream_id_,
          "packet_lost_count_recv_video", 
          std::to_string(seq_lost));
      session->SetStreamCustomInfo(
          stream_id_,
          "packet_lost_last_update_video",
          std::to_string(current_time));
    }

    if (expected > 0) { 
      recv_fraction_lost_of_video_ = seq_lost * 100 / expected;
    }
    VLOG(3) <<" recv_fraction_lost_of_video_ = " 
            << recv_fraction_lost_of_video_
            <<" , seq_lost = " << seq_lost
            <<" , expected = " << expected
            <<" , number_of_video_received_ = " << number_of_video_received_;
    max_recv_seqn_of_video_ = 0;
    min_recv_seqn_of_video_ = 0;
    number_of_video_received_ = 0;
    last_time_of_video_recv_fraction_update_ = current_time;
    recv_video_seqn_vector_.clear();
    lost_of_recv_video_map_.clear(); 
    // also update receive packet lost
    video_recv_cumulative_lost_ += seq_lost;
    expect_video_recv_packets_ = expected;
    video_lost_of_recv_ = seq_lost;
  }
}

void NetworkStatus::StatisticRecvFractionOfAudio(unsigned int seqnumber) {
  // Initialize
  if (!last_time_of_audio_recv_fraction_update_) {
    long long current_time = getTimeMS();
    last_time_of_audio_recv_fraction_update_ = current_time; 
  }
  if (max_recv_seqn_of_audio_ == 0) {
    max_recv_seqn_of_audio_ = seqnumber;
  }
  if (min_recv_seqn_of_audio_ == 0) {
    min_recv_seqn_of_audio_ = seqnumber;
    recv_audio_seqn_vector_.push_back(seqnumber);
    number_of_audio_received_++;
  }
  // It's a new packet ?
  int new_lost = seqnumber - max_recv_seqn_of_audio_;
  if (new_lost > 0) {   // new packet
    if (new_lost > 1) { // there are some packets lost, records to map.
      long long tm = getTimeMS();
      int lost_counter = new_lost - 1;
      while (lost_counter > 0) {
        lost_of_recv_audio_map_[seqnumber-lost_counter] = tm;
        lost_counter--;
      }
    } 
    max_recv_seqn_of_audio_ = seqnumber;
  } else {            // old packet
    // Ignore duplicate packet. 
    for (auto it=recv_audio_seqn_vector_.begin(); 
         it != recv_audio_seqn_vector_.end(); it++) {
      if (*it == seqnumber) 
        return;
    }
    // The packets that belongs to privious statistic cycle or retransmit will 
    // be ignored.
    auto iter = lost_of_recv_audio_map_.find(seqnumber);
    if (iter == lost_of_recv_audio_map_.end())
      return;
    long long current_time = getTimeMS();
    if (current_time > (iter->second + SEQ_MISSING_WAIT / 1000)) {
      return;
    }
  }
  // add to vector
  recv_audio_seqn_vector_.push_back(seqnumber);
  number_of_audio_received_++;
  // It's time to update the fraction lost ?
  long long current_time = getTimeMS();
  if (current_time - last_time_of_audio_recv_fraction_update_ > 
      UPDATE_INTERVAL) {
    // update receive fraction lost
    unsigned int expected = 
        max_recv_seqn_of_audio_ - min_recv_seqn_of_audio_ + 1;
    unsigned int seq_lost = expected - number_of_audio_received_;

    SessionInfoManager *session_info = 
        Singleton<SessionInfoManager>::GetInstance();
    SessionInfoPtr session = session_info->GetSessionInfoById(session_id_);
    if(session != NULL) {
      session->AppendStreamCustomInfo(
          stream_id_,
          "packet_lost_count_recv_audio", 
          std::to_string(seq_lost));
      session->SetStreamCustomInfo(
          stream_id_,
          "packet_lost_last_update_audio",
          std::to_string(current_time));
    }

    if (expected > 0) { 
      recv_fraction_lost_of_audio_ = seq_lost * 100 / expected;
    }
    max_recv_seqn_of_audio_ = 0;
    min_recv_seqn_of_audio_ = 0;
    number_of_audio_received_ = 0;
    last_time_of_audio_recv_fraction_update_ = current_time;
    recv_audio_seqn_vector_.clear();
    lost_of_recv_audio_map_.clear();
    // also update receive packet lost
    audio_recv_cumulative_lost_ += seq_lost;
    expect_audio_recv_packets_ = expected;
    audio_lost_of_recv_ = seq_lost;
  }
}

/*
 * In this function , we'll statistic and update the send fraction lost of
 * ssrcs, includes multi video and one audio send fraction lost. And calculate 
 * the total lost rate of the stream.
 */
void NetworkStatus::StatisticSendFractionLost(bool is_video,
                                              unsigned int highest_seqnumber,
                                              unsigned int cumulative_lost,
                                              uint32_t ssrc) {
  // find and update the send fraction status of the source 'ssrc'
  auto iter = map_send_packets_status_.find(ssrc);
  if (iter == map_send_packets_status_.end())
    return;
   
  SendPacketsStatus* send_frac_status = &(iter->second);
  send_frac_status->CalculateSendFractionLost(highest_seqnumber,
                                              cumulative_lost);

  // update total send fraction lost of all ssrcs.
  UpdateTotalSendFractionLost();
}

void
NetworkStatus::InitWithSsrcs(std::vector<unsigned int> extended_video_ssrcs,
                             uint32_t default_video_ssrc,
                             uint32_t default_audio_ssrc) {
  SendPacketsStatus send_packets_status;
  for (auto iter : extended_video_ssrcs) {
    LOG(INFO) << "iter=" << iter;
    // initiate SendPacketsStatus object 
    map_send_packets_status_[iter] = send_packets_status;
    // initiate rtt status with extended video ssrcs
    RttStatus rtt_status;
    map_rtt_status_[iter] = rtt_status;
  }
  RttStatus rtt_status;
  map_rtt_status_[default_video_ssrc] = rtt_status;
  map_rtt_status_[default_audio_ssrc] = rtt_status;
  map_send_packets_status_[default_audio_ssrc] = send_packets_status;
  map_send_packets_status_[default_video_ssrc] = send_packets_status;
  
  // record the default_audio_ssrc
  audio_local_ssrc_ = default_audio_ssrc;
}

/*
 * Get the average rtt value of all local ssrcs in real time
 */
int64_t NetworkStatus::GetLastestRttOfSsrcs() {
  int64_t avg_rtt_count = 0;
  int64_t counter = 0;
  {
    std::lock_guard<std::mutex> lock(rtt_status_mtx_);
    for (auto iter = map_rtt_status_.begin(); 
         iter != map_rtt_status_.end(); ++iter) {
      RttStatus* rtt_status = &(iter->second);
      int64_t rtt = rtt_status->rtt();
      if (rtt <= 0)
        continue;
      avg_rtt_count += rtt_status->rtt();
      counter++;
    }
    if (counter == 0)
      return 0;
    int64_t avg_rtt = avg_rtt_count / counter;
    return avg_rtt;
  }
}

/*
 * Get the average rtt value of all local ssrcs in unittime
 */
int64_t NetworkStatus::GetAllSsrcsAverageRtt() {
  int64_t avg_rtt_count = 0;
  int64_t counter = 0;
  {
    std::lock_guard<std::mutex> lock(rtt_status_mtx_);
    for (auto iter = map_rtt_status_.begin(); 
         iter != map_rtt_status_.end(); ++iter) {
      RttStatus* rtt_status = &(iter->second);
      int64_t rtt = rtt_status->avg_rtt();
      if (rtt <= 0)
        continue;
      avg_rtt_count += rtt_status->avg_rtt();
      counter++;
    }
    if (counter == 0)
      return 0;
    int64_t avg_rtt = avg_rtt_count / counter;
    return avg_rtt;
  }
}

/**
 * Check if has audio packet entry
 */
bool NetworkStatus::CheckHasAudioPacket(const int& stream_id) {
  std::lock_guard<std::mutex> lock(stream_energy_mtx_);
  long last_time = map_stream_energy_[stream_id];
  long current_time = GetCurrentTime_MS();

  if (last_time == 0 || current_time - last_time > FLAGS_mute_time) {
    return false;
  } else {
    return true;
  }
}

/**
 * Check if has video packet entry
 */
bool NetworkStatus::CheckHasVideoPacket() {
  if (recv_packets_video_old_ > 0) {
    return true;
  } else {
    return false;
  }
}

/**
 * Set Network status
 */
void NetworkStatus::SetNetworkStatus(olive::ProbeNetTestResult* probe_result) {
  switch (network_status_) {
  case NET_GREAT : {
    probe_result->set_status(olive::ProbeNetTestResult::GREAT);
    break;
  }
  case NET_GOOD : {
    probe_result->set_status(olive::ProbeNetTestResult::GOOD);
    break;
  }
  case NET_NOT_GOOD : {
    probe_result->set_status(olive::ProbeNetTestResult::NOT_GOOD);
    break;
  }
  case NET_BAD : {
    probe_result->set_status(olive::ProbeNetTestResult::BAD);
    break;
  }
  case NET_VERY_BAD : {
    probe_result->set_status(olive::ProbeNetTestResult::VERY_BAD);
    break;
  }
  case NET_CANT_CONNECT : {
    probe_result->set_status(olive::ProbeNetTestResult::CANT_CONNECT);
    break;
  }
  default: {
    break;
  }
  }
}

/**
 * When the audio of the stream is higher than ENERGY_LOW(1000000)
 * update the current time. If there is no update for a long time, 
 * we think the stream has low audio or mute.
 */
void NetworkStatus::UpdateHighEnergyTime(const int& stream_id) {
  std::lock_guard<std::mutex> lock(stream_energy_mtx_);
  long current_time = GetCurrentTime_MS();
  map_stream_energy_[stream_id] = current_time;
}

} /* namespace orbit */
