/*
 * Copyright 2016 (C) Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * sys_info.h --- defines the utils functions for getting system information.
 */
#include "sys_info.h"

#include "sys/types.h"
#include "sys/sysinfo.h"
#include "sys/times.h"
#include "sys/vtimes.h"

#include "stdlib.h"
#include "stdio.h"
#include "string.h"

#include <math.h> 
#include <sys/prctl.h>

#include "glog/logging.h"
#include "stream_service/orbit/base/strutil.h"
#include "stream_service/orbit/base/timeutil.h"

using namespace orbit::health;

namespace orbit {
  namespace {
    static unsigned long long lastTotalUser, lastTotalUserLow, lastTotalSys, lastTotalIdle;
    static clock_t lastCPU, lastSysCPU, lastUserCPU;
    static int numProcessors;

    static bool initialized_proc_stat = false;
    static bool initialized_cpu_info = false;

    void initProcStat() {
      FILE* file = fopen("/proc/stat", "r");
      fscanf(file, "cpu %llu %llu %llu %llu", &lastTotalUser, &lastTotalUserLow,
             &lastTotalSys, &lastTotalIdle);
      fclose(file);
    }
    
    void initCpuInfo(){
      FILE* file;
      struct tms timeSample;
      char line[128];

      lastCPU = times(&timeSample);
      lastSysCPU = timeSample.tms_stime;
      lastUserCPU = timeSample.tms_utime;

      file = fopen("/proc/cpuinfo", "r");
      numProcessors = 0;
      while(fgets(line, 128, file) != NULL){
        if (strncmp(line, "processor", 9) == 0) numProcessors++;
      }
      fclose(file);
    }
    

    double getProcStatCurrentValue() {
      if (!initialized_proc_stat) {
        initProcStat();
        initialized_proc_stat = true;
        return -1;
      }
      double percent;
      FILE* file;
      unsigned long long totalUser, totalUserLow, totalSys, totalIdle, total;
  
      file = fopen("/proc/stat", "r");
      fscanf(file, "cpu %llu %llu %llu %llu", &totalUser, &totalUserLow,
             &totalSys, &totalIdle);
      fclose(file);
  
      if (totalUser < lastTotalUser || totalUserLow < lastTotalUserLow ||
          totalSys < lastTotalSys || totalIdle < lastTotalIdle){
        //Overflow detection. Just skip this value.
        percent = -1.0;
      } else{
        total = (totalUser - lastTotalUser) + (totalUserLow - lastTotalUserLow) +
          (totalSys - lastTotalSys);
        percent = total;
        total += (totalIdle - lastTotalIdle);
        percent /= total;
        percent *= 100;
      }
    

      lastTotalUser = totalUser;
      lastTotalUserLow = totalUserLow;
      lastTotalSys = totalSys;
      lastTotalIdle = totalIdle;
      return percent;
    }

   double getCpuInfoCurrentValue(){
      if (!initialized_cpu_info) {
        initCpuInfo();
        initialized_cpu_info = true;
        return -1;
      }
     struct tms timeSample;
     clock_t now;
     double percent;

     now = times(&timeSample);
     if (now <= lastCPU || timeSample.tms_stime < lastSysCPU ||
         timeSample.tms_utime < lastUserCPU){
       //Overflow detection. Just skip this value.
       percent = -1.0;
     } else{
       percent = (timeSample.tms_stime - lastSysCPU) +
         (timeSample.tms_utime - lastUserCPU);
       percent /= (now - lastCPU);
       percent /= numProcessors;
       percent *= 100;
     }
     lastCPU = now;
     lastSysCPU = timeSample.tms_stime;
     lastUserCPU = timeSample.tms_utime;
     return percent;
   }

    // Parse the /proc/self...
    int parseLine(char* line){
        int i = strlen(line);
        while (*line < '0' || *line > '9') line++;
        line[i-3] = '\0';
        i = atoi(line);
        return i;
    }
    

    int getValueVirtual(){ //Note: this value is in KB!
        FILE* file = fopen("/proc/self/status", "r");
        int result = -1;
        char line[128];
    

        while (fgets(line, 128, file) != NULL){
            if (strncmp(line, "VmSize:", 7) == 0){
                result = parseLine(line);
                break;
            }
        }
        fclose(file);
        return result;
    }

    int getValuePhysical(){ //Note: this value is in KB!
      FILE* file = fopen("/proc/self/status", "r");
      int result = -1;
      char line[128];

      while (fgets(line, 128, file) != NULL){
        if (strncmp(line, "VmRSS:", 6) == 0){
          result = parseLine(line);
          break;
        }
      }
      fclose(file);
      return result;
    }

