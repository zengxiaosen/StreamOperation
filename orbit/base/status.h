/*
 * Copyright 2016 (C) Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 * --------------------------------------------------------------------------
 * status.h
 * -- defines the basic status information.
 *
 * Reference from: https://github.com/google/or-tools
 * --------------------------------------------------------------------------
 */

#ifndef ORBIT_BASE_STATUS_H__
#define ORBIT_BASE_STATUS_H__

#include <string>
#include "strutil.h"
#include "glog/logging.h"

namespace orbit {
namespace util {

namespace error {
enum Error {
  INTERNAL = 1,
  INVALID_ARGUMENT = 2,
  DEADLINE_EXCEEDED = 3,
};
}  // namespace error

struct Status {
  enum { OK = 0 };
  Status() : error_code_(OK) {}
  Status(int error_code) : error_code_(error_code) {}  // NOLINT
  Status(int error_code, const std::string& error_message)
      : error_code_(error_code), error_message_(error_message) {}
  Status(const Status& other)
      : error_code_(other.error_code_), error_message_(other.error_message_) {}

  bool ok() const { return error_code_ == OK; }

  std::string ToString() const {
    if (ok()) return "OK";
    //return StrCat("ERROR #", error_code_, ": '", error_message_, "'");
    return StringPrintf("ERROR #%d: '%s'", error_code_, error_message_.c_str());
  }

  std::string error_message() const { return error_message_; }

  void IgnoreError() const {}

 private:
  int error_code_;
  std::string error_message_;
};

inline std::ostream& operator<<(std::ostream& out, const Status& status) {
  out << status.ToString();
  return out;
}

}  // namespace util

#define CHECK_OK(status) CHECK_EQ("OK", (status).ToString())

}  // namespace orbit

#endif  // ORBIT_BASE_STATUS_H__
