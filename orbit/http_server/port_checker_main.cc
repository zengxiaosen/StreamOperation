/*
 * Copyright 2016 All Rights Reserved.
 * Author: zhanghao@orangelab.cn
 *
 * port_checker_main.cc
 * ---------------------------------------------------------------------------
 * Implements the class to check that the port if is in used.
 * ---------------------------------------------------------------------------
 */

#include "port_checker.h"

#include <unistd.h>

#include "gflags/gflags.h"
#include "glog/logging.h"

namespace PopStackTest {
  class Test {
  public:
    Test(int id) : id_(id) {
      LOG(INFO) << "Pop test id = " << id_;
    }

    ~Test() {
      LOG(INFO) << "Destroy test id = " << id_;
    }

  private:
    int id_;
  };
  
} // namespace PopStackTest

int main(int argc, char** argv) {

  bool port_is_used = true;
  int port = 10000;
  int timeout = 3;
  orbit::PortChecker port_checker(port_is_used, port, timeout);

  sleep(1); // less 3s the ok
  //sleep(4); // do something then crash
  
  port_is_used = false;

  // When the main end, first it can destroy t2, then destroy t1
  PopStackTest::Test t1(1);
  PopStackTest::Test t2(2);

  return 0;
}
