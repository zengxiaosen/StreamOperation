/*
 * Copyright 2016 All Rights Reserved.
 * Author: zhanghao@orangelab.cn
 *
 * orbit_zk_client.cc
 * ---------------------------------------------------------------------------
 * The zookeeper client regist itself to zookeeper.
 * ---------------------------------------------------------------------------
 */

#include "orbit_zk_client.h"
#include "stream_service/orbit/base/sys_info.h"
#include "stream_service/orbit/production/machine_db.h"
#include "stream_service/orbit/stun_server_prober.h"
#include "stream_service/orbit/base/timeutil.h"

#include <algorithm>

DEFINE_string(zk_server, "127.0.0.1:2181", "The Zookeeper server's hostname and port");
DEFINE_string(server_zk_node, "/namespace/prod/orbit/slave",
              "The ZK node of the server node.");
DECLARE_string(set_local_address);
DECLARE_string(turn_server_ip);
DECLARE_int32(turn_server_port);
DECLARE_string(stun_server_ip);
DECLARE_int32(stun_server_port);
DECLARE_int32(port);
DECLARE_int32(http_port);

namespace orbit {

namespace zookeeper {

  static const std::string PREFIX = "server_";
  std::string ZookeeperRegister::path_ = "";

  void ZookeeperRegister::Init() {
    zh_ = NULL;
    my_id_ = -1;
  }

  // Create node in zookeeper
  bool ZookeeperRegister::CreateNode(std::string node) {
    if (zh_ == NULL) {
      LOG(ERROR) << "zh_ is null.";
      return false;
    }

    std::vector<std::string> result;
    SplitStringUsing(node, "/", &result);

    std::string node_str;
    for(auto i = 0; i < result.size(); i ++) {
      node_str = node_str + "/" + result[i];
      LOG(INFO) << "node_str = " << node_str;
      if (CreateSingleNode(node_str) == false) {
        LOG(INFO) << "CreateSingleNode error.";
        return false;
      }
    }

    return true;
  }

  bool ZookeeperRegister::CreateSingleNode(std::string node) {
    if (zh_ == NULL) {
      LOG(ERROR) << "zh_ is null.";
      return false;
    }

    int rc = zoo_exists(zh_, node.c_str(), 0, NULL);
    if (rc == ZNONODE) {
      char res_path[128];
      rc = zoo_create(zh_, node.c_str(), NULL, -1,
                      &ZOO_OPEN_ACL_UNSAFE, 0, res_path, 128);
      if (rc != ZOK) {
        LOG(ERROR) << "Create znode(" << node << ") --- zerror = " << zerror(rc);
        return false;
      } else {
        return true;
      }
    } else {
      LOG(INFO) << "znode(" << node << ") is exist";
    }

    return true;
  }

  void ZookeeperRegister::Watcher(zhandle_t* zh, int type, int state, const char *path, void *v) {
    LOG(INFO) << "watcher ... event ... state = " << state
              << " type = " << type;

    if (state == ZOO_CONNECTED_STATE) {
      LOG(INFO) << "ZOO_CONNECTED_STATE......";
    }

    if (type == ZOO_CHILD_EVENT) {
      LOG(INFO) << "ZOO_CHILD_EVENT######";
      string path = FLAGS_server_zk_node;
      ZookeeperRegister* ctx = (ZookeeperRegister*)zoo_get_context(zh);
      //ctx->PrintNodes(zh, path);
    }
  }

  bool ZookeeperRegister::Enter() {
    if (zh_ == NULL) {
      if (!ConnectToServer()) {
        LOG(ERROR) << "zh_ is null. Reconnect to zookeeper failed.";
        return false;
      }
    }
    
    // Check if it exists a server znode.
    string path = FLAGS_server_zk_node;
    int rc = zoo_exists(zh_, path.c_str(), 0, NULL);
    if (rc == ZNONODE) {
      if (CreateNode(path) == false) {
        return false;
      }
    }

    // Create the ephermal node under the zk_node.
    string ext_path = FLAGS_server_zk_node + "/" + PREFIX;
    char res_path[128];
    rc = zoo_create(zh_, ext_path.c_str(), NULL, -1,  
                    &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL|ZOO_SEQUENCE , res_path, 128);
    if (ZOK != rc) {
      LOG(ERROR) << "Create znode(" << path << ")---zerror=" << zerror(rc);
      return false;
    }

    // Set the server path in zookeeper
    SetPath(res_path);
    
    my_id_ = ParseNodePath(res_path);

    //zoo_set_context(zh_, this);
    //zoo_set_watcher(zh_, Watcher);
    //PrintNodes(zh_, path);
      
    return true;
  }

  bool ZookeeperRegister::WriteData(std::string data) {
    if (zh_ == NULL) {
      if (!ConnectToServer()) {
        LOG(ERROR) << "zh_ is null. Reconnect to zookeeper failed.";
        return false;
      }
    }

    int rc = zoo_exists(zh_, path_.c_str(), 0, NULL);
    if (rc == ZNONODE) {
      LOG(ERROR) << "Zookeeper has no path : " << path_;
      return false;
    }

    char res_path[128];
    rc = zoo_set(zh_, path_.c_str(), data.c_str(), data.size(), -1);
    if (ZOK != rc) {
      LOG(ERROR) << "Write znode(" << path_ << ")---zerror=" << zerror(rc);
      return false;
    }
    
    return true;
  }

  void ZookeeperRegister::CloseServer() {
    if (zh_ == NULL) {
      zookeeper_close(zh_);
      zh_ = NULL;
    }
  }

