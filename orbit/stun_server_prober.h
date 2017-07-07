 /*
  * Copyright 2016 (C) Orange Labs Inc. All Rights Reserved.
  * Author: cheng@orangelab.cn (Cheng Xu)
  * 
  * StunServerProber is used to validate if the stun server works well.
  * - The other functions in this prober is to determine the external IP
  *   of the host server.
  *  It is a singleton class.
  * - Example usage:
  *------------------------------------------------------------------------
  *        StunServerProber* prober = Singleton<StunServerProber>::get();
  *        public_ip_ = prober->GetPublicIP(ice_config_.stunServer,
  *                                         ice_config_.stunPort);
  *------------------------------------------------------------------------
  */

#pragma once
#include "stream_service/orbit/base/singleton.h"
#include "glog/logging.h"

#include <mutex>
#include <thread>

namespace orbit {
 class StunServerProber {
 public:
   int ValidateStunServer(std::string stun_server_str, uint16_t stun_port);
   std::string GetPublicIP(std::string stun_server_str, uint16_t stun_port) {
    std::lock_guard<std::mutex> p_lock(stun_mutex_);
     if (!has_public_ip_) {
       if (ValidateStunServer(stun_server_str, stun_port) == -1) {
         LOG(ERROR) << "Validate the stun server failed. Can not get the public IP.";
         return "";
       }
       has_public_ip_ = true;
     }
     return public_ip_;
   }
 private:
   std::string public_ip_;
   bool has_public_ip_ = false;
   /* STUN server/port, if any */
   char *janus_stun_server_ = NULL;
   uint16_t janus_stun_port_ = 0;
   std::mutex stun_mutex_;

   DEFINE_AS_SINGLETON(StunServerProber);
 };

 // Resolve the hostname (DNS) address into a IP address
 namespace net {
   std::string ResolveAddress(const std::string& host_name);
 } // namespace net
}  // namespace orbit
