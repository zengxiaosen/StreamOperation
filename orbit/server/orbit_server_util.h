/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * orbit_util.h
 * ---------------------------------------------------------------------------
 * Defines the interface and implementation of the useful util functions..
 * ---------------------------------------------------------------------------
 */

#ifndef ORBIT_SERVER_UTIL_H__
#define ORBIT_SERVER_UTIL_H__
#include <string>
namespace olive {
  class IceCandidate;
}
namespace orbit {
  // Parse the JSON object to IceCandidate
  // {"candidate":"a=candidate:2 1 udp 2013266431 192.168.1.101 41606 typ host generation 0","sdpMLineIndex":"0","sdpMid":"video"}
  bool ParseJSONString(const std::string& json, olive::IceCandidate* ice_candidate);
}

#endif  // ORBIT_SERVER_UTIL_H__
