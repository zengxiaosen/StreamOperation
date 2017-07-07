/*
 * transport_delegate_test.cc
 *
 *  Created on: May 17, 2016
 *      Author: chenteng
 */

#include "gtest/gtest.h"
#include "gflags/gflags.h"
#include "transport_delegate.h"
#include "nice_connection.h"

DEFINE_bool(enable_red_fec, true, "EnablR Video RED/FEC codecs");
DEFINE_bool(only_public_ip, false, "use public ip only on candidate.");
DEFINE_string(packet_capture_filename, "", "If any filename is specified, "
              "the rtp packet capture will be enabled and stored into the file."
              " e.g. video_rtp.pb");

namespace orbit {
//namespace {

orbit::IceConfig ice_config;

class TransportDelegateTest : public testing::Test {
 protected:

  virtual void SetUp() override {
  }

  virtual void TearDown() override {
  }
};

TEST_F(TransportDelegateTest, TransportDelegateDefaults) {
  orbit::TransportDelegate transport_delegate(true,true,true,ice_config,NULL);
//  EXPECT_EQ(0, queue.getSize());
//  EXPECT_EQ(false, queue.hasData());
}


//}
} /* namespace orbit */
