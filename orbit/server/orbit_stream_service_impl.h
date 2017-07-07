/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * orbit_stream_service_impl.h
 * ---------------------------------------------------------------------------
 * Implements the stream service GRPC interface:
 *  -- Session management
 *     CreateSession - create a video/audio session for webrtc
 *     CreateStream  - create a video/audio stream and take part into a session
 *  -- Message management
 *     Send/RecvMessage - send and receive the messages to control the webrtc session 
 * ---------------------------------------------------------------------------
 */
#pragma once

#include "stream_service/proto/stream_service.grpc.pb.h"

namespace olive {
using namespace google::protobuf;

class OrbitStreamServiceImpl : public StreamService::Service {
 public:
  void Init();
  virtual ~OrbitStreamServiceImpl();

  virtual grpc::Status CreateSession(grpc::ServerContext* context,
                                     const CreateSessionRequest* request,
                                     CreateSessionResponse* response);

  virtual grpc::Status CloseSession(grpc::ServerContext* context,
                                    const CloseSessionRequest* request,
                                    CloseSessionResponse* response);

  virtual grpc::Status CreateStream(grpc::ServerContext* context,
                                    const CreateStreamRequest* request,
                                    CreateStreamResponse* response);

  virtual grpc::Status CloseStream(grpc::ServerContext* context,
                                   const CloseStreamRequest* request,
                                   CloseStreamResponse* response);

  virtual grpc::Status SendSessionMessage(grpc::ServerContext* context,
                                          const SendSessionMessageRequest* request,
                                          SendSessionMessageResponse* response);

  virtual grpc::Status RecvSessionMessage(grpc::ServerContext* context,
                                          const RecvSessionMessageRequest* request,
                                          RecvSessionMessageResponse* response);
};

}  // namespace olive
