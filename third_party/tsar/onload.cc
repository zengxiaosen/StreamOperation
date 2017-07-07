/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * onload.cc
 * ---------------------------------------------------------------------------
 * onload = (Orangelab Network load tool) is the homebrew network load tool.
 *  Works same as nload.
 * ---------------------------------------------------------------------------
 */

#include "gflags/gflags.h"
#include "glog/logging.h"

#include "tsar_wrapper.h"
#include "stream_service/orbit/base/timeutil.h"
#include "stream_service/orbit/base/strutil.h"
#include <vector>
#include <map>
#include <string>

using namespace std;

namespace orbit {
  string DisplayTraffic(long inbytes_per_second) {
    if (inbytes_per_second < 1024) {
      return StringPrintf("%d bytes/s", inbytes_per_second);
    } else if (inbytes_per_second < 1024 * 1024) {
      return StringPrintf("%3.2f KB/s", (float)inbytes_per_second / 1024);
    } else {
      return StringPrintf("%3.2f MB/s", (float)inbytes_per_second / (1024 * 1024));
    }
    return "";
  }

  struct NetworkStat {
    long last_bytein = 0;
    long last_byteout = 0;
  };
  
  void RunTest() {
    TsarWrapper tsar;
    bool running_ = true;
    map<string, NetworkStat> nic_with_stats;
    int last_time = 0;
    while(running_) {
      int now = getTimeMS();
      vector<stats_pernic> all_nics;
      tsar.CollectPerNicStat(&all_nics);
      for (auto nic : all_nics) {
        // Skip the network interface 'lo' = loopback
        string nic_name = nic.name;
        if (nic_name == "lo") {
          continue;
        }
        LOG(INFO) << "------------------------------------------------";
        LOG(INFO) << "nic_name=" << nic.name;
        LOG(INFO) << "bytein=" << nic.bytein;
        LOG(INFO) << "byteout=" << nic.byteout;
        LOG(INFO) << "pktin=" << nic.pktin;
        LOG(INFO) << "pktout=" << nic.pktout;
        NetworkStat nstat = nic_with_stats[nic_name];
        long last_bytein = nstat.last_bytein;
        long last_byteout = nstat.last_byteout;
        
        if (last_bytein != 0 && last_byteout != 0) {
          // Calculate the bits.
          int time_diff_ms = now - last_time;
          int inbytes_per_second = (nic.bytein - last_bytein) / time_diff_ms * 1000;
          int outbytes_per_second = (nic.byteout - last_byteout) / time_diff_ms * 1000;
          LOG(INFO) << "inboud traffic=" << DisplayTraffic(inbytes_per_second);
          LOG(INFO) << "outbound traffic=" << DisplayTraffic(outbytes_per_second);
          last_bytein = nic.bytein;
          last_byteout = nic.byteout;
        } else {
          last_bytein = nic.bytein;
          last_byteout = nic.byteout;
        }
        nstat.last_bytein = last_bytein;
        nstat.last_byteout = last_byteout;
        nic_with_stats[nic_name] = nstat;
      }
      last_time = now;
      sleep(1);
    }
  }
}  // namespace orbit

int main(int argc, char** argv) {
  google::InstallFailureSignalHandler();
  google::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);
  orbit::RunTest();
  return 0;
}
