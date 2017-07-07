/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * erizo_stream_server.cc
 * ---------------------------------------------------------------------------
 * Implements the stream server based on erizo lib.
 * ---------------------------------------------------------------------------
 */

#include <grpc/grpc.h>
#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>

#include "gflags/gflags.h"
#include "glog/logging.h"

#include <google/protobuf/text_format.h>

#include "erizo_stream_service_impl.h"

#include <log4cxx/logger.h>
#include "log4cxx/basicconfigurator.h"

DEFINE_int32(port, 10000, "Listening port of RPC service");
DEFINE_string(stun_server_ip, "101.200.238.173", "The stun server's ip");
DEFINE_bool(use_log4cxx, false, "The flag to turn on/off log4cxx logger.");

using namespace log4cxx;

namespace olive {

static void SetupServer() {
  std::stringstream server_address;
  server_address << "0.0.0.0:" << FLAGS_port;
  ErizoStreamServiceImpl service;

  LOG(INFO) << "Server listening on " << server_address.str();
  grpc::ServerBuilder builder;
  builder.AddListeningPort(server_address.str(),
                           grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  server->Wait();
}

}  // namespace olive

int main(int argc, char** argv) {
  google::InstallFailureSignalHandler();
  google::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);
  grpc_init();

  if (FLAGS_use_log4cxx) {
    BasicConfigurator::configure();
  }
  olive::SetupServer();

  grpc_shutdown();
  return 0;
}
