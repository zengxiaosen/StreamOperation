/*
 * network_status.h
 *
 * Created on: May 9, 2016
 *      Author: chenteng
 *
 * This file is part of Orbit.
 * The file is used to calculate and update the network status, You can get the 
 * indicate stream's network information by calling the methods of NetworkStatus. 
 * Such as total number of packets sent, packets received, total number of packets
 * lost ,network status level, and so on.
 */

#pragma once

#include <atomic>
#include <map>
#include <mutex>
#include <memory>
#include <vector>

#include "common_def.h"
#include "stream_service/proto/stream_service.grpc.pb.h"
#include "stream_service/orbit/audio_processing/audio_energy.h"

namespace orbit {

typedef enum _NETWORK_STATUS {
  NET_UNKNOWN,
  NET_GREAT, 
  NET_GOOD,
  NET_NOT_GOOD,
  NET_BAD,
  NET_VERY_BAD,
  NET_CANT_CONNECT,
} NETWORK_STATUS;

typedef enum _REPORT_SOURCE {
  SERVER_REPORT,
  CLIENT_REPORT,
} REPORT_SOURCE;

class NetworkStatus;
class ConnStatusInfo;

/*
 * Manage bitrate status, include actual bandwidth used and target bitrate
 * estimated.
 */
class BitrateStatus {
 public:
  BitrateStatus() {}
  ~BitrateStatus() {}

  uint32_t send_bitrate() const { return send_bitrate_; }
 
  uint32_t recv_bitrate() const { return recv_bitrate_; }

  int32_t audio_target_bitrate() const { return audio_target_bitrate_; } 

  int32_t video_target_bitrate() const { return video_target_bitrate_; } 

  void set_send_bitrate(uint32_t bitrate) { send_bitrate_ = bitrate; }

  void set_recv_bitrate(uint32_t bitrate) { recv_bitrate_ = bitrate; }

  void set_audio_target_bitrate(uint32_t bitrate) { 
    audio_target_bitrate_ = bitrate;
  }

  void set_video_target_bitrate(uint32_t bitrate) { 
    video_target_bitrate_ = bitrate;
  }

 private:
  std::atomic <uint32_t> send_bitrate_{0};
  std::atomic <uint32_t> recv_bitrate_{0};
  std::atomic <int32_t> audio_target_bitrate_{-1};
  std::atomic <int32_t> video_target_bitrate_{-1};
};

class RttStatus {
 public:
  RttStatus();
  ~RttStatus() {}

  int64_t avg_rtt() { return avg_rtt_; }
  int64_t rtt() { return rtt_; }
  
  void UpdateRTT(int64_t rtt);

 private: 
  uint64_t total_rtt_{0};
  long long last_time_of_rtt_update_{0};
  uint16_t number_of_rtt_{0};
  int64_t avg_rtt_{0};
  int64_t rtt_{0};
};

/**
 * The class is used to statistic the status of packets send. such as send
 * packets, send bytes, send fraction lost etc.
 */
class SendPacketsStatus {
 public:
  SendPacketsStatus() {}
  ~SendPacketsStatus() {}
  void CalculateSendFractionLost(uint32_t highest_seqnumber,
                                 uint32_t cumulative_lost);
  uint32_t send_fraction_lost() { return send_fraction_lost_; }
  uint32_t expect_send_packets() { return expect_send_packets_; }
  uint32_t send_lost() { return send_lost_; }
  uint32_t last_cumulative_lost() { return last_cumulative_lost_; }

  int send_packets() { return send_packets_; }
  int send_bytes() { return send_bytes_; }
  uint32_t last_ts() { return last_ts_; }

  void set_send_packets(int increase_number) {
    send_packets_ += increase_number;  
  }
  void set_send_bytes(int increase_number) {
    send_bytes_ += increase_number;  
  }
  void set_last_ts(uint32_t last_ts) {
    last_ts_ += last_ts;  
  }

 private:
  long long last_update_time_{0};
  uint32_t current_highest_seqnumber_{0};
  uint32_t last_cumulative_lost_{0};
  uint32_t send_fraction_lost_{0};

  int send_packets_{0};
  int send_bytes_{0};
  uint32_t last_ts_{0};
  
  /* used for calculate total send fraction lost of every ssrcs */
  uint32_t expect_send_packets_{0}; // expected number of send packets in unittime
  uint32_t send_lost_{0}; // real number of packets lost of send in unittime
};

class NetworkStatusManager final {
 public:
  class Key final {
   public:
    Key(int32_t session_id, int32_t stream_id)
      : session_id_(session_id), stream_id_(stream_id) {}
    int32_t session_id() const { return session_id_; }
    int32_t stream_id() const { return stream_id_; }
   private:
    int32_t session_id_;
    int32_t stream_id_;
  };
  
