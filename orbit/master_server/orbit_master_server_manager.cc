/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * orbit_master_server_manager.cc
 * ---------------------------------------------------------------------------
 * Implements the class to manage the registered servers
 * ---------------------------------------------------------------------------
 */

#include "orbit_master_server_manager.h"

#include "gflags/gflags.h"
#include "glog/logging.h"

DEFINE_string(scheduling_strategy, "ROUND_ROBIN", "Defines the scheduling strategy for "
              "allocating server. Values: ROUND_ROBIN, LEAST_LOAD .");

namespace orbit {

  registry::ServerInfo OrbitMasterServerManager::RoundRobin() {
    vector<ServerStat> live_servers = registry_->GetLiveServers();
    registry::ServerInfo info;
    
    if (live_servers.size() > 0) {
      info = live_servers[server_id_].info;
      server_id_++;
      // Round robin allocation algorithm.
      if (server_id_ >= live_servers.size()) {
        server_id_ = 0;
      }
    }
    return info;
  }

  registry::ServerInfo OrbitMasterServerManager::LeastLoad() {
    vector<ServerStat> live_servers = registry_->GetLiveServers();
    registry::ServerInfo info;
    double min_usage = 10000;
    for (auto server_candidate : live_servers) {
      double usage = server_candidate.status.system().cpu_usage_percent();
      if (usage < min_usage) {
        min_usage = usage;
        info = server_candidate.info;
      }
    }
    return info;
  }

  // Use the algorithm to allocate an server in the server pools.
  // The server will 
  registry::ServerInfo OrbitMasterServerManager::AllocateServer() {
    std::lock_guard<std::mutex> guard(mutex_);
    
    if (FLAGS_scheduling_strategy == "ROUND_ROBIN") {
      return RoundRobin();
    } else if (FLAGS_scheduling_strategy == "LEAST_LOAD") {
      return LeastLoad();
    } else {
      LOG(FATAL) << "Use a scheduling strategy(" << FLAGS_scheduling_strategy
                 << ") is not supported yet.";
    }
  }
}  // namespace orbit
