/*
 * Copyright 2016 All Rights Reserved.
 * Author: caixinyu@orangelab.cn (Cai Xinyu)
 *
 * grpc_stream_test.cc
 * ---------------------------------------------------------------------------
 * A test which call grpc for stream in a chaos way by multi thread.
 * ---------------------------------------------------------------------------
 */

#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <functional>

#include <unistd.h>

// For Gflags and Glog
#include "gflags/gflags.h"
#include "glog/logging.h"

#include "grpc_client.h"

using namespace std;
using namespace olive;
using namespace olive::client;



DEFINE_string(server_ip, "127.0.0.1", "IP of GRPC service.");
DEFINE_int32(server_port, 10000, "Port of GRPC service.");

#define NO_STREAM       (-1)

#define MAX_STREAM_NUM  (10000)
#define MIN_CLOSED_NUM  (500)

#define ICE_CANDIDATE     ("")
#define SDP_MID           ("")
#define SDP_M_LINE_INDEX  (0)

#define SDP_OFFER         ("")

#define STREAM_INFO_SET   ("")

static GrpcClient *grpc_client = nullptr;
static session_id_t session;

static vector<stream_id_t> good_streams;
static mutex mutex_good_streams;

static vector<stream_id_t> closed_streams;
static mutex mutex_closed_streams;

typedef vector<stream_id_t>::size_type vecn_t;

static void FunctionCreateStream() {
  {
    lock_guard<mutex> lock(mutex_good_streams);
    if (good_streams.size() > MAX_STREAM_NUM) {
      return;
    }
  }
  
  CreateStreamOption option;
  stream_id_t stream_id = grpc_client->CreateStream(session, option);
  LOG(INFO) << "CreateStream id = " << stream_id;
  {
    lock_guard<mutex> lock(mutex_good_streams);
    good_streams.push_back(stream_id);
  }
}

static void FunctionCloseRandGood() {
  stream_id_t stream_id = -1;
  {
    lock_guard<mutex> lock(mutex_good_streams);
    if (good_streams.empty()) {
      return;
    }
    vecn_t i = rand() % good_streams.size();
    stream_id = good_streams[i];
    good_streams.erase(good_streams.begin() + i);
  }
  if (stream_id == -1) {
    return;
  }
  {
    lock_guard<mutex> lock(mutex_closed_streams);
    closed_streams.push_back(stream_id);
  }
  grpc_client->CloseStream(session, stream_id);

  LOG(INFO) << "Close a good stream";
}

static stream_id_t RandGoodStreamId() {
  lock_guard<mutex> lock(mutex_good_streams);
  if (good_streams.empty()) {
    return NO_STREAM;
  }
  return good_streams[rand() % good_streams.size()];
}

static stream_id_t RandClosedStreamId() {
  lock_guard<mutex> lock(mutex_closed_streams);
  if (closed_streams.empty()) {
    return NO_STREAM;
  }
  return closed_streams[rand() % closed_streams.size()];
}

static void FunctionCloseRandClosed() {
  stream_id_t stream_id = RandClosedStreamId();
  if (stream_id == NO_STREAM) {
    return;
  }
  grpc_client->CloseStream(session, stream_id);
  LOG(INFO) << "Close a closed stream";
}

static void FunctionSendIceCandidateToGood() {
  stream_id_t stream_id = RandGoodStreamId();
  if (stream_id == NO_STREAM) {
    return;
  }
  IceCandidate ice_candidate;
  ice_candidate.set_ice_candidate(ICE_CANDIDATE);
  ice_candidate.set_sdp_mid(SDP_MID);
  ice_candidate.set_sdp_m_line_index(SDP_M_LINE_INDEX);
  grpc_client->SendIceCandidate(session, stream_id, ice_candidate);
  LOG(INFO) << "Send ice candidate to a good stream";
}