  NetworkStatusManager() = delete;
  static void Init(int32_t session_id, int32_t stream_id);
  static std::shared_ptr<NetworkStatus> Get(int32_t session_id, 
                                            int32_t stream_id);
  static void Destroy(int32_t session_id, int32_t stream_id);
  static void UpdateStreamIncommingTime(int32_t session_id, int32_t stream_id);
  static long GetStreamLastTime(int32_t session_id, int32_t stream_id);

  static void TraverseStreamMap(std::vector<NetworkStatusManager::Key>* remove_stream);
  static void GetProbeNetResponse(const int& session_id,
                                  const int& stream_id,
                                  olive::ProbeNetTestResult* probe_result);
  static void UpdateTimeOfStreamHighEnergy(const int& session_id, const int& stream_id, opus_int32 audio_energy);
  
 private:
  static std::map<Key, std::shared_ptr<NetworkStatus>> map_;
  static std::mutex mutex_;

  static std::map<Key, long> stream_update_map_;

  friend bool operator< (const Key &, const Key &);
};

class NetworkStatus  {
 public:
  NetworkStatus(uint32_t session_id, uint32_t stream_id);

  virtual ~NetworkStatus ();

  uint32_t session_id() { return session_id_; }

  uint32_t stream_id() { return stream_id_; }

  uint32_t recv_fraction_lost_of_video() {
    return recv_fraction_lost_of_video_;
  }

  uint32_t recv_fraction_lost_of_audio() {
    return recv_fraction_lost_of_audio_;
  }

  uint32_t GetSendFractionLost(bool is_audio = false, uint32_t ssrc = 0);

  /*
   * One packet received, need to update the network status information.
   */
  void ReceivingPacket(bool is_video);

  /*
   * calculate rtcp receive count
   */
  void ReceivingRTCPPacket(bool is_video);

  /**
   * One packet will be send, need to update the network status information.
   * @param[in] is_video  incidator video or audio packet ?
   * @param[in] ssrc      the sender ssrc
   * @param[in] len       length of packet
   * @param[in] timestamp the timestamp of the last packet sent
   */
  void SendingPacket(bool is_video, uint32_t ssrc, int len, uint32_t timestamp);

  /**
   * calculate rtcp send count
   */
  void SendingRTCPPacket(bool is_video);

  /*
   * Increase the number of NACK packet sent & received. Only used for statistical
   * purposes.The statistical result will be output to varz.
   */
  void IncreaseNackCount(bool is_send);

  /*
   * Update average RTT in unit time.
   *
   * @param[in] rtt    : Calculated rtt value
   * @param[in] ssrc   : The ssrc that the rtt belongs to.
   */
  void UpdateRTT(int64_t rtt, uint32_t ssrc);

  /*
   * Update the send or receive bitrate that is actual used.
   *
   * @param[in] is_send The direction of stream data
   * @param[in] bitrate Actual bandwidth used.
   */
  void UpdateBitrate(bool is_send, uint32_t bitrate);

  uint32_t GetBitrate(bool is_send);

  /*
   * Update target bitrate estimated by receiver 
   *
   * @param[in] is_video Indicate video or audio ?
   * @param[in] target_bitrate The target bitrate estimated recently. 
   */
  void UpdateTargetBitrate(bool is_video, uint32_t target_bitrate);

  /*
   * Get current stream's target bitrate include audio and video.
   *
   * @return The sum of audio and video target bitrates. -1 is no proper
   *         estimate.
   */
  int32_t GetTargetBitrate() const;

  /* 
   * This function is used to update all the StatusZ's information of the
   * current stream
   */
  void UpdateStatusZ();

  /*
   * Get the total packet loss rate, the return value is one of the send
   * or receive fraction lost, the bigger one will be choose.
   *
   * @return fraction lost
   */
  int GetNetworkFractionLost();

  /*
   * Get the network status info include rtt, target bitrate , fraction lost and
   * the general network status.
   *
   * @return connect status info
   */
  olive::ConnectStatus GetConnectStatus();

