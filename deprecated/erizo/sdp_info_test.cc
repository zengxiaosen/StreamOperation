/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * sdp_info_test.cc
 */

#include "gtest/gtest.h"
#include "glog/logging.h"

#include <sstream>
#include <fstream>

// Headers for SdpInfo.h tests
#include "stream_service/erizo/SdpInfo.h"
#include "stream_service/erizo/MediaDefinitions.h"

const std::string TEST_DIR = "./stream_service/erizo/testdata/";

std::string readFile(std::ifstream& in) {
  std::stringstream sstr;
  sstr << in.rdbuf();
  return sstr.str();
}

namespace erizo {
namespace {

class SdpInfoTest : public testing::Test {
 protected:
  virtual void SetUp() override {
  }

  virtual void TearDown() override {
  }
};

TEST_F(SdpInfoTest, ChromeSdp) {
  erizo::SdpInfo sdp;
  std::ifstream ifs(TEST_DIR + "Chrome.sdp", std::fstream::in);
  std::string sdpString = readFile(ifs);
  sdp.initWithSdp(sdpString, "video");

  //Check the mlines
  EXPECT_EQ(true, sdp.hasVideo);
  EXPECT_EQ(true, sdp.hasAudio);

  //Check the fingerprint
  EXPECT_EQ(0, sdp.fingerprint.compare("58:8B:E5:05:5C:0F:B6:38:28:F9:DC:24:00:8F:E2:A5:52:B6:92:E7:58:38:53:6B:01:1A:12:7F:EF:55:78:6E"));
  //Check ICE Credentials
  std::string username, password;
  sdp.getCredentials(username, password, erizo::VIDEO_TYPE);
EXPECT_EQ(0, username.compare("Bs0jL+c884dYG/oe"));
EXPECT_EQ(0, password.compare("ilq+r19kdvFsufkcyYAxoUM8"));
}

TEST_F(SdpInfoTest, FirefoxSdp) {
  erizo::SdpInfo sdp;
  std::ifstream ifs(TEST_DIR + "Firefox.sdp", std::fstream::in);
  std::string sdpString = readFile(ifs);
  sdp.initWithSdp(sdpString, "video");

  //Check the mlines
  EXPECT_EQ(true, sdp.hasVideo);
  EXPECT_EQ(true, sdp.hasAudio);

  //Check the fingerprint
  EXPECT_EQ(0, sdp.fingerprint.compare("C4:16:75:5C:5B:5F:E1:89:D7:EF:84:F7:40:B7:23:87:5F:A1:20:E0:F1:0F:89:B9:AB:87:62:17:80:E8:39:19"));

  //Check ICE Credentials
  std::string username, password;
  sdp.getCredentials(username, password, erizo::VIDEO_TYPE);
  EXPECT_EQ(0, username.compare("b1239219"));
  EXPECT_EQ(0, password.compare("b4ade8617fe94d5c800fdd085b86fd84"));
}

}  // annoymous namespace

}  // namespace erizo
