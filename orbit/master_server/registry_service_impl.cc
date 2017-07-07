/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng  Xu)
 *
 * registry_service_impl.cc
 * ---------------------------------------------------------------------------
 * Implements the regisry::RegistryService.
 * ---------------------------------------------------------------------------
 */
#pragma once

#include "registry_service_impl.h"

namespace orbit {
  grpc::Status RegistryServiceImpl::Register(grpc::ServerContext* context,
                                             const RegisterRequest* request,
                                             RegisterResponse* response) {
    VLOG(2) << "request=" << request->DebugString();
    //LOG(INFO) << "request=" << request->DebugString();

    string qname = StringPrintf("%s:%d/%s",
                                request->server().host().c_str(),
                                request->server().port(),
                                request->server().name().c_str());
    // Update the register information.
    {
      std::lock_guard<std::mutex> guard(mutex_);
      
      ServerStat stat = stats_[qname];
      stat.info = request->server();
      stat.last_ts = getTimeMS();

      if (request->has_status() && request->status().ts() != 0) {
        stat.status = request->status();
      }
      stats_[qname] = stat;
      
      string dbKey = StringPrintf("%s:%d_%d",request->server().host().c_str(),
                                  request->server().port(),
                                  getTimeMS()/1000);
      
      if (request->has_status() && request->status().ts() != 0) {
        string buffer;
        stat.status.SerializeToString(&buffer);
        
        SlaveDataCollector* collector = Singleton<SlaveDataCollector>::GetInstance();
        
        if(!collector->Add(dbKey, stat.status)) {
          LOG(ERROR) << "save slave data error at " <<dbKey;
        }
        //collector->Find("192.168.1.126:10001_1465033376","192.168.1.126:10001_1465033400");
      }
    }
    
    // Set the response of the Register method.
    response->set_status(RegisterResponse_Status_OK);
    response->set_id(GetNextId());
    response->set_message("ok");
    return grpc::Status::OK;
  }

  // Helper function to get all the live servers in the system.
  vector<ServerStat> RegistryServiceImpl::GetLiveServers() {
    std::lock_guard<std::mutex> guard(mutex_);
    vector<ServerStat> res;
    for (auto& stat : stats_) {
      ServerStat x = stat.second;
      long long now_ts = getTimeMS();
      // If there is no heartbeat for 10 second... the server is considered as DEAD.
      if (now_ts - x.last_ts < 10000) {
        res.push_back(x);
      }
    }
    return res;
  }

  grpc::Status RegistryServiceImpl::GetServers(grpc::ServerContext* context,
                                               const GetServersRequest* request,
                                               GetServersResponse* response) {
    vector<ServerStat> live_servers = GetLiveServers();
    for (auto x : live_servers) {
      ServerInfo* server_info = response->add_servers();
      server_info->CopyFrom(x.info);
    }

    response->set_status(GetServersResponse::OK);
    return grpc::Status::OK;
  }
}  // namespace orbit
