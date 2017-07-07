/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng  Xu)
 *
 * orbit_master_server_manager.h
 * ---------------------------------------------------------------------------
 * Defines a server manager class to manage all the registered servers.
 * ---------------------------------------------------------------------------
 */
#pragma once

#include "registry_service_impl.h"
#include <unordered_map>
#include "glog/logging.h"

#include <map>

namespace orbit {
using std::map;
  /* 
   *  OrbitMasterServerManager uses to collaborate with the Registry to get
   *  all the live servers and the record the session and server relations
   *  mapping.
   *
   *  For each session, the OrbitMasterServerManager records the server to serve all the
   *  participants in the session (all streams), and all the events should be
   *  redirected to the corresponding server.
   */
  class OrbitMasterServerManager {
  public:
    void SetupRegistry(RegistryServiceImpl* registry) {
      registry_ = registry;
    }

    registry::ServerInfo RoundRobin();
    registry::ServerInfo LeastLoad();
    registry::ServerInfo AllocateServer();

    // Establish the session and server mapping.
    bool SetSessionServer(int session_id, registry::ServerInfo info) {
      std::lock_guard<std::mutex> guard(mutex_);
      session_to_server_[session_id] = info;
      return true;
    }

    vector<ServerStat> GetRawLiveServers() {
      return registry_->GetLiveServers();
    }

    vector<registry::ServerInfo> GetLiveServers() {
      vector<registry::ServerInfo> ret;
      vector<ServerStat> stats = GetRawLiveServers();
      for (auto stat : stats) {
        ret.push_back(stat.info);
      }
      return ret;
    }
    
    registry::ServerInfo GetServer(int session_id) {
      std::lock_guard<std::mutex> guard(mutex_);

      registry::ServerInfo info;
      if (session_to_server_.find(session_id) == session_to_server_.end()) {
        return info;
      }
      info = session_to_server_[session_id];

      // The session_to_server_ mapping from <session, server> does not take the
      // server's alive status. Thus we should GetLiveServers here and see if
      // the session's corresponding server is still alive. If not alive, we
      // should remove the mapping and in the future the session will not be
      //processed.
      vector<ServerStat> live_servers = registry_->GetLiveServers();
      for (auto server_stat : live_servers) {
        auto server = server_stat.info;
        if (server.name() == info.name() &&
            server.host() == info.host() &&
            server.port() == info.port()) {
          return info;
        }
      }

      // If the info does not match any live server, it is considered as DEAD.
      // Remove it from the session mapping.
      auto it = session_to_server_.find(session_id);
      LOG(INFO) << "erase..." << session_id;
      session_to_server_.erase(it);

      // Return the empty server info to the callee.
      registry::ServerInfo empty;
      return empty;
    }

    void ReclaimServer(int session_id) {
      std::lock_guard<std::mutex> guard(mutex_);
      auto it = session_to_server_.find(session_id);
      if (it != session_to_server_.end()) {
        session_to_server_.erase(it);
      }
    }

    map<int, registry::ServerInfo> GetSessionServerMap() {
      std::lock_guard<std::mutex> guard(mutex_);
      return session_to_server_;
    }
  private:
    RegistryServiceImpl* registry_;
    map<int, registry::ServerInfo> session_to_server_;
    int server_id_;
    std::mutex mutex_;

    DEFINE_AS_SINGLETON(OrbitMasterServerManager);
  };

}  // namespace orbit
