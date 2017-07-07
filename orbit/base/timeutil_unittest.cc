/*
 * Copyright © 2016 Orangelab Inc. All Rights Reserved.
 * 
 * Author: qingyong@orangelab.cn (Qingyong Zhang)
 * Created on: Jun 16, 2016
 *
 *
 * common_utils_test.cc
 * --------------------------------------------------------
 *
 * --------------------------------------------------------
 */

#include "gtest/gtest.h"
#include "glog/logging.h"

#include "timeutil.h"

namespace orbit {
namespace {
  class TimeUtilsTest : public testing::Test {
   protected:
    virtual void SetUp() override {
    }
    virtual void TearDown() override {
    }
  };

  TEST_F(TimeUtilsTest, TimeTest) {
    long time = GetCurrentTime_MS();
    usleep(10000);
    long second = GetCurrentTime_MS();
    EXPECT_EQ(true, (second-time) >= 10);
    EXPECT_EQ(true, (second-time) <= 12);
    //    LOG(INFO)<< "Current time is "<< time;

    time = GetCurrentTime_MS();
    usleep(20000);
    second = GetCurrentTime_MS();
    EXPECT_EQ(true, (second-time) >= 20);
    EXPECT_EQ(true, (second-time) <= 22);
  }

}  // annoymous namespace
}  // namespace orbit
