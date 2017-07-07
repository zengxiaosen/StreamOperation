/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: zhihan@orangelab.cn (Zhihan He)
 *
 * slavedata_collector.h
 * ---------------------------------------------------------------------------
 * Defines the collector for collector slave data and show out.
 * ---------------------------------------------------------------------------
 */

#pragma once

#include "stream_service/proto/registry.grpc.pb.h"
#include "stream_service/orbit/base/strutil.h"

namespace orbit {
  struct ServerStat {
    registry::ServerInfo info;
    health::HealthStatus status;
    long last_ts;
  };
  static string HealthStatusToJsonData(string host, int port, const health::HealthStatus& status) {
      auto systemInfo = status.system();
      auto process = status.process();
      return StringPrintf("{\"host\":\"%s\",\"port\":%d,\"ts\":%ld,\
                            \"system\" : {\
                              \"cpu_number\" : %d,\
                              \"cpu_usage_percent\":%f,\
                              \"total_vm_size\":%ld,\
                              \"total_pm_size\":%ld,\
                              \"used_vm_size\":%ld,\
                              \"used_pm_size\":%ld,\
                              \"traffic\":\"%s\",\
                              \"load\" : {\
                                \"load_avg_1\": %d,\
                                \"load_avg_5\": %d,\
                                \"load_avg_15\":%d,\
                                \"nr_threads\": %d\
                              }\
                            },\
                            \"process\" : {\
                              \"process_use_cpu_percent\": %d,\
                              \"used_vm_process\": %d,\
                              \"used_pm_process\": %d,\
                              \"thread_count\": %d,\
                            }\
                          }",
                          host.c_str(),
                          port,
                          status.ts(),
                          systemInfo.cpu_number(),
                          systemInfo.cpu_usage_percent(),
                          systemInfo.total_vm_size(),
                          systemInfo.total_pm_size(),
                          systemInfo.used_vm_size(),
                          systemInfo.used_pm_size(),
                          systemInfo.network().nic_readable_traffic().c_str(),
                          systemInfo.load().load_avg_1(),
                          systemInfo.load().load_avg_5(),
                          systemInfo.load().load_avg_15(),
                          systemInfo.load().nr_threads(),
                          process.process_use_cpu_percent(),
                          process.used_vm_process(),
                          process.used_pm_process(),
                          process.thread_count());
    }
}

