 /*
  * Copyright 2016 (C) Orange Labs Inc. All Rights Reserved.
  * Author: cheng@orangelab.cn (Cheng Xu)
  * 
  * - Example usage:
  *------------------------------------------------------------------------
  *        MachineDb* machine_db = Singleton<MachineDb>::get();
  *        vector<string> machines =
  *            machine_db.ReverseIpAddress("123.206.52.92");
  *------------------------------------------------------------------------
  */

#pragma once
#include "stream_service/orbit/base/singleton.h"
#include "glog/logging.h"

#include <string>
#include <vector>
namespace orbit {
using std::string;
using std::vector;

class MachineDb {
 public:
  vector<string> ReverseAddressToMachine(const string& ip);
 private:
   DEFINE_AS_SINGLETON(MachineDb);
};

}  // namespace orbit