    int getValueThreads(){
      FILE* file = fopen("/proc/self/status", "r");
      int result = -1;
      char line[128];

      while (fgets(line, 128, file) != NULL){
        if (strncmp(line, "Threads:", 8) == 0){
          result = parseLine(line);
          break;
        }
      }
      fclose(file);
      return result;
    }
  }  // annoymouse namespace

// Constructor
SystemInfo::SystemInfo() {
  Init();
}

// Destructor
SystemInfo::~SystemInfo() {
}

long long SystemInfo::GetTotalVM() {
  struct sysinfo memInfo;
  sysinfo (&memInfo);

  long long totalVirtualMem = memInfo.totalram;
  //Add other values in next statement to avoid int overflow on right hand side...
  totalVirtualMem += memInfo.totalswap;
  totalVirtualMem *= memInfo.mem_unit;

  return totalVirtualMem;
}

long long SystemInfo::GetTotalUsedVM() {
  struct sysinfo memInfo;
  sysinfo (&memInfo);

  long long virtualMemUsed = memInfo.totalram - memInfo.freeram;
  //Add other values in next statement to avoid int overflow on right hand side...
  virtualMemUsed += memInfo.totalswap - memInfo.freeswap;
  virtualMemUsed *= memInfo.mem_unit;

  return virtualMemUsed;
}

int SystemInfo::GetUsedVMByCurrentProcess() {
  int result = getValueVirtual();
  return result;
}

long long SystemInfo::GetTotalPhysicalMemory() {
  struct sysinfo memInfo;
  sysinfo (&memInfo);

  long long totalPhysMem = memInfo.totalram;
  //Multiply in next statement to avoid int overflow on right hand side...
  totalPhysMem *= memInfo.mem_unit;

  return totalPhysMem;
}


long long SystemInfo::GetTotalUsedPhysicalMemory() {
  struct sysinfo memInfo;
  sysinfo (&memInfo);

  long long physMemUsed = memInfo.totalram - memInfo.freeram;
  //Multiply in next statement to avoid int overflow on right hand side...
  physMemUsed *= memInfo.mem_unit;

  return physMemUsed;
}

int SystemInfo::GetUsedPhysicalMemoryByCurrentProcess() {
  int result = getValuePhysical();
  return result;
}

double SystemInfo::GetTotalCpuLoad() {
  return getProcStatCurrentValue();
}
 
double SystemInfo::GetCurrentProcessCpuLoad() {
  return getCpuInfoCurrentValue();
}

int SystemInfo::GetCurrentProcessThreadCount() {
  int thread_count = getValueThreads();
  return thread_count;
}

int SystemInfo::GetCpuCount() {
  int nthreads = std::thread::hardware_concurrency();
  return nthreads;
}

// Get current date/time, format is YYYY-MM-DD.HH:mm:ss
const std::string GetCurrentDateTime() {
  time_t     now = time(0);
  struct tm  tstruct;
  char       buf[80];
  tstruct = *localtime(&now);
  // Visit http://en.cppreference.com/w/cpp/chrono/c/strftime
  // for more information about date/time format
  strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);
  
  return buf;
}

clock_t SystemInfo::GetElapsedTime() {
  time_t end_t = time(NULL);;
  return end_t - start_clock_;
}

stats_load SystemInfo::GetAvgLoad() {
  stats_load st_load;
  tsar_.CollectLoadStat(&st_load);
  return st_load;
}
  
void SystemInfo::GetNicState(const string& interface,
                             NetworkTrafficStat* nic_stat) {
  std::lock_guard<std::mutex> guard(cpu_queue_mutex_);
  nic_stat->CopyFrom(interface_stats_[interface]);
}

static std::string ReadableTraffic(long inbytes_per_second) {
  if (inbytes_per_second < 1024) {
    return StringPrintf("%d Bytes/s", inbytes_per_second);
  } else if (inbytes_per_second < 1024 * 1024) {
    return StringPrintf("%3.2f KB/s", (float)inbytes_per_second / 1024);
  } else {
      return StringPrintf("%3.2f MB/s", (float)inbytes_per_second / (1024 * 1024));
  }
  return "";
}

std::string SystemInfo::GetReadableTrafficStat(const string& interface) {
  std::lock_guard<std::mutex> guard(cpu_queue_mutex_);
  NetworkTrafficStat stat = interface_stats_[interface];
  return stat.nic_readable_traffic();
}

