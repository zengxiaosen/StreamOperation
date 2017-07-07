/*
 * Copyright 2016 All Rights Reserved.
 * Author: caixinyu@orangelab.cn (Cai Xinyu)
 *
 * grpc_client.h
 * ---------------------------------------------------------------------------
 * A grpc client by which can easily use the service provided by server.
 * ---------------------------------------------------------------------------
 */

#pragma once

#include <vector>
#include <sstream>

#include <grpc/grpc.h>
#include <grpc++/channel.h>
#include <grpc++/client_context.h>
#include <grpc++/create_channel.h>
#include <grpc++/security/credentials.h>
#include "stream_service/proto/stream_service.grpc.pb.h"

namespace olive {
namespace client {

typedef CreateSessionRequest_SessionCreationType session_type_t;

typedef int32_t session_id_t;

typedef int32_t stream_id_t;

class GrpcClient {
 public:
  // Construct a GrpcClient by ip and port
  GrpcClient(const std::string &ip, uint16_t port)
      : channel_(grpc::CreateChannel(IpPort(ip, port), grpc::InsecureCredentials())),
        stub_(olive::StreamService::NewStub(channel_)) {}

  // Construct a GrpcClient by ip and port
  GrpcClient(const std::string &ip, const std::string &port)
      : channel_(grpc::CreateChannel(ip + ":" + port, grpc::InsecureCredentials())),
        stub_(olive::StreamService::NewStub(channel_)) {}

  // Construct a GrpcClient by a string contains both ip and port.
  // eg. "172.0.0.1:10000"
  GrpcClient(const std::string &ip_port)
      : channel_(grpc::CreateChannel(ip_port, grpc::InsecureCredentials())),
        stub_(olive::StreamService::NewStub(channel_)) {}

  // Create a session and return its session id.
  // return: >0: session id, -1: error
  session_id_t CreateSession(session_type_t session_type,
                             const std::string &rtmp_location);

  // Close a session.
  // return: 0: succeed , -1: error
  int CloseSession(session_id_t session_id);

  // Create a stream in a session and return its stream id.
  // return: >0: stream id , -1: error
  stream_id_t CreateStream(session_id_t session_id,
                           const CreateStreamOption &create_stream_option);

  // Close a stream in a session.
  // return: 0: succeed , -1: error
  int CloseStream(session_id_t session_id, stream_id_t stream_id);

  // Send an ice candidate to the server side of a stream.
  // return: 0: succeed , -1: error
  int SendIceCandidate(session_id_t session_id, stream_id_t stream_id,
                       const IceCandidate &ice_candidate);

  // Recive some ice candidates from server side for a stream.
  // vec will be cleared.
  // return: 0: succeed , -1: error
  int RecvIceCandidate(session_id_t session_id, stream_id_t stream_id,
                       std::vector<IceCandidate> *vec);

  // Send sdp offer and return sdp answer.
  // return: not empty: sdp answer , empty: error
  std::string SendSdpOffer(session_id_t session_id, stream_id_t stream_id,
                           const std::string &sdp_offer);

  // Raise hand.
  // return: 0: succeed , -1: error
  int RaiseHand(session_id_t session_id, stream_id_t stream_id);

  // Mute.
  // return: 0: succeed , -1: error
  int Mute(session_id_t session_id, stream_id_t stream_id,
           bool mute, const std::vector<stream_id_t> &streams);

  // Set stream info for a stream.
  // return: 0: succeed , -1: error
  int SetStreamInfo(session_id_t session_id, stream_id_t stream_id,
                    const std::string &info);

  // Set a stream as the publisher of the session.
  // return: 0: succeed , -1: error
  int SetPublisher(session_id_t session_id, stream_id_t stream_id);

  // Get connect status of a session.
  // return: 0: succeed , -1: error
  int GetConnectStatus(session_id_t session_id, stream_id_t stream_id,
                       std::vector<ConnectStatus> *vec);

  int SendSessionMessage(session_id_t session_id, stream_id_t stream_id);

  int QueryProbeNetTestResult(session_id_t session_id, stream_id_t stream_id);
 private:
  std::string IpPort(const std::string &ip, uint16_t port) {
    std::ostringstream ss;
    ss << ip << ":" << port;
    return ss.str();
  }

 private:
  std::shared_ptr<grpc::Channel> channel_;
  std::unique_ptr<olive::StreamService::Stub> stub_;
};

}  // namespace client
}  // namespace olive

