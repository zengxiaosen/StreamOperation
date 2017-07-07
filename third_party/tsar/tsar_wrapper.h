/*
 * Copyright 2016 (C) Orange Labs Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 * 
 * - Example usage:
 *------------------------------------------------------------------------
 *------------------------------------------------------------------------
 */

#pragma once

#include "glog/logging.h"

#include <string>
#include <vector>

namespace orbit {
/*
 * Structure for CPU infomation.
 */
struct stats_cpu {
    unsigned long long cpu_user;
    unsigned long long cpu_nice;
    unsigned long long cpu_sys;
    unsigned long long cpu_idle;
    unsigned long long cpu_iowait;
    unsigned long long cpu_steal;
    unsigned long long cpu_hardirq;
    unsigned long long cpu_softirq;
    unsigned long long cpu_guest;
    unsigned long long cpu_number;
};

#define STATS_CPU_SIZE (sizeof(struct stats_cpu))

/* 
 * Structure for memory and swap space utilization statistics
 */
struct stats_mem {
  /* the amount of free memory in kB */
  unsigned long frmkb;
  /* the amount of buffered memory in kB */
  unsigned long bufkb;
  /* the amount of cached memory in kB */
  unsigned long camkb;
  /* the total amount of memory in kB */
  unsigned long tlmkb;
  /* the amount of Active memory in kB */
  unsigned long acmkb;
  /* the amount of Inactive memory in kB */
  unsigned long iamkb;
  /* the amount of Slab memory in kB */
  unsigned long slmkb;
  /* the amount of free swap memory in kB */
  unsigned long frskb;
  /* the total amount of swap memory in kB */
  unsigned long tlskb;
  /* the amount of cached swap in kB */
  unsigned long caskb;
  /* the amount of commited memory in kB */
  unsigned long comkb;
};
#define STATS_MEM_SIZE sizeof(struct stats_mem)

/* Structure for queue and load statistics */
struct stats_load {
    unsigned long nr_running;
    unsigned int  load_avg_1;
    unsigned int  load_avg_5;
    unsigned int  load_avg_15;
    unsigned int  nr_threads;
};

#define STATS_LOAD_SIZE (sizeof(struct stats_load))
/*
 * Structure for Proc infomation.
 */
struct stats_proc {
    unsigned long long user_cpu;
    unsigned long long sys_cpu;
    unsigned long long total_cpu;
    unsigned long long mem;
    unsigned long long total_mem;
    unsigned long long read_bytes;
    unsigned long long write_bytes;
};

#define PID_IO          "/proc/%u/io"
#define PID_STAT        "/proc/%u/stat"
#define PID_STATUS      "/proc/%u/status"
#define STATS_PROC_SIZE (sizeof(struct stats_proc))

/*
 * Structure for PerCpu infomation.
 */
#define STAT_PATH "/proc/stat"
#define MAX_CPUS 64

struct stats_percpu {
    unsigned long long cpu_user;
    unsigned long long cpu_nice;
    unsigned long long cpu_sys;
    unsigned long long cpu_idle;
    unsigned long long cpu_iowait;
    unsigned long long cpu_steal;
    unsigned long long cpu_hardirq;
    unsigned long long cpu_softirq;
    unsigned long long cpu_guest;
    char cpu_name[10];
};

#define STATS_PERCPU_SIZE (sizeof(struct stats_percpu))

#define MAX_NICS 8

/*
 * Structure for pernic infomation.
 */
struct stats_pernic {
    unsigned long long bytein;
    unsigned long long byteout;
    unsigned long long pktin;
    unsigned long long pktout;
    char name[16];
};

#define STATS_TRAFFIC_SIZE (sizeof(struct stats_pernic))

// Wrapper class for Tsar monitoring.
class TsarWrapper {
 public:
  TsarWrapper() {
  }
  virtual ~TsarWrapper() {
  }
  void CollectCpuStat(stats_cpu* st_cpu);
  void CollectMemStat(stats_mem* st_mem);
  void CollectLoadStat(stats_load* st_load);
  void CollectProcStat(std::string proc_name, stats_proc* st_proc);
  
  void CollectPerCpuStat(std::vector<stats_percpu>* all_cpus);
  void CollectPerNicStat(std::vector<stats_pernic>* all_nics);
};

}  // namespace orbit