void SystemInfo::CollectNetworkStats() {
  std::vector<stats_pernic> all_nics;
  tsar_.CollectPerNicStat(&all_nics);
  int now = getTimeMS();
  for (auto nic : all_nics) {
    std::string nic_name = nic.name;
    if (nic_name == "lo") {
      continue;
    }
    {
      std::lock_guard<std::mutex> guard(cpu_queue_mutex_);
      NetworkTrafficStat stat = interface_stats_[nic_name];
    
      int time_diff_ms = now - stat.last_nic_collect_time();
      if (stat.last_nic_collect_time() == 0 || time_diff_ms == 0) {
        stat.set_interface(nic_name);
        stat.set_last_nic_collect_time(now);
        stat.set_last_bytein(nic.bytein);
        stat.set_last_byteout(nic.byteout);
        stat.set_inbytes_per_second(0);
        stat.set_outbytes_per_second(0);
        stat.set_nic_readable_traffic("~");
      } else {
        int inbytes_per_second = (nic.bytein - stat.last_bytein()) / time_diff_ms * 1000;
        int outbytes_per_second = (nic.byteout - stat.last_byteout()) / time_diff_ms * 1000;
        std::string ret = "inboud traffic=";
        ret += ReadableTraffic(inbytes_per_second);
        ret += ", outbound traffic=";
        ret += ReadableTraffic(outbytes_per_second);
        stat.set_last_nic_collect_time(now);
        stat.set_last_bytein(nic.bytein);
        stat.set_last_byteout(nic.byteout);
        stat.set_inbytes_per_second(inbytes_per_second);
        stat.set_outbytes_per_second(outbytes_per_second);
        stat.set_nic_readable_traffic(ret);
      }
      interface_stats_[nic_name] = stat;
    }
  }
}

void SystemInfo::FillCpuAverageLoad() {
  stats_load st_load;
  tsar_.CollectLoadStat(&st_load);
  avg_load_.set_load_avg_1(st_load.load_avg_1);
  avg_load_.set_load_avg_5(st_load.load_avg_5);
  avg_load_.set_load_avg_15(st_load.load_avg_15);
  avg_load_.set_nr_threads(st_load.nr_threads);
}

health::HealthStatus SystemInfo::GetHealthStatus() {
  std::lock_guard<std::mutex> guard(health_status_mutex_);
  return health_status_;
}

void SystemInfo::FillHealthStatus(double cpu_load, double process_load) {
  std::lock_guard<std::mutex> guard(health_status_mutex_);
  health_status_.set_ts(getTimeMS());
  NetworkTrafficStat nic_stat;
  GetNicState(network_interface_, &nic_stat);
  SystemStat* system_stat = health_status_.mutable_system();
  system_stat->mutable_network()->CopyFrom(nic_stat);
  
  vector<stats_percpu> all_cpus;
  tsar_.CollectPerCpuStat(&all_cpus);
  system_stat->set_cpu_number(all_cpus.size());

  system_stat->mutable_load()->CopyFrom(avg_load_);

  system_stat->set_cpu_usage_percent(cpu_load);
  system_stat->set_total_vm_size(GetTotalVM());
  system_stat->set_total_pm_size(GetTotalPhysicalMemory());
  system_stat->set_used_vm_size(GetTotalUsedVM());
  system_stat->set_used_pm_size(GetTotalUsedPhysicalMemory());

  ProcessLevelStat* process_stat = health_status_.mutable_process();
  process_stat->set_process_use_cpu_percent(process_load);
  process_stat->set_used_vm_process(GetUsedVMByCurrentProcess());
  process_stat->set_used_pm_process(GetUsedPhysicalMemoryByCurrentProcess());
  process_stat->set_thread_count(GetCurrentProcessThreadCount());
}

void SystemInfo::Init() {
  start_time_ = GetCurrentDateTime();
  start_clock_ = time(NULL);
  std::thread t([&] () {
    /* Set thread name */
    prctl(PR_SET_NAME, (unsigned long)"SystemInfo");

    while(true) {
      double cpu_load = GetTotalCpuLoad();
      double process_load = GetCurrentProcessCpuLoad();
      if (cpu_load != -1 && !isnan(cpu_load) && !isnan(process_load)) {
        std::lock_guard<std::mutex> guard(cpu_queue_mutex_);
        cpu_loads_.push(cpu_load);
        while (cpu_loads_.size() > 30) {
          cpu_loads_.pop();
        }
        process_loads_.push(process_load);
        while (process_loads_.size() > 30) {
          process_loads_.pop();
        }
      }
      CollectNetworkStats();
      FillCpuAverageLoad();
      FillHealthStatus(cpu_load, process_load);
      //LOG(INFO) << "health_status=" << health_status_.DebugString();
      std::this_thread::sleep_for(std::chrono::seconds(10));
    }
  });
  t.detach();
}

}  // namespace orbit
