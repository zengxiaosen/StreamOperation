/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * stun_server_prober_client.cc
 * ---------------------------------------------------------------------------
 * Implements a test client program to test Stun server
 * ---------------------------------------------------------------------------
 *  Command line and usage:
 *  bazel-bin/stream_service/orbit/stun_server_prober_client --logtostderr
 * ---------------------------------------------------------------------------
 */

#include "gflags/gflags.h"
#include "glog/logging.h"

// For thread
#include <thread>
#include <sys/prctl.h>
// For chrono::seconds
#include <chrono> 

// For getTimeMS()
#include "stream_service/orbit/base/timeutil.h"
#include "stream_service/orbit/base/singleton.h"
#include "stream_service/orbit/stun_server_prober.h"

using namespace std;
using namespace orbit;

DEFINE_string(stun_server_ip, "test_relay.intviu.cn", "The stun server's ip");
DEFINE_int32(stun_server_port, 3478, "The stun server's port");
DEFINE_int32(threads_number, 1, "Define the threads number to test the stun server.");

namespace orangelab_test {
  void TestStunServer(int thread_id) {
    std::string public_ip; // The external/public ip of the server.
    long start = orbit::getTimeMS();
    StunServerProber* prober = Singleton<StunServerProber>::get();
    public_ip = prober->GetPublicIP(FLAGS_stun_server_ip,
                            FLAGS_stun_server_port);
    if (public_ip.empty()) {
      LOG(FATAL) << "[" << thread_id << "]" << "Error in finding public_ip....";
      return;
    }
    LOG(INFO) << "[" << thread_id << "]" << "Public_ip=" << public_ip;
    LOG(INFO) << "[" << thread_id << "]" << "Running takes "
              << orbit::getTimeMS() - start << " ms";
  }
}  // namespace orangelab

int main(int argc, char** argv) {
  google::InstallFailureSignalHandler();
  google::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);

  vector<std::thread*> threads;

  for (int i = 0; i < FLAGS_threads_number; ++i) {
    std::thread* t = new std::thread([i] () {
      orangelab_test::TestStunServer(i);
      });
    threads.push_back(t);
  }

  for (int i = 0; i < FLAGS_threads_number; ++i) {
    threads[i]->join();
    delete threads[i];
  }

  return 0;
}
