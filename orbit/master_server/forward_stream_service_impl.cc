/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * forward_stream_service_impl.cc
 * ---------------------------------------------------------------------------
 * Implements a forward service.
 *  (just forward the RPC request to backend servers)
 * ---------------------------------------------------------------------------
 */

#include "forward_stream_service_impl.h"
#include "gflags/gflags.h"
#include "stream_service/orbit/http_server/rpc_call_stats.h"

DEFINE_int32(rpc_retries, 3, "Specify the retries number for rpc.");

namespace orbit {
  grpc::Status ForwardStreamServiceImpl::CreateSession(grpc::ServerContext* context,
                                                       const CreateSessionRequest* request,
                                                       CreateSessionResponse* response) {
    RpcCallStats _rpc_stat("ForwardStreamService_CreateSession");

    VLOG(2) << request->DebugString();
    int retries = 0;
    Status status;
    while (retries < FLAGS_rpc_retries) {
      registry::ServerInfo server_info = AllocateServer();
      if (server_info.host().empty()) {
        _rpc_stat.Fail();
        LOG(ERROR) << "CreateSession can not allocate server...";
        return Status::CANCELLED;
      }
      string server = StringPrintf("%s:%d", server_info.host().c_str(), server_info.port());
      std::shared_ptr<StreamService::Stub> stub = InitStub(server);

      ClientContext client_context;
      std::chrono::system_clock::time_point deadline =
        std::chrono::system_clock::now() + std::chrono::seconds(1);
      client_context.set_deadline(deadline);
      
      Status status = stub->CreateSession(&client_context, *request, response);
      retries++;
      if (status.ok()) {
        LOG(INFO) << "response=" << response->DebugString();
        int session_id = response->session_id();
        OrbitMasterServerManager* manager = Singleton<OrbitMasterServerManager>::GetInstance();
        manager->SetSessionServer(session_id, server_info);
        return status;
      }
      if (!status.ok() && retries >= 3 &&
          status.error_code() == StatusCode::DEADLINE_EXCEEDED) {
        DestroyStub(server);
        _rpc_stat.Fail();
      }
    }
    return status;
  }

  grpc::Status ForwardStreamServiceImpl::CloseSession(grpc::ServerContext* context,
                                                      const CloseSessionRequest* request,
                                                      CloseSessionResponse* response) {
    RpcCallStats _rpc_stat("ForwardStreamService_CloseSession");

    VLOG(2) << request->DebugString();
    int session_id = request->session_id();
    int retries = 0;
    Status status;
    registry::ServerInfo server_info = GetServer(session_id);
    if (server_info.host().empty()) {
        _rpc_stat.Fail();
      LOG(ERROR) << "CloseSession find no session_id=" << session_id;
      return Status::CANCELLED;
    }
    string server = StringPrintf("%s:%d", server_info.host().c_str(), server_info.port());
    while (retries < FLAGS_rpc_retries) {
      std::shared_ptr<StreamService::Stub> stub = InitStub(server);

      ClientContext client_context;
      std::chrono::system_clock::time_point deadline =
        std::chrono::system_clock::now() + std::chrono::seconds(1);
      client_context.set_deadline(deadline);
      
      status = stub->CloseSession(&client_context, *request, response);
      retries++;
      if (status.ok()) {
        ReclaimServer(session_id);          
        return status;
      }
    }
    if (status.error_code() == StatusCode::DEADLINE_EXCEEDED) {
      DestroyStub(server);
      _rpc_stat.Fail();
    }
    return status;
  }

  grpc::Status ForwardStreamServiceImpl::CreateStream(grpc::ServerContext* context,
                                                      const CreateStreamRequest* request,
                                                      CreateStreamResponse* response) {
    RpcCallStats _rpc_stat("ForwardStreamService_CreateStream");

    VLOG(2) << request->DebugString();
    int session_id = request->session_id();
    int retries = 0;
    Status status;
    registry::ServerInfo server_info = GetServer(session_id);
    if (server_info.host().empty()) {
      LOG(ERROR) << "CreateStream find no session_id=" << session_id;
      _rpc_stat.Fail();
      return Status::CANCELLED;
    }
    string server = StringPrintf("%s:%d", server_info.host().c_str(), server_info.port());
    while (retries < FLAGS_rpc_retries) {
      std::shared_ptr<StreamService::Stub> stub = InitStub(server);
      
      ClientContext client_context;
      std::chrono::system_clock::time_point deadline =
        std::chrono::system_clock::now() + std::chrono::seconds(1);
      client_context.set_deadline(deadline);
      
      status = stub->CreateStream(&client_context, *request, response);
      retries++;
      if (status.ok()) {
        return status;
      }
    }
    if (status.error_code() == StatusCode::DEADLINE_EXCEEDED) {
      DestroyStub(server);
      _rpc_stat.Fail();
    }
    return status;
  }

