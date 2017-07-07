/*
 * Copyright 2016 (C) Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 * --------------------------------------------------------------------------
 * timeutil.cc
 *  -- implements the util functions for time.
 * --------------------------------------------------------------------------
 */

#include "timeutil.h"
#include <sys/time.h>
#include <cstddef>

namespace orbit {
  long long getTimeMS() {
    struct timeval start;
    gettimeofday(&start, NULL);
    long timeMs = ((start.tv_sec) * 1000 + start.tv_usec/1000.0);
    return timeMs;
  }

  long long GetCurrentTime_MS() {
    return getTimeMS();
  }

  long long GetCurrentTime_US() {
    struct timeval start;
    gettimeofday(&start, NULL);
    long time_us = ((start.tv_sec) * 1000000 + start.tv_usec);
    return time_us;
  }

}