  /*
   * Update the jitter values for varz output, the jitter values are get from the 
   * sr/rr packet,divided into get from server or client, represent video or audio.
   *
   * @param[in] report_source : indicator where the jitter report come from ?
   * @param[in] is_video      : indicator the type of RR packet belongs to. 
   * @param[in] jitter_value  : the jitter value get from RR packet.
   * @param[in] local_ssrc    : the local ssrc that the RR packet report.
   */
  void UpdateJitters(REPORT_SOURCE report_source, 
                     bool is_video, 
                     uint32_t jitter_value,
                     uint32_t local_ssrc = 0);

  /*
   * This function is used to statistic the number of packet lost.
   * We're receiver and in this case we'll statistic the fraction lost of packets 
   * receiving, and update it every 3 seconds. 
   * The result is saved to recv_fraction_lost_;
   *
   * @param[in] is_video indicate the seqnumber is audio or video?
   * @param[in] seqnumber the sequence number of incoming packet.
   */
  void StatisticRecvFractionLost(bool is_video, uint32_t seqnumber);
 
  /*
   * We're sender and in this case we'll statistic the fraction lost of packets
   * sending of every ssrc, and update it every 3 seconds.
   * We'll call this function when there is a RR message arrived.
   *
   * @param[in] is_video indicate :the seqnumber is audio or video ?
   * @param[in] highest_seqnumber :the highest sequence number received
   * @param[in] lost_count :the cumulative number of packets lost get from RR 
   * @param[in] ssrc :the local ssrc
   */ 
  void StatisticSendFractionLost(bool is_video,
                                 uint32_t highest_seqnumber,
                                 uint32_t cumulative_lost,
                                 uint32_t ssrc);

  /*
   * Get the number of packets send of the stream indicate by ssrc
   */
  int GetSendPackets(uint32_t ssrc);

  int GetSendBytes(uint32_t ssrc);
  
  uint32_t GetLastTs(uint32_t ssrc);

  void InitWithSsrcs(std::vector<uint32_t> extended_video_ssrcs,
                     uint32_t default_video_ssrc,
                     uint32_t default_audio_ssrc);

  /**
   * Get the average rtt value of all local ssrcs in real time
   *
   * @return the latest average rtt value of all local ssrcs
   */
  int64_t GetLastestRttOfSsrcs();

  /**
   * Method used to get the average rtt value of all local_ssrcs.
   *
   * @return The average rtt value.
   */
  int64_t GetAllSsrcsAverageRtt();

  /**
   * If has no video packet entry or audio energy is low, we all think it has no audio.
   */
  bool CheckHasAudioPacket(const int& stream_id);

  /**
   * Check if has video packet entry
   */
  bool CheckHasVideoPacket();

  /**
   * Set Network status
   */
  void SetNetworkStatus(olive::ProbeNetTestResult* probe_result);

  /**
   * When the audio of the stream is higher than ENERGY_LOW(1000000)
   * update the current time. If there is no update for a long time, 
   * we think the stream has low audio or mute.
   */
  void UpdateHighEnergyTime(const int& stream_id);

 private:

  /*
   * Get the network status to display on StatusZ and front end,
   * The network status is a level from NET_GREAT to NET_CANT_CONNECT.
   * Calculated by fraction lost , estimated target bitrate and average RTT
   * value.
   *
   * @return The network status level
   */
  NETWORK_STATUS GetNetworkStatus();
  
  /*
   * Convert NETWORK_STATUS to string
   *
   * @return The network status level string
   */
  std::string ConvertNetworkStatusToString(NETWORK_STATUS network_status);

  /**
   * Calculate and update the total send lost rate of all ssrcs.
   */
  void UpdateTotalSendFractionLost();

  /*
   * Estimate the network status by target bitrate, that's the average bitrate
   * updated every 3 seconds.
   *
   * @param[out] return the estimated network status level.
   */
  NETWORK_STATUS EstimateNetworkStatusByBandwidth();
  NETWORK_STATUS EstimateNetworkStatusByFractionLost();
  NETWORK_STATUS EstimateNetworkStatusByRTT();

  NETWORK_STATUS NetworkStatusLevel(int);
 
  void StatisticRecvFractionOfVideo(uint32_t seqnumber);

  void StatisticRecvFractionOfAudio(uint32_t seqnumber);

  void StatisticSendFractionOfVideo(uint32_t highest_seqnumber,
                                    uint32_t cumulative_lost);
  void StatisticSendFractionOfAudio(uint32_t highest_seqnumber,
                                    uint32_t cumulative_lost);

  NETWORK_STATUS network_status_;   // network status level 
  
  std::mutex rtt_status_mtx_;
  std::map<uint32_t, RttStatus> map_rtt_status_;

  BitrateStatus bitrate_status_;

  uint32_t audio_local_ssrc_;