  void ZookeeperRegister::RegistDataLoop() {
    while (true) {
      // prepare the data
      orbit::registry::RegisterRequest register_info;
      register_info.mutable_server()->set_name("OrbitStreamService");
      if (!FLAGS_set_local_address.empty()) {
        register_info.mutable_server()->set_host(FLAGS_set_local_address);
      } else {
        orbit::StunServerProber* prober = Singleton<orbit::StunServerProber>::get();
        std::string public_ip = prober->GetPublicIP(FLAGS_stun_server_ip, FLAGS_stun_server_port);
        orbit::MachineDb* machine_db = Singleton<orbit::MachineDb>::get();
        std::vector<std::string> public_hosts = machine_db->ReverseAddressToMachine(public_ip); 
        if (public_hosts.size() >= 1) {
          register_info.mutable_server()->set_host(public_hosts[0]);
        } else {
          
          register_info.mutable_server()->set_host(public_ip);
        }
      }
      register_info.mutable_server()->set_port(FLAGS_port);
      register_info.mutable_server()->set_http_port(FLAGS_http_port);

      orbit::health::HealthStatus health_status = 
        Singleton<orbit::SystemInfo>::GetInstance()->GetHealthStatus();
      register_info.mutable_status()->CopyFrom(health_status);

      std::string buffer;
      register_info.SerializeToString(&buffer);
      if (buffer.size() == 0) {
        LOG(INFO) << "buffer size is 0";
      }
      
      WriteData(buffer);
      
      sleep(10);
    }
  }

  bool ZookeeperRegister::ConnectToServer() {
    if (zh_ != NULL) {
      zookeeper_close(zh_);
      zh_ = NULL;
    }

    zh_ = zookeeper_init(FLAGS_zk_server.c_str(),
                         NULL,
                         ZOOKEEPER_RECV_TIMEOUT,
                         0, 0, 0);
    if (zh_ == NULL) {
      LOG(ERROR) << "Initialize ZK to host:" << FLAGS_zk_server
                 << " and it fails.";
      return false;
    } else {
      LOG(INFO) << "INitialize ZK is ok.";
    }

    return true;
  }

  int ZookeeperRegister::ParseNodePath(const string& path) {
    std::vector<std::string> file_parts;
    SplitStringUsing(path, "/", &file_parts);
    string name = path;
    if (file_parts.size() > 1) {
      name = file_parts[file_parts.size() - 1];
    }

    if (HasPrefixString(name, PREFIX)) {
      string digit_part = StripPrefixString(name, PREFIX);
      return std::stoi(digit_part);
    }

    return -1;
  }

  void ZookeeperRegister::PrintNodes(zhandle_t *zh, const string& path) {
    String_vector nodes;
    int rc = zoo_get_children(zh, path.c_str(), 1, &nodes);
    if (ZOK != rc) {
      LOG(ERROR) << "Get znode(" << path << ") child---zerror=" << zerror(rc);
      return;
    }
    vector<int> node_seqs;
    for (int i = 0; i < nodes.count; ++i) {  
      LOG(INFO) << "node path=" << nodes.data[i];
      int node_seq = ParseNodePath(nodes.data[i]);
      LOG(INFO) << "seq_num=" << node_seq;
      node_seqs.push_back(node_seq);
    }
    
    auto minseq_iter = std::min_element(node_seqs.begin(), node_seqs.end());
    int minseq = *minseq_iter;
    LOG(INFO) << "minseq=" << minseq;
    if (minseq == my_id_) {
      LOG(INFO) << "I am the leader.";
    } else {
      LOG(INFO) << "I am the follower, the leader is:" << minseq;
    }
  }

  void ZookeeperRegister::SetPath(std::string path) {
    path_ = path;
  }

  // Get all server info from zookeeper
  bool ZookeeperRegister::Query(std::vector<orbit::ServerStat>* server_info) {
    long long cur_time = GetCurrentTime_MS();
    if (zh_ == NULL) {
      if (!ConnectToServer()) {
        LOG(ERROR) << "zh_ is null. Reconnect to zookeeper failed.";
        return false;
      }
    }

    String_vector nodes;
    int rc = zoo_get_children(zh_, FLAGS_server_zk_node.c_str(), 1, &nodes);
    if (ZOK != rc) {
      LOG(ERROR) << "Get znode(" << FLAGS_server_zk_node << ") child---zerror=" << zerror(rc);
      return false;
    }

    // Get all nodes and it's data
    vector<int> node_child;
    for (int i = 0; i < nodes.count; ++i) {  
      std::string node_str = FLAGS_server_zk_node + "/" + nodes.data[i];
      
      char buffer_tmp[1024];
      int len = sizeof(buffer_tmp);
      struct Stat stat;
      rc = zoo_get(zh_, node_str.c_str(), 0, buffer_tmp, &len, &stat);
      if (ZOK != rc) {
        LOG(ERROR) << "Get znode(" << node_str << ") child---zerror=" << zerror(rc);
        return false;
      } else {
        if (stat.dataLength >1024) {
          LOG(WARNING) << "Have more data to read in zookeeper.";
        }
      }

      orbit::registry::RegisterRequest server_data;
      std::string buffer(buffer_tmp);
      server_data.ParseFromString(buffer);

      LOG(INFO) << "Get server info from zookeeper : " << server_data.DebugString();

      orbit::ServerStat server_stat_tmp;
      server_stat_tmp.info = server_data.server();
      server_stat_tmp.status = server_data.status();
      server_info->push_back(server_stat_tmp);
    }

    return true;
  }
  
} // zookeeper
 
} // orbit
