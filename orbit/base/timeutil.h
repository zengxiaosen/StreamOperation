/*
 * Copyright 2016 (C) Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * timeutil.h --- defines the util function for time.
 */
#pragma once
#include "base.h"
namespace orbit {
  long long getTimeMS();
  long long GetCurrentTime_MS();
  long long GetCurrentTime_US();

  class SimpleClock {
  public:
    SimpleClock() {
      Start();
    }
    int ElapsedTime() {
      End();
      return (end_time_ms_ - start_time_ms_);
    }
    void PrintDuration() {
      End();
      LOG(INFO) << "Elapsed:" << ElapsedTime() << " ms";
    }
  private:
    void Start() {
      start_time_ms_ = GetCurrentTime_MS();
    }
    void End() {
      end_time_ms_ = GetCurrentTime_MS();
    }
    long long start_time_ms_;
    long long end_time_ms_;
  };
}  // orbit