  std::atomic <uint32_t> audio_queue_size_{0};
  std::atomic <uint32_t> video_queue_size_{0};
  /* get from RR/SR message */ 
  std::atomic <uint32_t> send_packets_{0};  
  std::atomic <uint32_t> recv_packets_{0};  // The count of packet received

  std::atomic <uint32_t> recv_packets_audio_{0};
  std::atomic <uint32_t> recv_packets_video_{0};

  std::atomic <uint32_t> recv_rtcp_packets_audio_{0};
  std::atomic <uint32_t> recv_rtcp_packets_video_{0};

  std::atomic <uint32_t> audio_send_packets_{0};
  std::atomic <uint32_t> video_send_packets_{0};

  std::atomic <uint32_t> send_rtcp_packets_audio_{0};
  std::atomic <uint32_t> send_rtcp_packets_video_{0};

  // We want to know the period of time receive the packets count,so we save the
  // old packets count
  uint32_t recv_packets_audio_old_{0};
  uint32_t recv_packets_video_old_{0};
  long long last_recv_update_time_{0};

  uint32_t recv_rtcp_packets_audio_old_{0};
  uint32_t recv_rtcp_packets_video_old_{0};
  long long last_recv_rtcp_update_time_{0};

  uint32_t audio_send_packets_old_{0};
  uint32_t video_send_packets_old_{0};
  long long last_send_update_time_{0};

  uint32_t send_rtcp_packets_audio_old_{0};
  uint32_t send_rtcp_packets_video_old_{0};
  long long last_send_rtcp_update_time_{0};

    // cumulative packet lost
  std::atomic <uint32_t> cumulate_send_lost_{0};
  std::atomic <uint32_t> cumulate_recv_lost_{0};
  std::atomic <uint32_t> video_recv_cumulative_lost_{0};
  std::atomic <uint32_t> audio_recv_cumulative_lost_{0};
    // jitters
  std::atomic <uint32_t> server_report_audio_jitter_{0};
  std::atomic <uint32_t> server_report_video_jitter_{0};
  std::atomic <uint32_t> client_report_audio_jitter_{0};
  std::map <uint32_t /*ssrc*/, uint32_t /*jitter-value*/>
      client_report_video_jitters_;
  std::mutex client_report_video_jitters_mtx_;

  uint32_t session_id_;
  uint32_t stream_id_;

  int send_nack_count_{0};               // The number of nack packet sent
  int recv_nack_count_{0};               // The number of nack packet received

    // used to calculate total number of fraction lost
  uint32_t expect_video_recv_packets_{0};
  uint32_t expect_audio_recv_packets_{0};
  uint32_t video_lost_of_recv_{0};    // video lost count in unit time
  uint32_t audio_lost_of_recv_{0};
  uint32_t number_of_video_received_{0};
  uint32_t number_of_audio_received_{0};
    // The max seqnumber of current received.
  uint32_t max_recv_seqn_of_video_{0}; 
  uint32_t max_recv_seqn_of_audio_{0};
  uint32_t min_recv_seqn_of_video_{0}; 
    // The min seqnumber of current received.
  uint32_t min_recv_seqn_of_audio_{0};
    // last time of fraction update
  long long last_time_of_video_recv_fraction_update_{0};
  long long last_time_of_audio_recv_fraction_update_{0};
  //long long last_time_of_video_send_fraction_update_{0};
  //long long last_time_of_audio_send_fraction_update_{0};
    // receive fraction lost in unittime
  std::atomic <uint32_t> recv_fraction_lost_{0}; // sum of audio and video
  std::atomic <uint32_t> recv_fraction_lost_of_video_{0};
  std::atomic <uint32_t> recv_fraction_lost_of_audio_{0};
    // send fraction lost in unittime
  std::atomic <uint32_t> send_fraction_lost_{0}; // sum of every ssrc
    // vector to save received sequence number
  std::vector<uint32_t> recv_video_seqn_vector_;
  std::vector<uint32_t> recv_audio_seqn_vector_;
    // map to record the lost packets
  std::map<uint32_t, long long> lost_of_recv_video_map_;  
  std::map<uint32_t, long long> lost_of_recv_audio_map_;  
    // record number of video send packets and bytes for every ssrc.
  std::mutex send_packets_status_mtx_;
  std::map<uint32_t /* ssrc */, SendPacketsStatus> map_send_packets_status_;

    //map to record the time of low audio energy for every stream
  std::mutex stream_energy_mtx_;
  std::map<uint32_t, long> map_stream_energy_;
};

} /* namespace orbit */

