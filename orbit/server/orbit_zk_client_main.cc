/*
 * Copyright 2016 All Rights Reserved.
 * Author: zhanghao@orangelab.cn
 *
 * orbit_zk_client_main.cc
 * ---------------------------------------------------------------------------
 * The zookeeper client regist itself to zookeeper.
 * ---------------------------------------------------------------------------
 */

#include "orbit_zk_client.h"

int main(int argc, char** argv) {
  google::InstallFailureSignalHandler();
  google::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);
  orbit::zookeeper::ZookeeperRegister regist;
  regist.Enter();
  bool exit = false;
  do {
    LOG(INFO) << "Press y to exit.";
    char c = getchar();
    if (c == 'y') {
      exit = true;
    }
  } while (!exit);
  regist.CloseServer();
  return 0;
}
