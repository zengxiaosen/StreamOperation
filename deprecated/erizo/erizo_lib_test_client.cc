/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * A Simple test client for erizo_lib
 * Test the WebRtcConnection class.
 */

#include "gflags/gflags.h"
#include "glog/logging.h"

#include <sstream>
#include <fstream>

#include "SdpInfo.h"
#include "WebRtcConnection.h"
#include "NiceConnection.h"

using namespace std;

namespace erizo {
class ConnectionListener : public WebRtcConnectionEventListener {
public:
 void notifyEvent(WebRTCEvent newEvent, const std::string& message) override {
LOG(INFO) << "event triggered...";
}

};

void testSetupWebRtcConnection() {
    //WebRtcConnection(bool audioEnabled, bool videoEnabled, const IceConfig& iceConfig,bool trickleEnabled, WebRtcConnectionEventListener* listener);
bool audioEnabled = true;
bool videoEnabled = false;
IceConfig ice_config;
bool trickleEnabled = true;
ConnectionListener listener;

WebRtcConnection *conn = new WebRtcConnection(audioEnabled, videoEnabled, ice_config, trickleEnabled, &listener);
	conn->init();

conn->createOffer();
string local_sdp = conn->getLocalSdp();

LOG(INFO) << local_sdp;
conn->close();
//delete conn;
}

}

int main(int argc, char** argv) {
  google::InstallFailureSignalHandler();
  google::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);

erizo::testSetupWebRtcConnection();
  return 0;
}
