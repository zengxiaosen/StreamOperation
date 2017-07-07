/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * tsar_main.cc
 * ---------------------------------------------------------------------------
 * The main program to run the tsar_wrapper library
 * ---------------------------------------------------------------------------
 */

#include "gflags/gflags.h"
#include "glog/logging.h"

#include "tsar_wrapper.h"
#include <vector>
#include <string>

using namespace std;

namespace orbit {
  void RunTest() {
    TsarWrapper tsar;
    stats_cpu cpu;
    tsar.CollectCpuStat(&cpu);
    LOG(INFO) << "------------------------------------------------";
    LOG(INFO) << "cpu_number=" << cpu.cpu_number;
    LOG(INFO) << "cpu_user=" << cpu.cpu_user;
    LOG(INFO) << "cpu_nice=" << cpu.cpu_nice;
    LOG(INFO) << "cpu_sys=" << cpu.cpu_sys;
    LOG(INFO) << "cpu_user=" << cpu.cpu_user;
    LOG(INFO) << "cpu_idle=" << cpu.cpu_idle;
    LOG(INFO) << "cpu_iowait=" << cpu.cpu_iowait;
    LOG(INFO) << "cpu_steal=" << cpu.cpu_steal;
    LOG(INFO) << "cpu_hardirq=" << cpu.cpu_hardirq;
    LOG(INFO) << "cpu_softirq=" << cpu.cpu_softirq;
    LOG(INFO) << "cpu_guest=" << cpu.cpu_guest;
    
    LOG(INFO) << "------------------------------------------------";
    stats_mem st_m;
    tsar.CollectMemStat(&st_m);
    LOG(INFO) << "total memory(KB)=" << st_m.tlmkb;
    LOG(INFO) << "free memory(KB)=" << st_m.frmkb;

    stats_load st_load;
    tsar.CollectLoadStat(&st_load);
    LOG(INFO) << "nr_running=" << st_load.nr_running;
    LOG(INFO) << "load_avg_1=" << st_load.load_avg_1;
    LOG(INFO) << "load_avg_5=" << st_load.load_avg_5;
    LOG(INFO) << "load_avg_15=" << st_load.load_avg_15;
    LOG(INFO) << "nr_threads=" << st_load.nr_threads;

    stats_proc st_proc;
    tsar.CollectProcStat("tsar_main", &st_proc);
    LOG(INFO) << "user_cpu=" << st_proc.user_cpu;
    LOG(INFO) << "sys_cpu=" << st_proc.sys_cpu;
    LOG(INFO) << "total_cpu=" << st_proc.total_cpu;
    LOG(INFO) << "total_mem=" << st_proc.total_mem;
    LOG(INFO) << "mem=" << st_proc.mem;
    LOG(INFO) << "read_bytes=" << st_proc.read_bytes;
    LOG(INFO) << "write_bytes=" << st_proc.write_bytes;

    vector<stats_percpu> all_cpus;
    tsar.CollectPerCpuStat(&all_cpus);
    for (auto cpu : all_cpus) {
      LOG(INFO) << "------------------------------------------------";
      LOG(INFO) << "cpu_name=" << cpu.cpu_name;
      LOG(INFO) << "cpu_user=" << cpu.cpu_user;
      LOG(INFO) << "cpu_nice=" << cpu.cpu_nice;
      LOG(INFO) << "cpu_sys=" << cpu.cpu_sys;
      LOG(INFO) << "cpu_user=" << cpu.cpu_user;
      LOG(INFO) << "cpu_idle=" << cpu.cpu_idle;
      LOG(INFO) << "cpu_iowait=" << cpu.cpu_iowait;
      LOG(INFO) << "cpu_steal=" << cpu.cpu_steal;
      LOG(INFO) << "cpu_hardirq=" << cpu.cpu_hardirq;
      LOG(INFO) << "cpu_softirq=" << cpu.cpu_softirq;
      LOG(INFO) << "cpu_guest=" << cpu.cpu_guest;
    }

    vector<stats_pernic> all_nics;
    tsar.CollectPerNicStat(&all_nics);
    for (auto nic : all_nics) {
      LOG(INFO) << "------------------------------------------------";
      LOG(INFO) << "nic_name=" << nic.name;
      LOG(INFO) << "bytein=" << nic.bytein;
      LOG(INFO) << "byteout=" << nic.byteout;
      LOG(INFO) << "pktin=" << nic.pktin;
      LOG(INFO) << "pktout=" << nic.pktout;
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