static void FunctionSendIceCandidateToClosed() {
  stream_id_t stream_id = RandClosedStreamId();
  if (stream_id == NO_STREAM) {
    return;
  }
  IceCandidate ice_candidate;
  ice_candidate.set_ice_candidate(ICE_CANDIDATE);
  ice_candidate.set_sdp_mid(SDP_MID);
  ice_candidate.set_sdp_m_line_index(SDP_M_LINE_INDEX);
  grpc_client->SendIceCandidate(session, stream_id, ice_candidate);
  
  LOG(INFO) << "Send ice candidate to a closed stream";
}

static void FunctionRecvIceCandidateFromGood() {
  stream_id_t stream_id = RandGoodStreamId();
  if (stream_id == NO_STREAM) {
    return;
  }
  vector<IceCandidate> ice_candidates;
  grpc_client->RecvIceCandidate(session, stream_id, &ice_candidates);

  LOG(INFO) << "Receive " << ice_candidates.size() << " ice candidate from good stream.";
  for (IceCandidate &ice_candidate : ice_candidates) {
    LOG(INFO) << "ICE from good:[" << ice_candidate.ice_candidate() << "]["
              << ice_candidate.sdp_mid() << "]["
              << ice_candidate.sdp_m_line_index() << "]";
  }
}

static void FunctionRecvIceCandidateFromClosed() {
  stream_id_t stream_id = RandClosedStreamId();
  if (stream_id == NO_STREAM) {
    return;
  }
  vector<IceCandidate> ice_candidates;
  grpc_client->RecvIceCandidate(session, stream_id, &ice_candidates);

  LOG(INFO) << "Receive " << ice_candidates.size() << " ice candidate from closed stream.";
  for (IceCandidate &ice_candidate : ice_candidates) {
    LOG(INFO) << "ICE from closed :[" << ice_candidate.ice_candidate() << "]["
              << ice_candidate.sdp_mid() << "]["
              << ice_candidate.sdp_m_line_index() << "]";
  }
}

static void FunctionSendSdpOfferToGood() {
  stream_id_t stream_id = RandGoodStreamId();
  if (stream_id == NO_STREAM) {
    return;
  }
  string sdp_offer = SDP_OFFER;
  string sdp_answer = grpc_client->SendSdpOffer(session, stream_id, sdp_offer);
  LOG(INFO) << "Send SDP Offer to good stream"
            << "\n\t    offer  is" << sdp_offer 
            << "\n\t    answer is" << sdp_answer;
}

static void FunctionSendSdpOfferToClosed() {
  stream_id_t stream_id = RandClosedStreamId();
  if (stream_id == NO_STREAM) {
    return;
  }
  string sdp_offer = SDP_OFFER;
  string sdp_answer = grpc_client->SendSdpOffer(session, stream_id, sdp_offer);
  
  LOG(INFO) << "Send SDP Offer to closed stream"
            << "\n\t    offer  is" << sdp_offer 
            << "\n\t    answer is" << sdp_answer;
}

static void FunctionRaiseHandOfGoodStream() {
  stream_id_t stream_id = RandGoodStreamId();
  if (stream_id == NO_STREAM) {
    return;
  }
  grpc_client->RaiseHand(session, stream_id);
  LOG(INFO) << "Good stream raise hand";
}

static void FunctionRaiseHandOfClosedStream() {
  stream_id_t stream_id = RandClosedStreamId();
  if (stream_id == NO_STREAM) {
    return;
  }
  grpc_client->RaiseHand(session, stream_id);
  LOG(INFO) << "Closed stream raise hand";
}

static bool RandBool() {
  return rand() % 2 == 1; 
}

static void FunctionMuteGoodStream() {
  stream_id_t stream_id = RandGoodStreamId();
  if (stream_id == NO_STREAM) {
    return;
  }
  vector<stream_id_t> streams = {stream_id};
  grpc_client->Mute(session, RandBool(), streams);
  LOG(INFO) << "Good stream mute";
}

