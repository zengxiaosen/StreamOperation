/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * rpc_call_stats.h
 * ---------------------------------------------------------------------------
 * Defines a rpc_call_stats class to record the RPC call statisticals.
 * ---------------------------------------------------------------------------
 */
#pragma once 

#include "glog/logging.h"

#include <string>
#include <map>
#include <list>
#include <mutex>
#include <chrono>
#include <memory>

#include "stream_service/orbit/base/singleton.h"
#include "stream_service/orbit/base/strutil.h"

namespace orbit {

// Example usage
// class RPCService {
//   grpc::Status Create(grpc::ServerContext* context.....) {
//      RpcCallStats stat("RPCService_Create");
//      if (!ok) {
//        stat.Fail();
//      }
//   }
// }
// And then in the /rpcz handler in the browser:
//  http://HOST:PORT/rpcz
// and it will display the call numbers stats by each call. And also
// the time histogram of each call.

class RpcCallStats {
 public:
  RpcCallStats(const std::string& method_name, std::string type="default");
  ~RpcCallStats();
  void Fail();

 private:
  bool is_fail_;
  std::string method_name_;
  std::string request_type_;

  std::chrono::high_resolution_clock::time_point begin_;
  std::chrono::high_resolution_clock::time_point end_;
};

struct RpcCallStatsData {
  RpcCallStatsData() {
    total_call = 0;
    failed_call = 0;
    success_call = 0;
    last_call_latency_nano = -1;
  }

  // Update the value in stat.
  void AddValue(long call_time) {
    add_value_count++;
    total_call_time += call_time;
    mean_call_time_ms = total_call_time / add_value_count;
    if (min_call_time_ms == -1) {
      min_call_time_ms = call_time;
    } else {
      if (min_call_time_ms > call_time) {
        min_call_time_ms = call_time;
      }
    }
    if (max_call_time_ms < call_time) {
      max_call_time_ms = call_time;
    }
   
    // Update last ten times call statistic
    call_time_queue_.push_back(call_time); 
    while (call_time_queue_.size() > 10) {
      call_time_queue_.pop_front();
    }
  }

  std::string GetLastTenTimesCallStat() {
    std::string ret_str;
    for (auto it : call_time_queue_) {
      std::stringstream ss_call_time;
      std::string str_call_time;
      ss_call_time << it;
      ss_call_time >> str_call_time;
      ret_str += str_call_time + ", ";
    }
    return ret_str;
  }

  int total_call;
  int success_call;
  int failed_call;
  long last_call_latency_nano;

  long mean_call_time_ms = 0;
  long min_call_time_ms = -1;
  long max_call_time_ms = 0;

private:
  long add_value_count = 0;
  long long total_call_time = 0;
  std::list<long> call_time_queue_;

};

struct CallRecordData {
  std::chrono::high_resolution_clock::time_point time;
  std::string method;
  std::string type;
  long call_time;
  bool result;
};

class RpcCallStatsManager {
 typedef std::map<std::string, RpcCallStatsData> RpcCallStatsMap;
 public:
   RpcCallStatsData GetCallStat(const std::string& method) {
     std::lock_guard<std::mutex> guard(var_mutex_);
     return rpc_call_data_map_[method];
   }

   std::vector<std::string> GetAllMethods() {
     std::lock_guard<std::mutex> guard(var_mutex_);
     std::vector<std::string> ret;
     for(auto iter : rpc_call_data_map_) {
       std::string key = iter.first;
       ret.push_back(key);
     }
     return ret;
   }

   std::string GetLastTenTimesCallStat(const std::string& method) {
     std::lock_guard<std::mutex> guard(var_mutex_);
     RpcCallStatsData stat = rpc_call_data_map_[method];
     return stat.GetLastTenTimesCallStat();
   }

