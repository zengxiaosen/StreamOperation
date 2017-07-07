/*
 * Copyright 2016 All Rights Reserved.
 * Author: caixinyu@orangelab.cn (Cai Xinyu)
 *
 * grpc_client_test.cc
 * -------------------------------------------------------------------
 * A test which create and close lots of session by multi thread way.
 * -------------------------------------------------------------------
 */

#include <stdint.h>
#include <time.h>
#include <errno.h>

#include <string>
#include <sstream>
#include <iostream>
#include <chrono> 
#include <thread>
#include <vector>

#include <unistd.h>

#include "gflags/gflags.h"
#include "glog/logging.h"

#include "grpc_client.h"

DEFINE_string(test_case, "test_create", "Test cases for this client.");
DEFINE_int32(session_id, 0, "The session_id of the room you want to entry.");
DEFINE_int32(stream_id, 0, "The stream_id of the stream you want to check.");

using namespace std;
using namespace olive;
using namespace olive::client;

#define GRPC_SERVER_IP    "localhost"
#define GRPC_SERVER_PORT  10000

static uint64_t GetTimeMS() {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return ts.tv_sec * 1000 + ts.tv_nsec / 1000;
}

static void SleepMS(uint64_t ms) {
  struct timespec ts, rem;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (ms % 1000) * 1000;
  int n;
  again: n = nanosleep(&ts, &rem);
  if (n == -1 && errno == EINTR) {
    ts = rem;
    goto again;
  }
}

static inline uint64_t SpendMS(uint64_t start, uint64_t end) {
  return start < end ? end - start : 0;
}

static inline string ToStr(uint64_t n) {
  ostringstream ss;
  ss << n;
  return ss.str();
}

#define DEF_SPEED(name) \
  uint64_t min_##name##_speed = UINT64_MAX; \
  uint64_t max_##name##_speed = 0;

#define CHECK_SPEED(name) \
  do { \
    if (name##_speed > max_##name##_speed) { \
      max_##name##_speed = name##_speed; \
    } \
    if (name##_speed < min_##name##_speed) { \
      min_##name##_speed = name##_speed; \
    }\
  } while (false)

#define STR(n) (ToStr(n).c_str())

static void TestCreateSessionSpeed(const std::string &tag) {
  std::string tg = tag;
  GrpcClient client(GRPC_SERVER_IP, GRPC_SERVER_PORT);
  SleepMS(100);

  DEF_SPEED(create_session);
  DEF_SPEED(close_session);

  for (int i = 0; true; ++i) {
    uint64_t start = GetTimeMS();
    session_id_t session_id = client.CreateSession(CreateSessionRequest::VIDEO_BRIDGE, "");
    uint64_t create_session_speed = SpendMS(start, GetTimeMS());
    CHECK_SPEED(create_session);
    
    vector<stream_id_t> streams;
    for (int j = 0; j < 100; ++j) {
      CreateStreamOption option;
      streams.push_back(client.CreateStream(session_id, option));
    }

    for (stream_id_t stream_id : streams) {
      client.CloseStream(session_id, stream_id);
    }

    start = GetTimeMS();
    client.CloseSession(session_id);
    uint64_t close_session_speed = SpendMS(start, GetTimeMS());
    CHECK_SPEED(close_session);

    LOG(INFO) << tg << ": create use " << create_session_speed
              << "s , close use " << close_session_speed << "s.";
  }
  LOG(INFO) << tg << ":min create: " << min_create_session_speed;
  LOG(INFO) << tg << ":max create: " << max_create_session_speed;
  LOG(INFO) << tg << ":min close : " << min_close_session_speed;
  LOG(INFO) << tg << ":max close : " << max_close_session_speed;
}

int main(int argc, char *argv[]) {
  google::InstallFailureSignalHandler();
  google::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);

  if (FLAGS_test_case == "test_create") {
    vector<thread> threads;
    for (int i = 0; i < 8; ++i) {
      ostringstream ss;
      ss << "T" << i;
      threads.push_back(thread([&ss]() {TestCreateSessionSpeed(ss.str());}));
    }
    
    for (thread &t : threads) {
      t.join();
    }
  } else if (FLAGS_test_case == "query_net") {
    session_id_t session_id;
    stream_id_t stream_id;
    GrpcClient client(GRPC_SERVER_IP, GRPC_SERVER_PORT);
    if (FLAGS_session_id == 0) {
      session_id = client.CreateSession(CreateSessionRequest::VIDEO_BRIDGE, "");
      CreateStreamOption option;
      stream_id = client.CreateStream(session_id, option);
    } else if (FLAGS_session_id > 0 && FLAGS_stream_id > 0) {
      session_id = FLAGS_session_id;
      stream_id = FLAGS_stream_id;
    } else {
      return 0;
    }
    
    LOG(INFO) << "session_id=" << session_id
              << " stream_id=" << stream_id;

    client.QueryProbeNetTestResult(session_id, stream_id);
  }

  return 0;
}

