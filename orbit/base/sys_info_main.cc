/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * sys_info_main.cc
 * ---------------------------------------------------------------------------
 * Implements a test program to test the sys_info.h/cc module.
 * ---------------------------------------------------------------------------
 */

// For Gflags and Glog
#include "gflags/gflags.h"
#include "glog/logging.h"

#include "sys_info.h"
#include "stream_service/orbit/base/singleton.h"

int main(int argc, char** argv) {
  google::InstallFailureSignalHandler();
  google::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);

  orbit::SystemInfo* sys_info = Singleton<orbit::SystemInfo>::GetInstance();

  // Get the total Virtual memory of the system.
  LOG(INFO) << "Total VM=" << sys_info->GetTotalVM();
  LOG(INFO) << "Total UsedVM=" << sys_info->GetTotalUsedVM();
  LOG(INFO) << "Total CurrentProcessUsedVM=" << sys_info->GetUsedVMByCurrentProcess() << " KB";

  LOG(INFO) << "Total Physical M=" << sys_info->GetTotalPhysicalMemory();
  LOG(INFO) << "Total Used Physical M=" << sys_info->GetTotalUsedPhysicalMemory();
  LOG(INFO) << "Total CurrentProcessUsedVM=" << sys_info->GetUsedPhysicalMemoryByCurrentProcess() << " KB";

  LOG(INFO) << "Total CPU usage:" << sys_info->GetTotalCpuLoad();
  LOG(INFO) << "Current process's CPU usage:" << sys_info->GetCurrentProcessCpuLoad();

  for (int i = 0; i < 20; ++i) {
    LOG(INFO) << "Total CPU usage:" << sys_info->GetTotalCpuLoad();
    LOG(INFO) << "Current process's CPU usage:" << sys_info->GetCurrentProcessCpuLoad();
    sleep(3);
  }
  return 0;
}
