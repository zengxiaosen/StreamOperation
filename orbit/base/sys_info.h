/*
 * Copyright 2016 (C) Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * sys_info.h --- defines the utils functions for getting system information.
 */

#ifndef ORBIT_BASE_SYS_INFO_H__
#define ORBIT_BASE_SYS_INFO_H__

#include "sys/types.h"
#include "sys/sysinfo.h"

#include "time.h"

#include <queue>
#include <thread>
#include <mutex>

#include "stream_service/orbit/base/monitor/network_traffic_stats.pb.h"
#include "stream_service/orbit/base/monitor/system_info.pb.h"
#include "stream_service/orbit/base/singleton.h"
#include "stream_service/third_party/tsar/tsar_wrapper.h"

namespace orbit {
  class SystemInfo {
  public:
    void Init();

    void SetNetworkInterface(const std::string& interface) {
      network_interface_ = interface;
    }

    // Get the total Virtual memory of the system. (in bytes)
    long long GetTotalVM();

    long long GetTotalPhysicalMemory();
    
    long long GetTotalUsedVM();

    long long GetTotalUsedPhysicalMemory();
    
    int GetUsedVMByCurrentProcess();
  
    int GetUsedPhysicalMemoryByCurrentProcess();

    int GetCpuCount(); 
    int GetCurrentProcessThreadCount();
 
    double GetTotalCpuLoad();
    double GetCurrentProcessCpuLoad();

    // Call the third_party/tsar to get the data.
    // The average system load for the last 1, 5, 15 minutes
    stats_load GetAvgLoad();
    // The network statisticals for network interface.
    void GetNicState(const std::string& interface, NetworkTrafficStat* nic_stat);
    std::string GetReadableTrafficStat(const std::string& interface);
    health::HealthStatus GetHealthStatus();

    std::queue<double> GetRecentCpuLoads() {
      std::lock_guard<std::mutex> guard(cpu_queue_mutex_);
      return cpu_loads_;
    }
    std::queue<double> GetRecentProcessLoads() {
      std::lock_guard<std::mutex> guard(cpu_queue_mutex_);    
      return process_loads_;
    }

    std::string GetStartTime() {
      return start_time_;
    }
    clock_t GetElapsedTime();

  private:
    // Call the tsar module to collect the network stats.
    void CollectNetworkStats();
    void FillHealthStatus(double cpu_load, double process_load);
    void FillCpuAverageLoad();

    unsigned long long lastTotalUser, lastTotalUserLow, lastTotalSys, lastTotalIdle;

    std::string network_interface_;

    std::queue<double> cpu_loads_;
    std::queue<double> process_loads_;
    std::mutex cpu_queue_mutex_;

    std::string start_time_;
    time_t start_clock_;

    // The tsar and the related information.
    TsarWrapper tsar_;
    // Map from interface to the NetworkTrafficStat
    std::map<std::string, NetworkTrafficStat> interface_stats_;
    // the mutex to lock the interface_stats_ variable.
    std::mutex interface_stats_mutex_;

    // HealthStatus encapsulate the system information.
    health::HealthStatus health_status_;
    // Average load of the system.
    health::CpuAverageLoad avg_load_;
    std::mutex health_status_mutex_;

    DEFINE_AS_SINGLETON_WITHOUT_CONSTRUCTOR(SystemInfo);
  };
}  // namespace orbit

#endif  // ORBIT_BASE_SYS_INFO_H__


