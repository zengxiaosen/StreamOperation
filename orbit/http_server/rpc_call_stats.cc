/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * rpc_call_stats.cc
 * ---------------------------------------------------------------------------
 * Implements a rpc_call_stats class to record the RPC call statisticals.
 * ---------------------------------------------------------------------------
 */
#include "rpc_call_stats.h"

namespace orbit {
using namespace std;

  RpcCallStats::RpcCallStats(const std::string& method_name, std::string type) {
    method_name_ = method_name;
    request_type_ = type;
    is_fail_ = false;
    VLOG(4) << "method_name_=" << method_name_;
    begin_ = std::chrono::high_resolution_clock::now();
    RpcCallStatsManager* stats_manager = Singleton<RpcCallStatsManager>::GetInstance();
    stats_manager->StartCall(method_name_);
  }

  RpcCallStats::~RpcCallStats() {
    end_ = std::chrono::high_resolution_clock::now();
    RpcCallStatsManager* stats_manager = Singleton<RpcCallStatsManager>::GetInstance();
    if (is_fail_) {
      stats_manager->FailCall(method_name_);
      stats_manager->AddCallRecord(begin_, method_name_, request_type_, 0, false);
      // If the RPC call is failed, we do not use the histogram to calculate the latency.
    } else {
      stats_manager->SuccessCall(method_name_);
      long elapsed_nano_second = std::chrono::duration_cast<std::chrono::nanoseconds>(end_-begin_).count();
      VLOG(4) << "elapsed_nano_second=" << elapsed_nano_second;
      stats_manager->NanoTime(method_name_, elapsed_nano_second);
      stats_manager->AddCallRecord(begin_, method_name_, request_type_, elapsed_nano_second, true);
    }
  } 

  void RpcCallStats::Fail() {
    is_fail_ = true;
  }

}  // namespace orbit
