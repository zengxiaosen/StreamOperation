/*
 * Copyright 2016 All Rights Reserved.
 * Author: zhanghao@orangelab.cn
 *
 * port_checker.h
 * ---------------------------------------------------------------------------
 * Implements the class to check that the port if is in used.
 * ---------------------------------------------------------------------------
 */

#pragma once

#ifndef PORT_CHECKER_H_
#define PORT_CHECKER_H_

#include <thread>

namespace orbit {

class PortChecker {
 public:
  PortChecker(const bool& flag, const int& port, const int& timeout);
  ~PortChecker();

 private:
  std::unique_ptr<std::thread> t;
};

} // namespace orbit
#endif

