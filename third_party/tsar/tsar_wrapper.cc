/*
 * Copyright 2016 (C) Orange Labs Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 * 
 */

#include "tsar_wrapper.h"
#include "upstream/define.h"
#include <string>

#include <mntent.h>
#include <sys/statfs.h>

using namespace std;

#define MAXPART 32

namespace orbit {
namespace {
  // ------------------------ mod_cpu.c ---------------------
static void read_cpu_stats(struct stats_cpu* mod) {
    FILE   *fp, *ncpufp;
    char    line[LEN_4096];
    struct  stats_cpu st_cpu;
    memset(&st_cpu, 0, sizeof(struct stats_cpu));
    //unsigned long long cpu_util;
    if ((fp = fopen(STAT, "r")) == NULL) {
        return;
    }
    while (fgets(line, LEN_4096, fp) != NULL) {
        if (!strncmp(line, "cpu ", 4)) {
            /*
             * Read the number of jiffies spent in the different modes
             * (user, nice, etc.) among all proc. CPU usage is not reduced
             * to one processor to avoid rounding problems.
             */
            sscanf(line + 5, "%llu %llu %llu %llu %llu %llu %llu %llu %llu",
                    &st_cpu.cpu_user,
                    &st_cpu.cpu_nice,
                    &st_cpu.cpu_sys,
                    &st_cpu.cpu_idle,
                    &st_cpu.cpu_iowait,
                    &st_cpu.cpu_hardirq,
                    &st_cpu.cpu_softirq,
                    &st_cpu.cpu_steal,
                    &st_cpu.cpu_guest);
        }
    }

    /* get cpu number */
    if ((ncpufp = fopen("/proc/cpuinfo", "r")) == NULL) {
        fclose(fp);
        return;
    }
    while (fgets(line, LEN_4096, ncpufp)) {
        if (!strncmp(line, "processor\t:", 11))
            st_cpu.cpu_number++;
    }
    fclose(ncpufp);
    if (fclose(fp) < 0) {
        return;
    }
    memcpy(mod, &st_cpu, STATS_CPU_SIZE);
    return;
}

  // ------------------------ mod_mem.c ---------------------
  
  static void read_mem_stats(stats_mem* mod) {
    FILE             *fp;
    char              line[LEN_128];
    struct stats_mem  st_mem;

    memset(&st_mem, 0, sizeof(struct stats_mem));
    if ((fp = fopen(MEMINFO, "r")) == NULL) {
        return;
    }

    while (fgets(line, 128, fp) != NULL) {

        if (!strncmp(line, "MemTotal:", 9)) {
            /* Read the total amount of memory in kB */
            sscanf(line + 9, "%lu", &st_mem.tlmkb);
        }
        else if (!strncmp(line, "MemFree:", 8)) {
            /* Read the amount of free memory in kB */
            sscanf(line + 8, "%lu", &st_mem.frmkb);
        }
        else if (!strncmp(line, "Buffers:", 8)) {
            /* Read the amount of buffered memory in kB */
            sscanf(line + 8, "%lu", &st_mem.bufkb);
        }
        else if (!strncmp(line, "Cached:", 7)) {
            /* Read the amount of cached memory in kB */
            sscanf(line + 7, "%lu", &st_mem.camkb);
        }
        else if (!strncmp(line, "Active:", 7)) {
            /* Read the amount of Active memory in kB */
            sscanf(line + 7, "%lu", &st_mem.acmkb);
        }
        else if (!strncmp(line, "Inactive:", 9)) {
            /* Read the amount of Inactive memory in kB */
            sscanf(line + 9, "%lu", &st_mem.iamkb);
        }
        else if (!strncmp(line, "Slab:", 5)) {
            /* Read the amount of Slab memory in kB */
            sscanf(line + 5, "%lu", &st_mem.slmkb);
        }
        else if (!strncmp(line, "SwapCached:", 11)) {
            /* Read the amount of cached swap in kB */
            sscanf(line + 11, "%lu", &st_mem.caskb);
        }
        else if (!strncmp(line, "SwapTotal:", 10)) {
            /* Read the total amount of swap memory in kB */
            sscanf(line + 10, "%lu", &st_mem.tlskb);
        }
        else if (!strncmp(line, "SwapFree:", 9)) {
            /* Read the amount of free swap memory in kB */
            sscanf(line + 9, "%lu", &st_mem.frskb);
        }
        else if (!strncmp(line, "Committed_AS:", 13)) {
            /* Read the amount of commited memory in kB */
            sscanf(line + 13, "%lu", &st_mem.comkb);
        }
    }

    fclose(fp);
    memcpy(mod, &st_mem, STATS_MEM_SIZE);
    return;
  }

