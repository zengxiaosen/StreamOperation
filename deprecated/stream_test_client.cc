#include "stream_service/stream_service.grpc.pb.h"

#include "gflags/gflags.h"
#include "glog/logging.h"

#include <grpc/grpc.h>
#include <grpc++/channel.h>
#include <grpc++/client_context.h>
#include <grpc++/create_channel.h>
#include <grpc++/security/credentials.h>
#include <grpc++/support/status.h>

#include <google/protobuf/text_format.h>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

DEFINE_int32(port, 10000, "Listening port of RPC service");

using olive::StreamService;

namespace examples {

static void RunClient() {
  LOG(INFO) << "Start the client.";
  LOG(INFO) << "Start the server yet?";

  std::shared_ptr<Channel>
    channel(grpc::CreateChannel("localhost:10000",
                                    grpc::InsecureCredentials()));
  std::unique_ptr<StreamService::Stub> stub(StreamService::NewStub(channel));

  olive::CreateSessionRequest request;
  olive::CreateSessionResponse response;
  ClientContext context;

  request.set_type(olive::CreateSessionRequest::VIDEO_MIXER_BRIDGE);
  Status status = stub->CreateSession(&context, request, &response);
  if (status.ok()) {
    std::string str;
    google::protobuf::TextFormat::PrintToString(response, &str);
    LOG(INFO) << "response=" << str;
  } else {
    LOG(INFO) << "RPC failed.";
  }
}

}  // namespace examples

int main(int argc, char** argv) {
  google::InstallFailureSignalHandler();
  google::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);
  grpc_init();
  examples::RunClient();

  grpc_shutdown();
  return 0;
}