  grpc::Status ForwardStreamServiceImpl::CloseStream(grpc::ServerContext* context,
                                                     const CloseStreamRequest* request,
                                                     CloseStreamResponse* response) {
    RpcCallStats _rpc_stat("ForwardStreamService_CloseStream");

    VLOG(2) << request->DebugString();
    int session_id = request->session_id();
    int retries = 0;
    Status status;
    registry::ServerInfo server_info = GetServer(session_id);
    if (server_info.host().empty()) {
      LOG(ERROR) << "CloseStream find no session_id=" << session_id;
      _rpc_stat.Fail();
      return Status::CANCELLED;
    }
    string server = StringPrintf("%s:%d", server_info.host().c_str(), server_info.port());
    while (retries < FLAGS_rpc_retries) {
      std::shared_ptr<StreamService::Stub> stub = InitStub(server);
      
      ClientContext client_context;
      std::chrono::system_clock::time_point deadline =
        std::chrono::system_clock::now() + std::chrono::seconds(1);
      client_context.set_deadline(deadline);
      
      status = stub->CloseStream(&client_context, *request, response);
      retries++;
      if (status.ok()) {
        return status;
      }
    }
    if (status.error_code() == StatusCode::DEADLINE_EXCEEDED) {
      DestroyStub(server);
      _rpc_stat.Fail();
    }
    return status;
  }

  grpc::Status ForwardStreamServiceImpl::SendSessionMessage(grpc::ServerContext* context,
                                                            const SendSessionMessageRequest* request,
                                                            SendSessionMessageResponse* response) {
    RpcCallStats _rpc_stat("ForwardStreamService_SendSessionMessage");

    VLOG(2) << request->DebugString();
    int session_id = request->session_id();
    int retries = 0;
    Status status;
    registry::ServerInfo server_info = GetServer(session_id);
    if (server_info.host().empty()) {
      LOG(ERROR) << "SendSessionMessage find no session_id=" << session_id;
      _rpc_stat.Fail();
      return Status::CANCELLED;
    }
    string server = StringPrintf("%s:%d", server_info.host().c_str(), server_info.port());
    while (retries < FLAGS_rpc_retries) {
      std::shared_ptr<StreamService::Stub> stub = InitStub(server);
      
      ClientContext client_context;
      std::chrono::system_clock::time_point deadline =
        std::chrono::system_clock::now() + std::chrono::seconds(1);
      client_context.set_deadline(deadline);
      
      status = stub->SendSessionMessage(&client_context, *request, response);
      retries++;
      if (status.ok()) {
        return status;
      }
    }
    if (status.error_code() == StatusCode::DEADLINE_EXCEEDED) {
      DestroyStub(server);
      _rpc_stat.Fail();
    }
    return status;
  }

  grpc::Status ForwardStreamServiceImpl::RecvSessionMessage(grpc::ServerContext* context,
                                                            const RecvSessionMessageRequest* request,
                                                            RecvSessionMessageResponse* response) {
    RpcCallStats _rpc_stat("ForwardStreamService_RecvSessionMessage");

    VLOG(2) << request->DebugString();
    int session_id = request->session_id();
    int retries = 0;
    Status status;
    registry::ServerInfo server_info = GetServer(session_id);
    if (server_info.host().empty()) {
      LOG(ERROR) << "RecvSessionMessage find no session_id=" << session_id;
      _rpc_stat.Fail();
      return Status::CANCELLED;
    }
    string server = StringPrintf("%s:%d", server_info.host().c_str(), server_info.port());
    while (retries < FLAGS_rpc_retries) {
      std::shared_ptr<StreamService::Stub> stub = InitStub(server);
      ClientContext client_context;
      std::chrono::system_clock::time_point deadline =
        std::chrono::system_clock::now() + std::chrono::seconds(1);
      client_context.set_deadline(deadline);
      status = stub->RecvSessionMessage(&client_context, *request, response);
      retries++;
      if (status.ok()) {
        return status;
      }
    }
    if (status.error_code() == StatusCode::DEADLINE_EXCEEDED) {
      DestroyStub(server);
      _rpc_stat.Fail();
    }
    return status;
  }
}  // namespace orbit
