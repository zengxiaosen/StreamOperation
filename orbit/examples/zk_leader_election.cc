/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * zk_leader_election.cc
 * ---------------------------------------------------------------------------
 * The leader election demo code using zookeeper.
 * ---------------------------------------------------------------------------
 */
#include "gflags/gflags.h"
#include "glog/logging.h"

#include "stream_service/third_party/zookeeper_client/zk.h"
#include "stream_service/orbit/base/strutil.h"

#include <vector>
#include <string>
#include <assert.h>
#include <algorithm>

DEFINE_string(zk_server, "127.0.0.1:2181", "The Zookeeper server's hostname and port");
DEFINE_string(election_zk_node, "/namespace/prod/orbit/master/election",
              "The ZK node of the master election.");

using namespace std;
namespace orbit {
  static const string PREFIX = "p_";
  class LeaderElection {
  public:
    LeaderElection() {
      zh_ = NULL;
      my_id_ = -1;
    }
    ~LeaderElection() {
    }

    static void Watcher(zhandle_t* zh, int type, int state, const char *path,void*v){
      LOG(INFO) << "watcher...event...state=" << state
                << "type=" << type;
      if (state == ZOO_CONNECTED_STATE) {
        //ctx->connected = true;
        LOG(INFO) << "ZOO_CONNECTED_STATE....";
      }
      if (type == ZOO_CHILD_EVENT) {
        string path = FLAGS_election_zk_node;
        LeaderElection* ctx = (LeaderElection*)zoo_get_context(zh);
        ctx->PrintNodes(zh, path);
      }
    }

    bool Enter() {
      if (zh_ == NULL) {
        if (!ConnectToServer()) {
          return false;
        }
      }
      // Check if it exists a election znode.
      string path = FLAGS_election_zk_node;
      int rc = zoo_exists(zh_, path.c_str(), 0, NULL);
      if (rc == ZNONODE) {
        // Create the znode for election
        char res_path[128];  
        rc = zoo_create(zh_, path.c_str(), NULL, -1,  
                        &ZOO_OPEN_ACL_UNSAFE, 0, res_path, 128);
        if (ZOK != rc) {
          LOG(ERROR) << "Create znode(" << path << ")---zerror=" << zerror(rc);
          return false;
        }
      }

      // Create the ephermal node under the election.
      string ext_path = FLAGS_election_zk_node + "/" + PREFIX;
      char res_path[128];
      rc = zoo_create(zh_, ext_path.c_str(), NULL, -1,  
                      &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL|ZOO_SEQUENCE , res_path, 128);
      if (ZOK != rc) {
        LOG(ERROR) << "Create znode(" << path << ")---zerror=" << zerror(rc);
        return false;
      }
      LOG(INFO) << "res_path=" << res_path;
      my_id_ = ParseNodePath(res_path);
      LOG(INFO) << "my_id=" << my_id_;

      zoo_set_context(zh_, this);
      zoo_set_watcher(zh_, Watcher);
      PrintNodes(zh_, path);
      return true;
    }
    void CloseServer() {
      if (zh_ != NULL) {
        zookeeper_close(zh_);  
        zh_ = NULL;
      }      
    }
  private:
    const int ZOOKEEPER_RECV_TIMEOUT = 10000;

    // Parse the file path /election/p_00000xxx into a integer xxx.
    int ParseNodePath(const string& path) {
      vector<string> file_parts;
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

    void PrintNodes(zhandle_t *zh, const string& path) {
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

    bool ConnectToServer() {
      if (zh_ != NULL) {
        zookeeper_close(zh_);  
        zh_ = NULL;
      }

      zh_ = zookeeper_init(FLAGS_zk_server.c_str(),
                        NULL,  // Watcher
                        ZOOKEEPER_RECV_TIMEOUT,
                        0,0,0);
      if (zh_ == NULL) {
        LOG(ERROR) << "Initialize ZK to host:" << FLAGS_zk_server
                   << " and it fails.";
        return false;
      }
      return true;
    }

    zhandle_t *zh_;
    int my_id_;
  };
}  // namespace orbit

int main(int argc, char** argv) {
  google::InstallFailureSignalHandler();
  google::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);
  orbit::LeaderElection leader;
  leader.Enter();
  bool exit = false;
  do {
    LOG(INFO) << "Press y to exit.";
    char c = getchar();
    if (c == 'y') {
      exit = true;
    }
  } while (!exit);
  leader.CloseServer();
  return 0;
}
