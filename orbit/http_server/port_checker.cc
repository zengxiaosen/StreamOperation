/*
 * Copyright 2016 All Rights Reserved.
 * Author: zhanghao@orangelab.cn
 *
 * port_checker.cc
 * ---------------------------------------------------------------------------
 * Implements the class to check that the port if is in used.
 * ---------------------------------------------------------------------------
 */

#include "port_checker.h"

#include "gflags/gflags.h"
#include "glog/logging.h"

#include <unistd.h>
#include <sys/prctl.h>

namespace orbit {

  PortChecker::PortChecker(const bool& flag, const int& port, const int& timeout) {
    t.reset(new std::thread([&] () {
      prctl(PR_SET_NAME, (unsigned long)"PortCheck");

      sleep(timeout);
      if (flag) {
        LOG(FATAL) << "Port " << port << " has been used. ";
      }
    }));
  }

  PortChecker::~PortChecker() {
    // Wait for the thread to end.
    if (t) {
      t->join();
    }
  }
  
}

