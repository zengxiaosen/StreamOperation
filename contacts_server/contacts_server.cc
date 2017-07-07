/*
 * Copyright 2016 All Rights Reserved.
 * Author: stephen@orangelab.cn (Stephen Lee)
 *         cheng@orangelab.cn (Cheng Xu)
 *
 * contacts_server.cc
 * ---------------------------------------------------------------------------
 * Implements the server's main() program.
 * ---------------------------------------------------------------------------
 */

// For gRpc
#include <grpc/grpc.h>
#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>

// For gRpc client
#include <grpc++/channel.h>
#include <grpc++/client_context.h>
#include <grpc++/create_channel.h>
#include <grpc++/security/credentials.h>
#include <grpc++/support/status.h>

// For Gflags and Glog
#include "gflags/gflags.h"
#include "glog/logging.h"
// Contacts service implementation
#include "contacts_service_impl.h"
// For thread
#include <thread>
#include <sys/prctl.h>
// For chrono::seconds
#include <chrono> 

// For http server and handlers
#include "stream_service/orbit/http_server/http_server.h"
#include "stream_service/orbit/http_server/http_handler.h"

// Contacts client.
#include "stream_service/contacts_server/contacts_service.grpc.pb.h"

#include "stream_service/third_party/json2pb/json2pb.h"

DEFINE_int32(port, 20011, "Listening port of RPC service");
DEFINE_int32(http_port, 20012, "Port to listen on with HTTP protocol");
//DEFINE_bool(print_json, true, "Print in JSON format");

namespace orbit {

using orbit::HttpServer;
using namespace orangelab;
using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

class ContactHandler : public HttpHandler {
 public:
  ContactHandler() {
    std::shared_ptr<Channel>
      channel(grpc::CreateChannel("localhost:20011",
                                  grpc::InsecureCredentials()));
    stub_ = ContactsService::NewStub(channel);
  }

  string DoCreateNewUser(const string& json) {
    orangelab::CreateNewUserRequest request;
    orangelab::CreateNewUserResponse response;
    // Convert the json into protocol buffer.
    string body;
    try {
      jsonpb::json2pb(request, json.c_str(), json.size());
    } catch (std::exception e) {
      body = "pb2json error.";
    }
    if (body.empty()) {
      ClientContext context;
      std::chrono::system_clock::time_point deadline =
        std::chrono::system_clock::now() + std::chrono::seconds(1);
      context.set_deadline(deadline);

      Status status = stub_->CreateNewUser(&context, request, &response);

      if (status.ok()) {
        LOG(INFO) << "response=" << response.DebugString();
        body = jsonpb::pb2json(response);
      } else {
        body = "RPC failed.";
        LOG(INFO) << "RPC failed.";
      }
    }
    return body;
  }

  string DoGetUserContacts(const string& json) {
    orangelab::GetUserContactsRequest request;
    orangelab::GetUserContactsResponse response;
    // Convert the json into protocol buffer.
    string body;
    try {
      jsonpb::json2pb(request, json.c_str(), json.size());
    } catch (std::exception e) {
      body = "pb2json error.";
    }
    if (body.empty()) {
      ClientContext context;
      std::chrono::system_clock::time_point deadline =
        std::chrono::system_clock::now() + std::chrono::seconds(1);
      context.set_deadline(deadline);

      Status status = stub_->GetUserContacts(&context, request, &response);
      if (status.ok()) {
        LOG(INFO) << "response=" << response.DebugString();
        try {
          body = jsonpb::pb2json(response);
        } catch (std::exception e) {
          body = "pb2json error.";
        }
      } else {
        body = "RPC failed.";
        LOG(INFO) << "RPC failed.";
      }
    }
    return body;
  }

  string DoUploadContacts(const string& json) {
    orangelab::UploadContactsRequest request;
    orangelab::UploadContactsResponse response;
    // Convert the json into protocol buffer.
    string body;
    try {
      jsonpb::json2pb(request, json.c_str(), json.size());
    } catch (std::exception e) {
      body = "pb2json error.";
    }
    if (body.empty()) {
      ClientContext context;
      std::chrono::system_clock::time_point deadline =
        std::chrono::system_clock::now() + std::chrono::seconds(1);
      context.set_deadline(deadline);

      Status status = stub_->UploadContacts(&context, request, &response);

      if (status.ok()) {
        LOG(INFO) << "response=" << response.DebugString();
        try {
          body = jsonpb::pb2json(response);
        } catch (std::exception e) {
          body = "pb2json error.";
        }
      } else {
        body = "RPC failed.";
        LOG(INFO) << "RPC failed.";
      }
    }
    return body;
  }

  virtual std::shared_ptr<HttpResponse> HandleRequest(const HttpRequest& http_request) {
    std::shared_ptr<HttpResponse> http_response(new HttpResponse());
    http_response->set_code(HTTP_OK);

    string method = "";
    string message = "";
    string value = "";
    if (http_request.GetQueryValue("method", &value)) {
      method = value;
    }
    value = "";
    if (http_request.GetQueryValue("message", &value)) {
      message = value;
    }

    string body;
    if (method == "CreateNewUser") {
      body = DoCreateNewUser(message);
    } else if (method == "GetUserContacts") {
      body = DoGetUserContacts(message);
    } else if (method == "UploadContacts") {
      body = DoUploadContacts(message);
    } else {
      body = "No such method in the service.";
      LOG(ERROR) << body;
    }

    http_response->set_content(body);
    http_response->set_content_type("text/json");
    return http_response;
  }
private:
  std::unique_ptr<ContactsService::Stub> stub_;
};

static HttpServer* SetupHttpServer() {
  HttpServer* server = new HttpServer(FLAGS_http_port, orbit::HttpServer::TYPE_HTTP);
  server->SetThreadPool(sysconf(_SC_NPROCESSORS_ONLN));
  server->RegisterHandler("/contact", new orbit::ContactHandler);
  server->RegisterHandler("/file", new orbit::SimpleFileHandler("stream_service/orbit/olivedata/contacts_server/html/"));
  server->RegisterHandler("/", new orbit::SimpleFileHandler("stream_service/orbit/olivedata/contacts_server/html/home.html"));
  return server;
}

static void SetupServer() {
  std::stringstream server_address;
  server_address << "0.0.0.0:" << FLAGS_port;
  ContactsServiceImpl service;

  LOG(INFO) << "Server listening on " << server_address.str();
  grpc::ServerBuilder builder;
  builder.AddListeningPort(server_address.str(),
                           grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  server->Wait();
}

}  // namespace orbit 

int main(int argc, char** argv) {
  /* Set thread name */
  prctl(PR_SET_NAME, (unsigned long)"main");

  google::InstallFailureSignalHandler();
  google::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);
  grpc_init();
  std::thread t([&] () {
    // Start HTTPServer mainloop in a separate thread
    orbit::HttpServer* http_server = orbit::SetupHttpServer();

    /* Set thread name */
    prctl(PR_SET_NAME, (unsigned long)"HttpServerInMain");

    http_server->StartServer();
    http_server->Wait();
  });

  orbit::SetupServer();

  t.join();
  
  grpc_shutdown();
  return 0;
}
