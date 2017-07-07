/*
 * Copyright 2016 All Rights Reserved.
 * Author: zhihan@orangelab.cn
 *
 * slavedata_collector_main.cc
 */

#include "stream_service/orbit/base/singleton.h"
#include "slavedata_collector.h"
#include "stream_service/orbit/base/timeutil.h"

#include <thread>
#include <vector>
#include "gflags/gflags.h"
#include "glog/logging.h"

int main(int argc, char** argv) {
  google::InstallFailureSignalHandler();
  google::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);

  orbit::SlaveDataCollector* collector = Singleton<orbit::SlaveDataCollector>::GetInstance();
  std::string data_prefix = orbit::StringPrintf("%s:%d","192.168.1.123",1234);
  long current_second = orbit::getTimeMS()/1000;
  std::string limit = orbit::StringPrintf("%s_%d",data_prefix.c_str(),current_second);
  std::string start = orbit::StringPrintf("%s_%d",data_prefix.c_str(),current_second-30);
  auto datas = collector->Find(start,limit);


  return 0;
}