   std::vector<int> GetCallRecordStat() {
     std::list<CallRecordData> cur_list;
     {
       std::lock_guard<std::mutex> guard(call_record_list_mutex_);
       cur_list = call_record_list_;
     }
     // TODO compute and show the status
     std::vector<int> call_count;
     if (cur_list.size() > 0) {
       CallRecordData first_record = cur_list.front();
       std::chrono::high_resolution_clock::time_point base_time = first_record.time;

       int idx = 0;
       int counts[1800] = {0};

       for (auto ite = cur_list.begin(); ite != cur_list.end(); ite ++) {
         std::chrono::high_resolution_clock::time_point next_time = ite->time;
         std::chrono::duration<double, std::ratio<1, 1>> durations(next_time - base_time);
         int past_seconds = durations.count();
         if (past_seconds >= 0 && past_seconds < 1800) {
           counts[past_seconds] ++;
           if (idx < past_seconds) {
             idx = past_seconds;
           }
         }
       }

       for (auto i = 0; i <= idx; i ++) {
         call_count.push_back(counts[i]);
       }
     }
     
     return call_count;
   }

   // Get grpc qps data
   std::string GetGrpcQpsDataString() {
     std::vector<int> grpc_datas = GetCallRecordStat();
     std::string qps_str;

     // Get last 20 second grpc qps data string
     int show_num = 20;
     int first_data_idx = 0;
     if (grpc_datas.size() > 20) {
       first_data_idx = grpc_datas.size() - show_num;
     }

     for (auto i = first_data_idx; i < grpc_datas.size(); i ++) {
       std::string tmp_str = StringPrintf("%d ", grpc_datas[i]);
       qps_str += tmp_str;
     }
     
     return qps_str;
   }

 private:
   void FailCall(const std::string& method) {
     std::lock_guard<std::mutex> guard(var_mutex_);
     RpcCallStatsData stat = rpc_call_data_map_[method];
     stat.failed_call = stat.failed_call + 1;
     rpc_call_data_map_[method] = stat;
   }

   void StartCall(const std::string& method) {
     std::lock_guard<std::mutex> guard(var_mutex_);
     RpcCallStatsData stat = rpc_call_data_map_[method];
     stat.total_call = stat.total_call + 1;     
     rpc_call_data_map_[method] = stat;
   }

   void SuccessCall(const std::string& method) {
     std::lock_guard<std::mutex> guard(var_mutex_);
     RpcCallStatsData stat = rpc_call_data_map_[method];
     stat.success_call = stat.success_call + 1;
     rpc_call_data_map_[method] = stat;
   }

   void NanoTime(const std::string& method, long nano_time) {
     std::lock_guard<std::mutex> guard(var_mutex_);
     RpcCallStatsData stat = rpc_call_data_map_[method];
     stat.last_call_latency_nano = nano_time;
     long call_time_in_ms = nano_time / (1000 * 1000);
     //stat.latencies->addValue(call_time_in_ms);
     stat.AddValue(call_time_in_ms);
     rpc_call_data_map_[method] = stat;
   }

    void AddCallRecord(std::chrono::high_resolution_clock::time_point time, 
       const std::string& method, const std::string& type, long nano_time, bool result) {
      std::lock_guard<std::mutex> guard(call_record_list_mutex_);
      long call_time = nano_time / (1000 * 1000);
      // insert
      CallRecordData cr;
      cr.time = time;
      cr.method = method;
      cr.type = type;
      cr.call_time = call_time;
      cr.result = result;
      call_record_list_.push_back(cr);
      // clear 
      int list_size = call_record_list_.size();
      CallRecordData end_node = cr;
      while (list_size > 0) {
        CallRecordData node = call_record_list_.front(); 
        long time_diff = std::chrono::duration_cast<std::chrono::nanoseconds>(end_node.time - node.time).count();
        // time space : 3 minutes
        if ( time_diff/(1000 * 1000) > 180000 ) {
          call_record_list_.pop_front();
          list_size --;
        } else {
          break;
        }
      }
    }

    std::mutex var_mutex_;
    RpcCallStatsMap rpc_call_data_map_;

    // call record queue
    std::mutex call_record_list_mutex_;
    std::list<CallRecordData> call_record_list_;

    friend class RpcCallStats;
    DEFINE_AS_SINGLETON(RpcCallStatsManager);
};
}  // pace orbit
