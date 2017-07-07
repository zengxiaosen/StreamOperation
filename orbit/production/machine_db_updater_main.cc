/*
 * Copyright 2016 (C) Orangelab. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * machine_db_updater_main.cc
 * ---------------------------------------------------------------------------
 * Update the machine db file.
 * ---------------------------------------------------------------------------
 */

#include "stream_service/orbit/production/machine_db.pb.h"
#include "stream_service/orbit/base/file.h"
#include "stream_service/orbit/stun_server_prober.h"
#include "machine_db.h"

#include "gflags/gflags.h"
#include "glog/logging.h"

DEFINE_string(mode, "update", "Could be create or update, default is update.");
DECLARE_string(machine_db_path);

namespace orbit {
  void CreateMachineDb() {
    const string machine_db_file = FLAGS_machine_db_path;
    machine::MachineCollection machines;
    {
      machine::Machine* machine = machines.add_machines();
      machine->set_hostname("t1.intviu.cn");
      machine->set_ip("123.206.52.92");
    }
    {
      machine::Machine* machine = machines.add_machines();
      machine->set_hostname("t2.intviu.cn");
      machine->set_ip("");
    }
    assert(file::WriteProtoToASCIIFile(machines, machine_db_file));
  }
  void UpdateMachineDb() {
    const string machine_db_file = FLAGS_machine_db_path;
    machine::MachineCollection machines;
    file::ReadFileToProto(machine_db_file, &machines);
    for (int i = 0; i < machines.machines_size(); ++i) {
      machine::Machine* machine = machines.mutable_machines(i);
      LOG(INFO) << "machine - " << machine->hostname() << " - " << machine->ip();
      string ip = net::ResolveAddress(machine->hostname());
      LOG(INFO) << "resolve to ip:" << ip;
      machine->set_ip(ip);
    }
    LOG(INFO) << "proto=" << machines.DebugString();
    // Write back to the machine db_file.
    assert(file::WriteProtoToASCIIFile(machines, machine_db_file));
  }
}  // namespace orbit

int main(int argc, char** argv) {
  google::InstallFailureSignalHandler();
  google::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);

  if (FLAGS_mode == "create") {
    orbit::CreateMachineDb();
    LOG(INFO) << "Create machine_db successfully.";
  } else if (FLAGS_mode == "update") {
    orbit::UpdateMachineDb();
    LOG(INFO) << "Update machine_db successfully.";
  }
  return 0;
}
