/*
 * Copyright 2016 All Rights Reserved.
 * Author: chenteng@orangelab.cn (Chen teng)
 *
 * nack_processor_test.cc
 */

#include "gtest/gtest.h"
#include "nack_processor.h"

namespace orbit {
namespace {

class NackProcessorTest : public testing::Test {
 protected:
  virtual void SetUp() override {
  }

  virtual void TearDown() override {
  }
};

TEST_F(NackProcessorTest, correctNackProcessorDefaults) {
  orbit::NackProcessor nack_processor;
  unsigned int list_size = nack_processor.GetRetransmitListSize();
  EXPECT_EQ(0, list_size);
}

}  // anonymous namespace
}  // namespace orbit