  // ------------------------ mod_load.c ---------------------
  void read_stat_load(struct stats_load* mod) {
    int     load_tmp[3];
    FILE   *fp;
    struct  stats_load st_load;
    memset(&st_load, 0, sizeof(struct stats_load));

    if ((fp = fopen(LOADAVG, "r")) == NULL) {
        return;
    }

    /* Read load averages and queue length */
    if (fscanf(fp, "%d.%d %d.%d %d.%d %ld/%d %*d\n",
            &load_tmp[0], &st_load.load_avg_1,
            &load_tmp[1], &st_load.load_avg_5,
            &load_tmp[2], &st_load.load_avg_15,
            &st_load.nr_running,
            &st_load.nr_threads) != 8)
    {
        fclose(fp);
        return;
    }
    st_load.load_avg_1  += load_tmp[0] * 100;
    st_load.load_avg_5  += load_tmp[1] * 100;
    st_load.load_avg_15 += load_tmp[2] * 100;

    if (st_load.nr_running) {
        /* Do not take current process into account */
        st_load.nr_running--;
    }
    fclose(fp);
    memcpy(mod, &st_load, STATS_LOAD_SIZE);    
  }

  // ------------------------ mod_proc.c ---------------------
  static void read_proc_stats(struct stats_proc* mod, const char *parameter) {
    int     nb = 0, pid[16];
    char    filename[128], line[256];
    FILE   *fp;
    struct  stats_proc st_proc;

    memset(&st_proc, 0, sizeof(struct stats_proc));

    /* get pid by proc name */
    if (strlen(parameter) > 20) {
        return;
    }
    char cmd[32] = "/bin/pidof ";
    strncat(cmd, parameter, sizeof(cmd) - strlen(cmd) -1);
    char  spid[256];
    fp = popen(cmd, "r");
    if (fp == NULL) {
        return;
    }
    if(fscanf(fp, "%s", spid) == EOF) {
        pclose(fp);
        return;
    }
    pclose(fp);
    /* split pidof into array pid */
    char *p;
    p = strtok(spid, " ");
    while (p) {
        pid[nb] = atoi(p);
        if (pid[nb++] <= 0) {
            return;
        }
        if (nb >= 16) {
            return;
        }
        p = strtok(NULL, " ");
    }
    /* get all pid's info */
    while (--nb >= 0) {
        unsigned long long data;
        /* read values from /proc/pid/stat */
        sprintf(filename, PID_STAT, pid[nb]);
        if ((fp = fopen(filename, "r")) == NULL) {
            return;
        }
        unsigned long long cpudata[4];
        if (fgets(line, 256, fp) == NULL) {
            fclose(fp);
            return;
        }

        p = strstr(line, ")");
        if (sscanf(p, "%*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %llu %llu %llu %llu",
                    &cpudata[0], &cpudata[1], &cpudata[2], &cpudata[3]) == EOF) {
            fclose(fp);
            return;
        }
        st_proc.user_cpu += cpudata[0];
        st_proc.user_cpu += cpudata[2];
        st_proc.sys_cpu += cpudata[1];
        st_proc.sys_cpu += cpudata[3];
        fclose(fp);
        /* get cpu&mem info from /proc/pid/status */
        sprintf(filename, PID_STATUS, pid[nb]);
        if ((fp = fopen(filename, "r")) == NULL) {
            return;
        }
        while (fgets(line, 256, fp) != NULL) {
            if (!strncmp(line, "VmRSS:", 6)) {
                sscanf(line + 6, "%llu", &data);
                st_proc.mem += data * 1024;
            }
        }
        fclose(fp);
        /* get io info from /proc/pid/io */
        sprintf(filename, PID_IO, pid[nb]);
        if ((fp = fopen(filename, "r")) == NULL) {
            return;
        }
        while (fgets(line, 256, fp) != NULL) {
            if (!strncmp(line, "read_bytes:", 11)) {
                sscanf(line + 11, "%llu", &data);
                st_proc.read_bytes += data;
            }
            else if (!strncmp(line, "write_bytes:", 12)) {
                sscanf(line + 12, "%llu", &data);
                st_proc.write_bytes += data;
            }
        }
        fclose(fp);
    }

    /* read+calc cpu total time from /proc/stat */
    fp = fopen("/proc/stat", "r");
    if (fp == NULL) {
        fclose(fp);
        return;
    }
    unsigned long long cpu_time[10];
    bzero(cpu_time, sizeof(cpu_time));
    if (fscanf(fp, "%*s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                &cpu_time[0], &cpu_time[1], &cpu_time[2], &cpu_time[3],
                &cpu_time[4], &cpu_time[5], &cpu_time[6], &cpu_time[7],
                &cpu_time[8], &cpu_time[9]) == EOF) {
        fclose(fp);
        return;
    }
    fclose(fp);
    int i;
    for(i=0; i < 10;i++) {
        st_proc.total_cpu += cpu_time[i];
    }
    /* read total mem from /proc/meminfo */
    fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        fclose(fp);
        return;
    }
    if (fscanf(fp, "MemTotal:      %llu kB", &st_proc.total_mem) == EOF) {
        fclose(fp);
        return;
    }
    st_proc.total_mem *= 1024;
    fclose(fp);
    memcpy(mod, &st_proc, STATS_PROC_SIZE);
  }


