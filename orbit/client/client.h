/*
 * Copyright 2016 All Rights Reserved.
 * Author: caixinyu@orangelab.cn (Cai Xinyu)
 *
 * client.h
 * ---------------------------------------------------------------------------
 * A client by which one can easily communicate with server.
 * ---------------------------------------------------------------------------
 */

#pragma once

#include <stdint.h>

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>

#include "glog/logging.h"

#include "stream_service/orbit/transport_plugin.h"
#include "stream_service/orbit/transport_delegate.h"
#include "stream_service/orbit/sdp_info.h"
#include "stream_service/orbit/rtp/rtp_headers.h"

#include "grpc_client.h"

namespace orbit {
namespace client {

using namespace olive::client;

// If we don not make an empty TransportPlugin doing nothing,
// some logic will not reach.
class DoNothingTransportPlugin final : public TransportPlugin {
};

// We can send packet to server side and listen packet received from server side.
class OrbitClientStream final {
 public:
  OrbitClientStream(GrpcClient &grpc_client,
                    session_id_t session_id, stream_id_t stream_id)
    : grpc_client_(grpc_client),
      session_id_(session_id),
      stream_id_(stream_id),
      transport_delegate_(session_id, stream_id),
      send_len(0),
      recv_len(0){}

  // Check whether the connect between server side and client side.
  bool IsConnectReady() const { return is_connect_ready_; }

  // Make client side and server side connected.
  // If succeed, true will be returned, or false will be returned.
  bool Connect();

  // Send packet to server side.
  // We can only send packet after connect ready.
  void SendPacket(const char *buf, int len, packetType type) {
    if (IsConnectReady()) {
      transport_delegate_.queueData(1, buf, len, type);
    } else {
      LOG(ERROR) << "Cannot send packet, because connect is not ready.";
    }
  }

  // Add a listener for rtp packet received.
  void AddRecvRtpListener(std::function<void(const dataPacket &packet)> listener) {
    std::lock_guard<std::mutex> lock(recv_rtp_listeners_mutex_);
    recv_rtp_listeners_.push_back(listener);
  }

  // Add a listener for rtcp packet received.
  void AddRecvRtcpListener(std::function<void(const dataPacket &packet)> listener) {
    std::lock_guard<std::mutex> lock(recv_rtcp_listeners_mutex_);
    recv_rtcp_listeners_.push_back(listener);
  }

  void AddSendRtpPacket(std::shared_ptr<orbit::StoredPacket> packet) {
    const RtpHeader* rtp_header = reinterpret_cast<const RtpHeader*>(packet->data().c_str());
    uint32_t seqnum = rtp_header->getSeqNumber();
    uint32_t payload_type = rtp_header->getPayloadType();
    uint32_t header_len = rtp_header->getHeaderLength();
    uint32_t packet_len = packet->packet_length() - header_len;

    if (payload_type == VP8_90000_PT ||
        payload_type == VP9_90000_PT ||
        payload_type == H264_90000_PT) {
      if (send_rtp_video_packet.find(seqnum) == send_rtp_video_packet.end()) {
        send_rtp_video_packet.insert(std::pair<uint32_t, std::shared_ptr<orbit::StoredPacket>>(seqnum, packet));

        send_len += packet_len;
      }
    }
  }

  void AddRecvRtpPacket(const orbit::dataPacket& packet) {
    const orbit::RtpHeader* rtp_header = reinterpret_cast<const RtpHeader*>(packet.data);
    uint32_t ssrc = rtp_header->getSSRC();
    uint32_t seqnum = rtp_header->getSeqNumber();
    uint32_t payload_type = rtp_header->getPayloadType();
    uint32_t packet_len = packet.length - rtp_header->getHeaderLength();

    const orbit::dataPacket packet_recv = packet;

    if (payload_type == VP8_90000_PT ||
        payload_type == VP9_90000_PT ||
        payload_type == H264_90000_PT) {
      if (recv_rtp_video_packet.find(seqnum) == recv_rtp_video_packet.end()) {
        recv_rtp_video_packet.insert(std::pair<uint32_t, const orbit::dataPacket>(ssrc, packet_recv));

        recv_len += packet_len;
        LOG(INFO) << "recv RTP recv_len = " << recv_len;
      }
    }
  }

  uint32_t GetSendPacketLength() {
    return send_len;
  }

  uint32_t GetRecvPacketLength() {
    return recv_len;
  }

 private:
  void Convert(const olive::IceCandidate &ice_candidate,
               orbit::CandidateInfo *candidate_info) const;
  void Convert(const orbit::CandidateInfo &candidate_info,
               olive::IceCandidate *ice_candidate) const;
  void CallRecvRtpListeners(const dataPacket &data) const {
    std::lock_guard<std::mutex> lock(recv_rtp_listeners_mutex_);
    for (auto listener : recv_rtp_listeners_) {
      listener(data);
    }
  }
  void CallRecvRtcpListeners(const dataPacket &data) const {
    std::lock_guard<std::mutex> lock(recv_rtcp_listeners_mutex_);
    for (auto listener : recv_rtcp_listeners_) {
      listener(data);
    }
  }
  
  GrpcClient         &grpc_client_;
  const session_id_t  session_id_;
  const stream_id_t   stream_id_;
  TransportDelegate   transport_delegate_;

  std::atomic_bool    is_connect_ready_{false};
  mutable std::mutex  mutex_connect_;

  SdpInfo server_sdp_;
  SdpInfo client_sdp_; // my sdp, I'm client.

  std::string ice_username_;
  std::string ice_password_;

  std::vector<std::function<void(const dataPacket &)>> recv_rtp_listeners_;
  mutable std::mutex recv_rtp_listeners_mutex_;

  std::vector<std::function<void(const dataPacket &)>> recv_rtcp_listeners_;
  mutable std::mutex recv_rtcp_listeners_mutex_;

  DoNothingTransportPlugin plugin_;

  uint32_t send_len;
  uint32_t recv_len;
  std::map<uint32_t, std::shared_ptr<orbit::StoredPacket>> send_rtp_video_packet;
  std::map<uint32_t, const orbit::dataPacket> recv_rtp_video_packet;
};
}  // namespace client
}  // namespace orbit

