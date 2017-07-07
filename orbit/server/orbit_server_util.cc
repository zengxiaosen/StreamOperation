/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * orbit_util.cc
 * ---------------------------------------------------------------------------
 * Implementation of the util functions..
 * ---------------------------------------------------------------------------
 */

#include "orbit_server_util.h"
#include "glog/logging.h"
#include "stream_service/proto/stream_service.grpc.pb.h"

namespace orbit {
using namespace std;
bool ParseJSONString(const string& json, olive::IceCandidate* ice_candidate) {
    unsigned int start, end;
    string prefix = "candidate\":\"";
    start = json.find(prefix);
    if (start == string::npos) {
      return false;
    }
    start = start + prefix.size();
    end = json.find("\"", start);
    if (end == string::npos) {
      return false;
    }
    string tmp_ice = json.substr(start, end - start);
    LOG(INFO) << "tmp_ice=" << tmp_ice;
    ice_candidate->set_ice_candidate(tmp_ice);
    
    prefix = "sdpMid\":\"";
    start = json.find(prefix);
    start = start + prefix.size();
    if (start == string::npos) {
      return false;
    }
    end = json.find("\"", start);
    if (end == string::npos) {
      return false;
    }
    string tmp_sdp_mid = json.substr(start, end - start);
    LOG(INFO) << "tmp_sdp_mid=" << tmp_sdp_mid;
    ice_candidate->set_sdp_mid(tmp_sdp_mid);
    
    prefix = "sdpMLineIndex\":\"";
    start = json.find(prefix);
    start += prefix.size();
    if (start == string::npos) {
      return false;
    }
    end = json.find("\"", start);
    if (end == string::npos) {
      return false;
    }
    string sdp_m_line_index_str = json.substr(start, end - start);
    LOG(INFO) << "number:" << sdp_m_line_index_str;
    ice_candidate->set_sdp_m_line_index(std::stoi(sdp_m_line_index_str));
    
    return true;
  }
}  // namespace orbit

