/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * zk_example.cc
 * ---------------------------------------------------------------------------
 * A test example program for zookeeper client.
 * ---------------------------------------------------------------------------
 */
#include "gflags/gflags.h"
#include "glog/logging.h"

#include "stream_service/third_party/zookeeper_client/zk.h"

#include <vector>
#include <string>
#include <assert.h>

DEFINE_string(zk_server, "127.0.0.1:2181", "The Zookeeper server's hostname and port");

using namespace std;
namespace orbit {
  const int EXPECTED_RECV_TIMEOUT=10000;
  static void watcher(zhandle_t *, int type, int state, const char *path,void*v){
    //watchctx_t *ctx = (watchctx_t*)v;
    
    LOG(INFO) << "watcher...event...state=" << state
              << "type=" << type;
    if (state == ZOO_CONNECTED_STATE) {
      //ctx->connected = true;
      LOG(INFO) << "ZOO_CONNECTED_STATE....";
    }
  }

  void RunCreateTest() {
    zhandle_t *zh;
    zh=zookeeper_init(FLAGS_zk_server.c_str(),
                      watcher,  // Watcher
                      EXPECTED_RECV_TIMEOUT,
                      0,0,0);
    if (zh == NULL) {
      LOG(ERROR) << "Initialize ZK to host:" << FLAGS_zk_server
                 << " and it fails.";
      return;
    }

    string server_path = "/zk_test/dt";
    int rc = zoo_exists(zh, server_path.c_str(), 0, NULL);
    if (rc == ZNONODE) {
      string value = "192.168.1.13";
      char res_path[128];  
      rc = zoo_create(zh, server_path.c_str(), value.c_str(), value.size(),  
                      &ZOO_OPEN_ACL_UNSAFE, 0, res_path, 128);  
      if (ZOK != rc) {
        LOG(ERROR) << "zerror=" << zerror(rc);
        return;
      } else {
        LOG(INFO) << "Create successfully.";
      }
    } else {
      LOG(INFO) << "Exists a path. path=" << server_path;
    }

    zookeeper_close(zh);  
  }

  void RunUpdateTest() {
    zhandle_t *zh;
    zh=zookeeper_init(FLAGS_zk_server.c_str(),
                      watcher,  // Watcher
                      EXPECTED_RECV_TIMEOUT,
                      0,0,0);
    if (zh == NULL) {
      LOG(ERROR) << "Initialize ZK to host:" << FLAGS_zk_server
                 << " and it fails.";
      return;
    }

    string server_path = "/zk_test/dt";
    int rc = zoo_exists(zh, server_path.c_str(), 0, NULL);
    if (rc != ZNONODE) {
      zoo_set_watcher(zh, watcher);
      char res_path[128];
      int len;
      rc = zoo_get(zh, server_path.c_str(), 1, res_path, &len, NULL);
      if (ZOK != rc) {
        LOG(ERROR) << "zoo_aget zerror=" << zerror(rc);
        return;
      } else {
        LOG(INFO) << "res_path=" << res_path;
      }
      string value = "192.168.30.111";
      rc = zoo_set(zh, server_path.c_str(), value.c_str(), value.size(), -1 // version
                   );
      if (ZOK != rc) {
        LOG(ERROR) << "zerror=" << zerror(rc);
        return;
      } else {
        LOG(INFO) << "Update it to " << value;
      }
    } else {
      LOG(INFO) << "Not found the path:" << server_path;
    }
    sleep(30);
    zookeeper_close(zh);  
  }
}  // namespace orbit

int main(int argc, char** argv) {
  google::InstallFailureSignalHandler();
  google::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);
  //orbit::RunCreateTest();
  orbit::RunUpdateTest();
  return 0;
}
