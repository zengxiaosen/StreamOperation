/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * registry_test_client.cc
 * ---------------------------------------------------------------------------
 * Implements the test client for registry service.
 * ---------------------------------------------------------------------------
 */

// For Gflags and Glog
#include "gflags/gflags.h"
#include "glog/logging.h"

// For slave mode.
#include "stream_service/proto/registry.grpc.pb.h"

#include <grpc/grpc.h>

// For gRpc client
#include <grpc++/channel.h>
#include <grpc++/client_context.h>
#include <grpc++/create_channel.h>
#include <grpc++/security/credentials.h>
#include <grpc++/support/status.h>

// For thread
#include <thread>
#include <sys/prctl.h>
// For chrono::seconds
#include <chrono> 

#include "stream_service/orbit/base/timeutil.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using namespace std;
using orbit::registry::RegistryService;

DEFINE_string(master_server, "localhost:12350", "The master server ip and port");
DEFINE_int32(port, 10000, "Listening port of RPC service");
DEFINE_string(test_case, "register", "Specify the test case.");

namespace olive {
std::unique_ptr<RegistryService::Stub> stub;
void RegisterSlaveClient(string server) {
  if (stub.get() == NULL) {
    std::shared_ptr<Channel>
      channel(grpc::CreateChannel(server,
                                  grpc::InsecureChannelCredentials()));
    stub = RegistryService::NewStub(channel);
  }
  orbit::registry::RegisterRequest request;
  request.mutable_server()->set_name("OrbitStreamService");
  request.mutable_server()->set_host("127.0.0.1");
  request.mutable_server()->set_port(FLAGS_port);
  int start_time = orbit::getTimeMS();

  ClientContext context;
  //std::chrono::system_clock::time_point deadline =
  //  std::chrono::system_clock::now() + std::chrono::seconds(1);
  //context.set_deadline(deadline);
  orbit::registry::RegisterResponse response;
  LOG(INFO) << request.DebugString();
  LOG(INFO) << "Call stub->Register";
  Status status = stub->Register(&context, request, &response);
  LOG(INFO) << response.DebugString();
  if (!status.ok()) {
    LOG(INFO) << "Rpc failed. errmsg=" << status.error_message();
  }
  int end_time = orbit::getTimeMS();
  LOG(INFO) << "elapsed time(ms)=" << end_time - start_time;
}

void GetServers(string server) {
  std::shared_ptr<Channel>
    channel(grpc::CreateChannel(server,
                                grpc::InsecureChannelCredentials()));
  std::unique_ptr<RegistryService::Stub> stub(RegistryService::NewStub(channel));
  orbit::registry::GetServersRequest request;
  ClientContext context;
  std::chrono::system_clock::time_point deadline =
    std::chrono::system_clock::now() + std::chrono::seconds(1);
  context.set_deadline(deadline);
  orbit::registry::GetServersResponse response;
  LOG(INFO) << request.DebugString();
  LOG(INFO) << "Call stub->GetServers";
  Status status = stub->GetServers(&context, request, &response);
  LOG(INFO) << response.DebugString();
  if (!status.ok()) {
    LOG(INFO) << "Rpc failed. errmsg=" << status.error_message();
  }
}

}  // namespace olive

int main(int argc, char** argv) {
  google::InstallFailureSignalHandler();
  google::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);
  grpc_init();

  std::string server = FLAGS_master_server;  // "localhost:10000"
  LOG(INFO) << "Start to connect to master server:'" << server << "'.";

  if (FLAGS_test_case == "register") {
    bool running_ = true;
    
    while(running_) {
      olive::RegisterSlaveClient(server);
      //std::this_thread::sleep_for(std::chrono::microseconds(3000));
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }  else {
    olive::GetServers(server);
  }

  grpc_shutdown();
  return 0;
}
