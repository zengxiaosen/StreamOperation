#include <grpc/grpc.h>
#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>

#include "gflags/gflags.h"
#include "glog/logging.h"

#include <google/protobuf/text_format.h>

#include "stream_service_impl.h"
#include "gst_util.h"

DEFINE_int32(port, 10000, "Listening port of RPC service");
DEFINE_string(stun_server_ip, "101.200.238.173", "The stun server's ip");
DEFINE_string(public_ip, "", "stream server's public ip");

namespace olive {

static void SetupServer() {
  std::stringstream server_address;
  server_address << "0.0.0.0:" << FLAGS_port;
  StreamServiceImpl service;

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
  olive::InitGstreamer(argc, argv);

  google::InstallFailureSignalHandler();
  google::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);
  grpc_init();

  olive::SetupServer();

  grpc_shutdown();
  return 0;
}
