 /*
  * Copyright 2016 (C) Orange Labs Inc. All Rights Reserved.
  * Author: cheng@orangelab.cn (Cheng Xu)
  * 
  */

#include "machine_db.h"

#include "stream_service/orbit/production/machine_db.pb.h"
#include "stream_service/orbit/base/file.h"
#include "gflags/gflags.h"
#include "glog/logging.h"

DEFINE_string(machine_db_path, "stream_service/orbit/olivedata/production/machine_db.pb.txt", "The machine db file name and path.");

namespace orbit {
vector<string> MachineDb::ReverseAddressToMachine(const string& ip) {
  vector<string> ret;
  machine::MachineCollection machines;
  file::ReadFileToProto(FLAGS_machine_db_path, &machines);
  for (auto machine : machines.machines()) {
    string host_name = machine.hostname();
    string host_ip = machine.ip();
    if (host_ip == ip) {
      ret.push_back(host_name);
    }
  }
  return ret;
}
}  // namespace orbit