static void FunctionMuteClosedStream() {
  stream_id_t stream_id = RandClosedStreamId();
  if (stream_id == NO_STREAM) {
    return;
  }
  vector<stream_id_t> streams = {stream_id};
  grpc_client->Mute(session, RandBool(), streams);
  LOG(INFO) << "Closed stream mute";
}

static void FunctionSetStreamInfoToGoodStream() {
  stream_id_t stream_id = RandGoodStreamId();
  if (stream_id == NO_STREAM) {
    return;
  }
  string stream_info = STREAM_INFO_SET;
  grpc_client->SetStreamInfo(session, stream_id, stream_info);
  LOG(INFO) << "Set stream info to good stream";
}

static void FunctionSetStreamInfoToClosedStream() {
  stream_id_t stream_id = RandClosedStreamId();
  if (stream_id == NO_STREAM) {
    return;
  }
  string stream_info = STREAM_INFO_SET;
  grpc_client->SetStreamInfo(session, stream_id, stream_info);
  LOG(INFO) << "Set stream info to closed stream";
}

static void FunctionSetGoodStreamAsPublisher() {
  stream_id_t stream_id = RandGoodStreamId();
  if (stream_id == NO_STREAM) {
    return;
  }
  grpc_client->SetPublisher(session, stream_id);
  LOG(INFO) << "Set good stream as publisher";
}

static void FunctionSetCloseStreamAsPublisher() {
  stream_id_t stream_id = RandClosedStreamId();
  if (stream_id == NO_STREAM) {
    return;
  }
  grpc_client->SetPublisher(session, stream_id);
  LOG(INFO) << "Set closed stream as publisher";
}

static void ThreadBodyRandRemoveClosed() {
  while (false) {
    usleep(1000);
    {
      lock_guard<mutex> lock(mutex_closed_streams);
      if (closed_streams.size() > MIN_CLOSED_NUM) {
        vecn_t i = rand() % closed_streams.size();
        closed_streams.erase(closed_streams.begin() + i);
      }
    }
  }
}

static vector<function<void(void)>> funcs = {
  [](){ FunctionCreateStream(); },
  [](){ FunctionCloseRandGood(); },
  [](){ FunctionCloseRandClosed(); },
  [](){ FunctionSendIceCandidateToGood(); },
  [](){ FunctionSendIceCandidateToClosed(); },
  [](){ FunctionRecvIceCandidateFromGood(); },
  [](){ FunctionRecvIceCandidateFromClosed(); },
  [](){ FunctionSendSdpOfferToGood(); },
  [](){ FunctionSendSdpOfferToClosed(); },
  [](){ FunctionRaiseHandOfGoodStream(); },
  [](){ FunctionRaiseHandOfClosedStream(); },
  [](){ FunctionMuteGoodStream(); },
  [](){ FunctionMuteClosedStream(); },
  [](){ FunctionSetStreamInfoToGoodStream(); },
  [](){ FunctionSetStreamInfoToClosedStream(); },
  [](){ FunctionSetGoodStreamAsPublisher(); },
  [](){ FunctionSetCloseStreamAsPublisher(); },
};

static function<void(void)> RandFunction() {
  size_t n = rand() % funcs.size();
  return funcs[n];
}

static void SleepRandTime() {
  usleep(20 + rand() % 100);
}

static void ThreadBodyRandExecuteFunction() {
  while (true) {
    RandFunction()();
    SleepRandTime();
  }
}

int main(int argc, char *argv[]) {
  google::InstallFailureSignalHandler();
  google::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);

  grpc_client = new GrpcClient(FLAGS_server_ip, FLAGS_server_port);
  session = grpc_client->CreateSession(CreateSessionRequest::VIDEO_BRIDGE, "");
  srand(time(NULL));
 

  thread(ThreadBodyRandRemoveClosed).detach();

  for (int i = 0; i < 4; ++i) {
    thread(ThreadBodyRandExecuteFunction).detach();
  }

  while (true) {
    pause();
  }

  return 0;
}




