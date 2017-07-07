/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * forward_stream_service_impl.h
 * ---------------------------------------------------------------------------
 * Defines a forward service (just forward the RPC request to backend servers).
 * ---------------------------------------------------------------------------
 */
#pragma once

#include "stream_service/proto/registry.grpc.pb.h"
#include "stream_service/proto/stream_service.grpc.pb.h"
#include "stream_service/orbit/base/monitor/system_info.pb.h"
#include "stream_service/orbit/http_server/exported_var.h"

// For gRpc
#include <grpc/grpc.h>
#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>

// For gRpc client
#include <grpc++/channel.h>
#include <grpc++/client_context.h>
#include <grpc++/create_channel.h>
#include <grpc++/security/credentials.h>
#include <grpc++/support/status.h>
#include <grpc++/support/status_code_enum.h>

#include "orbit_master_server_manager.h"
#include "registry_service_impl.h"

namespace orbit {
  using grpc::Channel;
  using grpc::ClientContext;
  using grpc::Status;
  using grpc::StatusCode;
  using namespace olive;

  class ForwardStreamServiceImpl : public StreamService::Service {
  public:
    void Init() {
      // TODO: initialize the variables.
      stub_create_.reset(new ExportedVar("stub_create"));
    }

    virtual grpc::Status CreateSession(grpc::ServerContext* context,
                                       const CreateSessionRequest* request,
                                       CreateSessionResponse* response) override;
    
    virtual grpc::Status CloseSession(grpc::ServerContext* context,
                                      const CloseSessionRequest* request,
                                      CloseSessionResponse* response) override;
    
    virtual grpc::Status CreateStream(grpc::ServerContext* context,
                                      const CreateStreamRequest* request,
                                      CreateStreamResponse* response) override;
    
    virtual grpc::Status CloseStream(grpc::ServerContext* context,
                                     const CloseStreamRequest* request,
                                     CloseStreamResponse* response) override;
    
    virtual grpc::Status SendSessionMessage(grpc::ServerContext* context,
                                            const SendSessionMessageRequest* request,
                                            SendSessionMessageResponse* response) override;
    
    virtual grpc::Status RecvSessionMessage(grpc::ServerContext* context,
                                            const RecvSessionMessageRequest* request,
                                            RecvSessionMessageResponse* response) override;
  private:
    registry::ServerInfo AllocateServer() {
      OrbitMasterServerManager* manager = Singleton<OrbitMasterServerManager>::GetInstance();
      registry::ServerInfo server_info = manager->AllocateServer();
      return server_info;
    }

    void ReclaimServer(int session_id) {
      OrbitMasterServerManager* manager = Singleton<OrbitMasterServerManager>::GetInstance();
      manager->ReclaimServer(session_id);
    }

    registry::ServerInfo GetServer(int session_id) {
      OrbitMasterServerManager* manager = Singleton<OrbitMasterServerManager>::GetInstance();
      registry::ServerInfo server_info = manager->GetServer(session_id);
      return server_info;
    }
    
    // InitStub - initializes the gRPC stub if necessary.
    // we use a map (as a Cache) to store the gRPC stub, thus in most of the
    // cases, we do not need to intialize the gRPC stub but just reuse the
    // the existing stub from Cache.
    // But if the stubs is expired (says, the machine is down or leave),
    // the stub map(cache) should clear it or re-initialize it.
    std::shared_ptr<StreamService::Stub> InitStub(const string& server) {
      std::lock_guard<std::mutex> guard(mutex_);
      if (stubs_map_.find(server) != stubs_map_.end()) {
        std::shared_ptr<StreamService::Stub> stub = stubs_map_[server];
        if (stub != NULL) {
          return stub;
        }
      }

      std::shared_ptr<StreamService::Stub> stub;
      std::shared_ptr<Channel>
        channel(grpc::CreateChannel(server,
                                    grpc::InsecureCredentials()));
      stub = StreamService::NewStub(channel);
      stubs_map_[server] = stub;

      if (stub_create_.get() != NULL) {
        stub_create_->Increase(1);
      } else {
        LOG(WARNING) << "warnning : varz stub_create_ is null.";
      }
      
      return stub;
    }

    // Destroy the map(cache).
    void DestroyStub(const string& server) {
      std::lock_guard<std::mutex> guard(mutex_);
      stubs_map_[server] = NULL;
    }

    std::map<std::string, std::shared_ptr<StreamService::Stub> > stubs_map_;
    std::mutex mutex_;

    // Varz for the stub init times
    std::unique_ptr<ExportedVar> stub_create_;

  };

}  // namespace orbit
