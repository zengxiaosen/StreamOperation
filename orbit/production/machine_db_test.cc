/*
 * Copyright 2016 (C) Orangelab. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * machine_db_test.cc
 */

#include "gtest/gtest.h"
#include "glog/logging.h"
#include "gflags/gflags.h"

#include <sstream>
#include <fstream>
#include "stream_service/orbit/base/singleton.h"

#include "machine_db.h"

DECLARE_string(machine_db_path);

namespace orbit {
namespace {

class MachineDbTest : public testing::Test {
 protected:
  virtual void SetUp() override {
    FLAGS_machine_db_path = "stream_service/orbit/olivedata/production/machine_db.pb.txt";
  }

  virtual void TearDown() override {
  }
};

TEST_F(MachineDbTest, ReverseAddressToMachine) {
  MachineDb* machine_db = Singleton<MachineDb>::get();
  {
    vector<string> hosts = machine_db->ReverseAddressToMachine("123.206.52.92");
    EXPECT_EQ(1, hosts.size());
    EXPECT_EQ("t1.intviu.cn", hosts[0]);
  }
  {
    vector<string> hosts = machine_db->ReverseAddressToMachine("12.34.56.78");
    EXPECT_EQ(0, hosts.size());
  }  
}

}
}  // namespace orbit