  // ------------------------ mod_pernic.c ---------------------
  /*
   * collect pernic infomation
   */
  static void read_pernic_stats(vector<stats_pernic>* all_nics) {
    int                   pos = 0, nics = 0;
    FILE                 *fp;
    char                  line[LEN_1M] = {0};
    struct stats_pernic   st_pernic;

    memset(&st_pernic, 0, sizeof(struct stats_pernic));

    if ((fp = fopen(NET_DEV, "r")) == NULL) {
        return;
    }

    while (fgets(line, LEN_1M, fp) != NULL) {
        memset(&st_pernic, 0, sizeof(st_pernic));
        if (!strstr(line, ":")) {
            continue;
        }
        sscanf(line,  "%*[^a-z]%[^:]:%llu %llu %*u %*u %*u %*u %*u %*u "
                "%llu %llu %*u %*u %*u %*u %*u %*u",
                st_pernic.name,
                &st_pernic.bytein,
                &st_pernic.pktin,
                &st_pernic.byteout,
                &st_pernic.pktout);
        /* if nic not used, skip it */
        if (st_pernic.bytein == 0) {
            continue;
        }

        nics++;
        if (nics > MAX_NICS) {
          break;
        }
        all_nics->push_back(st_pernic);
    }
    fclose(fp);
    return;
  }

  // ------------------------ mod_percpu.c ---------------------
  static void read_percpu_stats(vector<stats_percpu>* all_cpus) {
    int                  pos = 0, cpus = 0;
    FILE                *fp;
    char                 line[LEN_1M];
    struct stats_percpu  st_percpu;

    memset(&st_percpu, 0, STATS_PERCPU_SIZE);
    if ((fp = fopen(STAT_PATH, "r")) == NULL) {
        return;
    }
    while (fgets(line, LEN_1M, fp) != NULL) {
        if (!strncmp(line, "cpu", 3)) {
            /*
             * Read the number of jiffies spent in the different modes
             * (user, nice, etc.) among all proc. CPU usage is not reduced
             * to one processor to avoid rounding problems.
             */
            sscanf(line, "%s %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                    st_percpu.cpu_name,
                    &st_percpu.cpu_user,
                    &st_percpu.cpu_nice,
                    &st_percpu.cpu_sys,
                    &st_percpu.cpu_idle,
                    &st_percpu.cpu_iowait,
                    &st_percpu.cpu_hardirq,
                    &st_percpu.cpu_softirq,
                    &st_percpu.cpu_steal,
                    &st_percpu.cpu_guest);
            if (st_percpu.cpu_name[3] == '\0') //ignore cpu summary stat
                continue;
            all_cpus->push_back(st_percpu);
            cpus++;
            if (cpus > MAX_CPUS) {
              break;
            }
        }
    }
    fclose(fp);
    return;
}

}  // annoymous namespace
  
  void TsarWrapper::CollectCpuStat(stats_cpu* st_cpu) {
    read_cpu_stats(st_cpu);
  }

  void TsarWrapper::CollectMemStat(stats_mem* st_mem) {
    read_mem_stats(st_mem);
  }

  void TsarWrapper::CollectLoadStat(stats_load* st_load) {
    read_stat_load(st_load);
  }

  void TsarWrapper::CollectProcStat(string proc_name, stats_proc* st_proc) {
    read_proc_stats(st_proc, proc_name.c_str());
  }

  void TsarWrapper::CollectPerCpuStat(std::vector<stats_percpu>* all_cpus) {
    read_percpu_stats(all_cpus);
  }
  void TsarWrapper::CollectPerNicStat(std::vector<stats_pernic>* all_nics) {
    read_pernic_stats(all_nics);
  }

}  // namespace orbit
