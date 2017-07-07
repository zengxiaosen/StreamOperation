/*
 * Copyright 2016 All Rights Reserved.
 * Author: caixinyu@orangelab.cn (Cai Xinyu)
 *
 * grpc_client.cc
 * ---------------------------------------------------------------------------
 * A grpc client by which can easily use the service provided by server.
 * ---------------------------------------------------------------------------
 */

#include "grpc_client.h"

#include "glog/logging.h"

namespace olive {
namespace client {
// Sometime, we must set stream id, but it is never used,
// so, we set a stream id.
// We cannot use a special number like 0, 1, -1, because
// they may cause some special behaviour.
static const stream_id_t NORMAL_STREAM_ID = 56475;

session_id_t GrpcClient::CreateSession(session_type_t session_type,
                                       const std::string &rtmp_location) {
  olive::CreateSessionRequest req;
  req.set_type(session_type);
  req.set_rtmp_location(rtmp_location);

  olive::CreateSessionResponse resp;
  grpc::ClientContext context;
  grpc::Status stat = stub_->CreateSession(&context, req, &resp);

  if (stat.ok()) {
    if (resp.status() == olive::CreateSessionResponse::OK) {
      return resp.session_id();
    } else {
      LOG(ERROR) << "Cannot create session because of wrong logic.";
      return -1;
    }
  } else {
    LOG(ERROR) << "Cannot create session because of grpc error, "
               << "error code = " << stat.error_code() << ", "
               << "error message = " << stat.error_message() << ".";
    return -1;
  }
}

int GrpcClient::CloseSession(session_id_t session_id) {
  olive::CloseSessionRequest req;
  req.set_session_id(session_id);

  olive::CloseSessionResponse resp;

  grpc::ClientContext context;

  grpc::Status stat = stub_->CloseSession(&context, req, &resp);

  if (stat.ok()) {
    return 0;
  } else {
    LOG(ERROR) << "Cannot close session because of grpc error, "
               << "error code = " << stat.error_code() << ", "
               << "error message = " << stat.error_message() << ".";
    return -1;
  }
}

stream_id_t GrpcClient::CreateStream(session_id_t session_id,
                                     const CreateStreamOption &stream_option) {
  olive::CreateStreamRequest req;
  req.set_session_id(session_id);
  req.mutable_option()->set_use_relay(stream_option.use_relay());
  req.mutable_option()->mutable_relay_server_info()->CopyFrom(stream_option.relay_server_info());
  req.mutable_option()->CopyFrom(stream_option);

  olive::CreateStreamResponse resp;

  grpc::ClientContext context;

  grpc::Status stat = stub_->CreateStream(&context, req, &resp);

  if (stat.ok()) {
    if (resp.status() == olive::CreateStreamResponse::OK) {
      return resp.stream_id();
    } else {
      LOG(ERROR) << "Cannot create stream because of wrong logic.";
      return -1;
    }
  } else {
    LOG(ERROR) << "Cannot create stream because of grpc error, "
               << "error code = " << stat.error_code() << ", "
               << "error message = " << stat.error_message() << ".";
    return -1;
  }
}

int GrpcClient::CloseStream(session_id_t session_id, stream_id_t stream_id) {
  olive::CloseStreamRequest req;
  req.set_session_id(session_id);
  req.set_stream_id(stream_id);

  olive::CloseStreamResponse resp;
  grpc::ClientContext context;
  grpc::Status stat = stub_->CloseStream(&context, req, &resp);

  if (stat.ok()) {
    if (resp.status() == olive::CloseStreamResponse::OK) {
      return 0;
    } else {
      LOG(ERROR) << "Cannot close stream because of wrong logic.";
      return -1;
    }
  } else {
    LOG(ERROR) << "Cannot close stream because of grpc error, "
               << "error code = " << stat.error_code() << ", "
               << "error message = " << stat.error_message() << ".";
    return -1;
  }
}

int GrpcClient::SendIceCandidate(session_id_t session_id, stream_id_t stream_id,
                                 const IceCandidate &ice_candidate) {
  olive::SendSessionMessageRequest req;
  req.set_session_id(session_id);
  req.set_stream_id(stream_id);
  req.set_type(ICE_CANDIDATE);
  req.mutable_candidate()->CopyFrom(ice_candidate);

  olive::SendSessionMessageResponse resp;
  grpc::ClientContext context;
  grpc::Status stat = stub_->SendSessionMessage(&context, req, &resp);

  if (stat.ok()) {
    if (resp.status() == olive::SendSessionMessageResponse::OK) {
      return 0;
    } else {
      LOG(ERROR) << "Cannot send ice candidate because of wrong logic.";
      return -1;
    }
  } else {
    LOG(ERROR) << "Cannot send ice candidate because of grpc error, "
               << "error code = " << stat.error_code() << ", "
               << "error message = " << stat.error_message() << ".";
    return -1;
  }
}

int GrpcClient::RecvIceCandidate(session_id_t session_id, stream_id_t stream_id,
                                 std::vector<IceCandidate> *vec) {
  if (!vec) {
    LOG(ERROR) << "NULL Pointer";
    return -1;
  }

  vec->clear();

  olive::RecvSessionMessageRequest req;
  req.set_session_id(session_id);
  req.set_stream_id(stream_id);
  req.set_type(ICE_CANDIDATE);

  olive::RecvSessionMessageResponse resp;
  grpc::ClientContext context;
  grpc::Status stat = stub_->RecvSessionMessage(&context, req, &resp);

  if (stat.ok()) {
    int size = resp.ice_candidate_size();
    for (int i = 0; i != size; ++i) {
      vec->push_back(resp.ice_candidate(i));
    }
    return 0;
  } else {
    LOG(ERROR) << "Cannot receive ice candidate because of grpc error, "
               << "error code = " << stat.error_code() << ", "
               << "error message = " << stat.error_message() << ".";
    return -1;
  }
}

std::string GrpcClient::SendSdpOffer(session_id_t session_id,
                                     stream_id_t stream_id,
                                     const std::string &sdp_offer) {
  olive::SendSessionMessageRequest req;
  req.set_session_id(session_id);
  req.set_stream_id(stream_id);
  req.set_type(SDP_OFFER);
  req.set_sdp_offer(sdp_offer);

  olive::SendSessionMessageResponse resp;
  grpc::ClientContext context;
  grpc::Status stat = stub_->SendSessionMessage(&context, req, &resp);

  if (stat.ok()) {
    if (resp.status() == olive::SendSessionMessageResponse::OK) {
      return resp.sdp_answer();
    } else {
      LOG(ERROR) << "Cannot send sdp offer because of wrong logic.";
      return "";
    }
  } else {
    LOG(ERROR) << "Cannot send sdp offer because of grpc error, "
               << "error code = " << stat.error_code() << ", "
               << "error message = " << stat.error_message() << ".";
    return "";
  }
}

int GrpcClient::RaiseHand(session_id_t session_id, stream_id_t stream_id) {
  olive::SendSessionMessageRequest req;
  req.set_session_id(session_id);
  req.set_stream_id(stream_id);
  req.set_type(MESSAGE);
  req.set_message("raise_hand");

  olive::SendSessionMessageResponse resp;
  grpc::ClientContext context;
  grpc::Status stat = stub_->SendSessionMessage(&context, req, &resp);

  if (stat.ok()) {
    if (resp.message_answer() == "OK") {
      return 0;
    } else {
      LOG(ERROR) << "Cannot raise hand because of wrong logic.";
      return -1;
    }
  } else {
    LOG(ERROR) << "Cannot raise hand because of grpc error, "
               << "error code = " << stat.error_code() << ", "
               << "error message = " << stat.error_message() << ".";
    return -1;
  }
}

int GrpcClient::Mute(session_id_t session_id,
                     stream_id_t stream_id,
                     bool mute,
                     const std::vector<stream_id_t> &streams) {
  olive::SendSessionMessageRequest req;
  req.set_session_id(session_id);
  req.set_stream_id(stream_id);
  req.set_type(MUTE);
  MuteStreams *mute_streams = req.mutable_mute_streams();
  for (stream_id_t i : streams) {
    mute_streams->add_stream_ids(i);
  }
  mute_streams->set_mute(mute);

  olive::SendSessionMessageResponse resp;
  grpc::ClientContext context;
  grpc::Status stat = stub_->SendSessionMessage(&context, req, &resp);

  if (stat.ok()) {
    return 0;
  } else {
    LOG(ERROR) << "Cannot mute because of grpc error, "
               << "error code = " << stat.error_code() << ", "
               << "error message = " << stat.error_message() << ".";
    return -1;
  }
}

int GrpcClient::SetStreamInfo(session_id_t session_id, stream_id_t stream_id,
                              const std::string &info) {
  olive::SendSessionMessageRequest req;
  req.set_session_id(session_id);
  req.set_stream_id(stream_id);
  req.set_type(STREAM_INFO);
  req.set_message(info);

  olive::SendSessionMessageResponse resp;
  grpc::ClientContext context;
  grpc::Status stat = stub_->SendSessionMessage(&context, req, &resp);

  if (stat.ok()) {
    if (resp.message_answer() == "OK") {
      return 0;
    } else {
      LOG(ERROR) << "Cannot set stream info because of wrong logic.";
      return -1;
    }
  } else {
    LOG(ERROR) << "Cannot set stream info because of grpc error, "
               << "error code = " << stat.error_code() << ", "
               << "error message = " << stat.error_message() << ".";
    return -1;
  }
}

int GrpcClient::SetPublisher(session_id_t session_id, stream_id_t stream_id) {
  olive::SendSessionMessageRequest req;
  req.set_session_id(session_id);
  req.set_stream_id(stream_id);

  olive::SendSessionMessageResponse resp;
  grpc::ClientContext context;
  grpc::Status stat = stub_->SendSessionMessage(&context, req, &resp);

  if (stat.ok()) {
    if (resp.message_answer() == "OK") {
      return 0;
    } else {
      LOG(ERROR) << "Cannot set publisher because of wrong logic.";
      return -1;
    }
  } else {
    LOG(ERROR) << "Cannot set publisher because of grpc error, "
               << "error code = " << stat.error_code() << ", "
               << "error message = " << stat.error_message() << ".";
    return -1;
  }
}

int GrpcClient::GetConnectStatus(session_id_t session_id,
                                 stream_id_t stream_id,
                                 std::vector<ConnectStatus> *vec) {
  if (!vec) {
    LOG(ERROR) << "NULL Pointer";
    return -1;
  }

  vec->clear();

  olive::RecvSessionMessageRequest req;
  req.set_session_id(session_id);
  req.set_stream_id(stream_id);
  req.set_type(CONNECT_STATUS);

  olive::RecvSessionMessageResponse resp;
  grpc::ClientContext context;
  grpc::Status stat = stub_->RecvSessionMessage(&context, req, &resp);

  if (stat.ok()) {
    int size = resp.connect_status_size();
    for (int i = 0; i != size; ++i) {
      vec->push_back(resp.connect_status(i));
    }
    return 0;
  } else {
    LOG(ERROR) << "Cannot get connect status because of grpc error, "
               << "error code = " << stat.error_code() << ", "
               << "error message = " << stat.error_message() << ".";
    return -1;
  }
}

int GrpcClient::SendSessionMessage(session_id_t session_id, stream_id_t stream_id) {
  std::string message = "{\"room_id\":\"123456789\",\"room_type\":\"VIDEO_BRIDGE\"}";
  
  olive::SendSessionMessageRequest req;
  req.set_message(message);
  req.set_session_id(session_id);
  req.set_type(STREAM_INFO);

  olive::SendSessionMessageResponse resp;
  grpc::ClientContext context;
  grpc::Status stat = stub_->SendSessionMessage(&context, req, &resp);

  return 0;
}

int GrpcClient::QueryProbeNetTestResult(session_id_t session_id, stream_id_t stream_id) {
  olive::SendSessionMessageRequest req;
  req.set_session_id(session_id);
  req.set_stream_id(stream_id);
  req.set_type(PROBE_NET);

  olive::SendSessionMessageResponse resp;
  grpc::ClientContext context;
  grpc::Status stat = stub_->SendSessionMessage(&context, req, &resp);

  LOG(INFO) << "response=" << resp.DebugString();
  LOG(INFO) << "has_probe_net_result=" << resp.has_probe_net_result();
  return 0;
}

}  // namespace client
}  // namespace olive

