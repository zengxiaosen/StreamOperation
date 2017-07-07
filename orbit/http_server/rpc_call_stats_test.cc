// Copyright 2016 Orangelab Inc. All Rights Reserved.
// Author: cheng@orangelab.com (Cheng Xu)
//
// Unittest for rpc_call_stats.h/cc

#include "gtest/gtest.h"

#include "rpc_call_stats.h"

namespace orbit {
using namespace std;

namespace {

class RpcCallStatsTest : public testing::Test {
 protected:
  virtual void SetUp() override {
  }

  virtual void TearDown() override {
  }
};

class ExampleService {
public:
  int DoSomething(int n) {
    RpcCallStats stat("ExampleService_DoSomething");

    if (n > 1000) {
      LOG(ERROR) << n << " is bigger than 1000, error. return.";
      stat.Fail();
      return -1;
    }

    int sum = 0;
    for (int i = 0; i < n; ++i) {
      sum += i;
    }
    LOG(INFO) << sum;
    return sum;
  }
};

TEST_F(RpcCallStatsTest, TestCallCount) {
  ExampleService service;
  int sum = service.DoSomething(100);
  RpcCallStatsManager* stats_manager = Singleton<RpcCallStatsManager>::GetInstance();
  EXPECT_EQ(1, stats_manager->GetCallStat("ExampleService_DoSomething").total_call);
  long call_time = stats_manager->GetCallStat("ExampleService_DoSomething").last_call_latency_nano;
  EXPECT_GE(call_time, 0);

  sum = service.DoSomething(200);
  EXPECT_EQ(2, stats_manager->GetCallStat("ExampleService_DoSomething").total_call);
  long call_time2 = stats_manager->GetCallStat("ExampleService_DoSomething").last_call_latency_nano;
  EXPECT_GE(call_time2, 0);


  sum = service.DoSomething(1200);
  // It will fail.
  EXPECT_EQ(-1, sum);
  EXPECT_EQ(3, stats_manager->GetCallStat("ExampleService_DoSomething").total_call);
  EXPECT_EQ(1, stats_manager->GetCallStat("ExampleService_DoSomething").failed_call);
  long call_time3 = stats_manager->GetCallStat("ExampleService_DoSomething").last_call_latency_nano;
  // Since it failed, and it will not update the last_call_latency_nano
  EXPECT_EQ(call_time2, call_time3);

  //LOG(INFO) << stats_manager->GetCallStat("ExampleService_DoSomething").latencies->debugString();

  vector<string> all_methods = stats_manager->GetAllMethods();
  EXPECT_EQ(1, all_methods.size());
  EXPECT_EQ("ExampleService_DoSomething", all_methods[0]);
}


}  // namespace annoymous

}  // namespace orbit
