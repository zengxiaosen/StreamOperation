/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * sdp_info_test.cc
 */

#include "gtest/gtest.h"
#include "glog/logging.h"
#include "gflags/gflags.h"

#include <sstream>
#include <fstream>

// Headers for SdpInfo.h tests
#include "stream_service/orbit/sdp_info.h"
//#include "stream_service/orbit/media_definitions.h"
DEFINE_bool(enable_red_fec, true, "EnablR Video RED/FEC codecs");

const std::string TEST_DIR = "./stream_service/orbit/testdata/";

std::string readFile(std::ifstream& in) {
  std::stringstream sstr;
  sstr << in.rdbuf();
  return sstr.str();
}

namespace orbit {
namespace {

class SdpInfoTest : public testing::Test {
 protected:
  virtual void SetUp() override {
  }

  virtual void TearDown() override {
  }
};

TEST_F(SdpInfoTest, ChromeSdp) {
  orbit::SdpInfo sdp;
  std::ifstream ifs(TEST_DIR + "Chrome.sdp", std::fstream::in);
  std::string sdpString = readFile(ifs);
  sdp.initWithSdp(sdpString, "video");

  //Check the mlines
  EXPECT_EQ(true, sdp.getHasVideo());
  EXPECT_EQ(true, sdp.getHasAudio());

  //Check the fingerprint
  EXPECT_EQ(0, sdp.getFingerprint().compare("58:8B:E5:05:5C:0F:B6:38:28:F9:DC:24:00:8F:E2:A5:52:B6:92:E7:58:38:53:6B:01:1A:12:7F:EF:55:78:6E"));
  //Check ICE Credentials
  std::string username, password;
  sdp.getCredentials(username, password, orbit::VIDEO_TYPE);
  EXPECT_EQ(0, username.compare("Bs0jL+c884dYG/oe"));
  EXPECT_EQ(0, password.compare("ilq+r19kdvFsufkcyYAxoUM8"));
}

TEST_F(SdpInfoTest, ChromeHasSSRCButFirefoxDoesNotHave) {
  {
    orbit::SdpInfo sdp;
    std::ifstream ifs(TEST_DIR + "Chrome.sdp", std::fstream::in);
    std::string sdpString = readFile(ifs);
    sdp.initWithSdp(sdpString, "video");
    EXPECT_EQ(1640977436, sdp.getVideoSsrc());
  }
  {
    orbit::SdpInfo sdp;
    std::ifstream ifs(TEST_DIR + "Firefox.sdp", std::fstream::in);
    std::string sdpString = readFile(ifs);
    sdp.initWithSdp(sdpString, "video");
    EXPECT_EQ(0, sdp.getVideoSsrc());
  }
}

TEST_F(SdpInfoTest, CreateOfferSdp) {
  {
    orbit::SdpInfo sdp;
    sdp.createOfferSdp();
    sdp.setIsFingerprint(true);
    sdp.setFingerprint("fingerprint:sha-256 58:8B:E5:05:5C:0F:B6:38:28:F9:DC:24:00:8F:E2:A5:52:B6:92:E7:58:38:53:6B:01:1A:12:7F:EF:55:78:6E");
    std::string username = "abc";
    std::string password = "123";
    sdp.setCredentials(username, password, AUDIO_TYPE);
    sdp.setCredentials(username, password, VIDEO_TYPE);
    LOG(INFO) << sdp.getSdp();

    {
      orbit::SdpInfo remote_sdp;
      remote_sdp.initWithSdp(sdp.getSdp(), "");
      EXPECT_EQ(44444, remote_sdp.getAudioSsrc());    
      std::string username, password;
      remote_sdp.getCredentials(username, password, orbit::VIDEO_TYPE);
      EXPECT_EQ(0, username.compare("abc"));
      EXPECT_EQ(0, password.compare("123"));
    }
  }
}

TEST_F(SdpInfoTest, FirefoxSdp) {
  orbit::SdpInfo sdp;
  std::ifstream ifs(TEST_DIR + "Firefox.sdp", std::fstream::in);
  std::string sdpString = readFile(ifs);
  sdp.initWithSdp(sdpString, "video");

  //Check the mlines
  EXPECT_EQ(true, sdp.getHasVideo());
  EXPECT_EQ(true, sdp.getHasAudio());

  //Check the fingerprint
  EXPECT_EQ(0, sdp.getFingerprint().compare("C4:16:75:5C:5B:5F:E1:89:D7:EF:84:F7:40:B7:23:87:5F:A1:20:E0:F1:0F:89:B9:AB:87:62:17:80:E8:39:19"));

  //Check ICE Credentials
  std::string username, password;
  sdp.getCredentials(username, password, orbit::VIDEO_TYPE);
  EXPECT_EQ(0, username.compare("b1239219"));
  EXPECT_EQ(0, password.compare("b4ade8617fe94d5c800fdd085b86fd84"));
}

TEST_F(SdpInfoTest, ParseIceCandidate) {
  orbit::SdpInfo sdp;
  std::string sdpString = "a=candidate:3141053312 1 udp 2122260223 10.171.94.20 62087 typ host generation 0 ufrag RtlwCUXUsSNLoX6/ network-id 1";
  std::string username, password;
  username = "RtlwCUXUsSNLoX6/";
  sdp.setCredentials(username, password, OTHER);
  bool ret = sdp.initWithSdp(sdpString, "audio");
  EXPECT_EQ(true, ret);
  for (uint8_t it = 0; it < sdp.getCandidateInfos().size(); it++){
    CandidateInfo cand_info = sdp.getCandidateInfos()[it];
    LOG(INFO) << cand_info.ToDebugString();
  }
  EXPECT_EQ(1, sdp.getCandidateInfos().size());
  EXPECT_EQ("a=candidate:3141053312 1 udp 2122260223 10.171.94.20 62087 typ host",
            sdp.getCandidateInfos()[0].ToDebugString());

}



}  // annoymous namespace

}  // namespace orbit
