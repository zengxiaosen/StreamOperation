/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng  Xu)
 *
 * orbit_master_server_manager.h
 * ---------------------------------------------------------------------------
 * Implements the regisry::RegistryService. The service will keep track all 
 * the heart-beat message from backend servers.
 * ---------------------------------------------------------------------------
 */
#pragma once

#include "stream_service/proto/registry.grpc.pb.h"
// For gRpc
#include <grpc/grpc.h>
#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>

#include "stream_service/orbit/base/timeutil.h"
#include "server_stat.h"
#include "slavedata_collector.h"
#include "glog/logging.h"

#include <unordered_map>

namespace orbit {
using std::string;

  class RegistryServiceImpl : public registry::RegistryService::Service {
  public:
    RegistryServiceImpl() {
      id_ = 0;
    }

    virtual grpc::Status Register(grpc::ServerContext* context,
                                  const RegisterRequest* request,
                                  RegisterResponse* response);

    virtual grpc::Status GetServers(grpc::ServerContext* context,
                                    const GetServersRequest* request,
                                    GetServersResponse* response);

    vector<ServerStat> GetLiveServers();

  private:
    int GetNextId() {
      std::lock_guard<std::mutex> guard(mutex_);
      id_ = id_ + 1;
      return id_;
    }

    int id_;
    std::mutex mutex_;
    std::unordered_map<std::string, ServerStat> stats_;
  };


}  // namespace orbit
