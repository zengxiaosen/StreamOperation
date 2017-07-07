/*
 * Copyright 2016 All Rights Reserved.
 * Author: zhanghao@orangelab.cn
 *
 * orbit_zk_client.h
 * ---------------------------------------------------------------------------
 * The zookeeper client regist itself to zookeeper.
 * ---------------------------------------------------------------------------
 */

#include "gflags/gflags.h"
#include "glog/logging.h"

#include "stream_service/third_party/zookeeper_client/zk.h"
#include "stream_service/orbit/base/strutil.h"
#include "stream_service/proto/registry.grpc.pb.h"
#include "stream_service/orbit/master_server/server_stat.h"
#include "stream_service/orbit/base/singleton.h"

#include <vector>
#include <string>

namespace orbit {

namespace zookeeper {

class ZookeeperRegister {
 public:
  // Init
  void Init();

  // The callback fuction when the zookeeper node changed.
  static void Watcher(zhandle_t* zh, int type, int state, const char *path, void *v);
  // Create zookeeper node to keep the server info.
  bool CreateNode(std::string node);
  bool CreateSingleNode(std::string node);
  
  bool Enter();
  void CloseServer();

  // Write data to zookeeper
  bool WriteData(std::string data);

  // Write the server data to zookeeper every 10s
  void RegistDataLoop();
  bool Query(std::vector<orbit::ServerStat>* server_info);
  
 private:
  bool ConnectToServer();
  int ParseNodePath(const string& path);
  void PrintNodes(zhandle_t *zh, const string& path);
  void SetPath(std::string path);
  
 private:
  const int ZOOKEEPER_RECV_TIMEOUT = 10000;
  zhandle_t* zh_;
  int my_id_;
  
  static std::string path_;
  DEFINE_AS_SINGLETON(ZookeeperRegister);
};
  
} // zookeeper
 
} // orbit
