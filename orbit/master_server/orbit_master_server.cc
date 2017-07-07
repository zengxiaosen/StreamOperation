/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * orbit_master_server.cc
 * ---------------------------------------------------------------------------
 * Implements a master server for orbit stream service
 * ---------------------------------------------------------------------------
 */

// For gRpc
#include <grpc/grpc.h>
#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>

// For Gflags and Glog
#include "gflags/gflags.h"
#include "glog/logging.h"

// For thread
#include <thread>
#include <sys/prctl.h>
// For chrono::seconds
#include <chrono> 

// For all the service.
#include "stream_service/proto/registry.grpc.pb.h"
#include "stream_service/proto/stream_service.grpc.pb.h"

#include "registry_service_impl.h"
#include "forward_stream_service_impl.h"
#include "orbit_master_server_manager.h"

// For base::singleton
#include "stream_service/orbit/base/singleton.h"

// For the http server and handler with implementation
#include "stream_service/orbit/http_server/statusz_handler.h"
#include "stream_service/orbit/http_server/varz_handler.h"
#include "stream_service/orbit/http_server/rpcz_handler.h"
#include "stream_service/orbit/http_server/pretty_signin_handler.h"
#include "stream_service/orbit/http_server/http_server.h"

// For sys_info.h - SystemInfo
#include "stream_service/orbit/base/sys_info.h"

// Master status http handler.
#include "master_status_handler.h"

// For port check
#include "stream_service/orbit/http_server/port_checker.h"

DEFINE_int32(registry_port, 12350, "Listening port of Registry RPC service.");
DEFINE_int32(stream_service_port, 10000, "Listening port of StreamService RPC service.");

// HTTP Server
DEFINE_string(server_pem, "./stream_service/orbit/server/certs/intviu.pem", "The server's secure certificates.");
DEFINE_string(server_key, "./stream_service/orbit/server/certs/intviu.key", "The server's secure key.");
DEFINE_int32(http_port, 12000, "Port to listen on with HTTP protocol");

DEFINE_string(master_handler_resource_dir, "./stream_service/orbit/master_server/html/",
              "The html template and other css/js etc. resource directory for master server.");
DEFINE_bool(use_http_auth, true, "If set to true, will use the http_auth for all handlers.");

#define SERVER_NAME "Orbit Master Server"
#define SESSION_KEY "user_name"

namespace orbit {
  using namespace registry;
  using namespace std;
  using namespace olive;

  static void RunRegistryServer() {
    std::stringstream server_address;
    server_address << "0.0.0.0:" << FLAGS_registry_port;
    RegistryServiceImpl service;
    OrbitMasterServerManager* manager = Singleton<OrbitMasterServerManager>::GetInstance();
    manager->SetupRegistry(&service);

    LOG(INFO) << "Server listening on " << server_address.str();
    // PortChecker class is used to check the port if is in use.
    // When the port is in used, BuildAndStart is blocked, and after
    // timeout seconds check the flag of port_is_used. 
    bool port_is_used = true;
    int check_time_out = 3;
    orbit::PortChecker port_checker(port_is_used, FLAGS_registry_port, check_time_out);
    
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address.str(),
                             grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    port_is_used = false;
    server->Wait();
  }

  static void RunForwardStreamService() {
    std::stringstream server_address;
    server_address << "0.0.0.0:" << FLAGS_stream_service_port;
    ForwardStreamServiceImpl service;
    service.Init();

    LOG(INFO) << "Server listening on " << server_address.str();
    // PortChecker class is used to check the port if is in use.
    // When the port is in used, BuildAndStart is blocked, and after
    // timeout seconds check the flag of port_is_used. 
    bool port_is_used = true;
    int check_time_out = 3;
    orbit::PortChecker port_checker(port_is_used, FLAGS_stream_service_port, check_time_out);

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address.str(),
                             grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    port_is_used = false;
    server->Wait();
  }

  static HttpServer* SetupHttpServer() {
    int threads = sysconf(_SC_NPROCESSORS_ONLN);

    HttpServer* server = new HttpServer(FLAGS_http_port, orbit::HttpServer::TYPE_HTTPS);

    server->SetThreadPool(threads);
    server->SetSecureCertificates(FLAGS_server_pem, FLAGS_server_key);
    server->RegisterHandler("/file",
                            new orbit::SimpleFileHandler());
    server->RegisterHandler("/data",
                            new orbit::SimpleFileHandler(FLAGS_master_handler_resource_dir));

    std::vector<pair<string, string>> userpass =
      { { "xucheng@orangelab.cn", "uxy12345ab" },
        { "s", "s8192" }, 
      };
    server->RegisterHandler("/signin", new orbit::PrettySigninHandler(SERVER_NAME,
                                                                      userpass,
                                                                      SESSION_KEY));

    // Other endpoints
    orbit::VarzHandler* varz_handler = new orbit::VarzHandler();
    server->RegisterHandler("/varz", varz_handler);
    orbit::RpczHandler* rpcz_handler = new orbit::RpczHandler();
    server->RegisterHandler("/rpcz", rpcz_handler);
    orbit::StatuszHandler* statusz_handler = new orbit::StatuszHandler();
    server->RegisterHandler("/statusz", statusz_handler);
    orbit::MasterStatusHandler* mstatus_handler = new orbit::MasterStatusHandler(FLAGS_master_handler_resource_dir);
    server->RegisterHandler("/mstatus", mstatus_handler);

    // Secure endpoints if flag is enabled.
    if (FLAGS_use_http_auth) {
      varz_handler->SetAuthMode(true, "/signin", SESSION_KEY, "/varz");
      rpcz_handler->SetAuthMode(true, "/signin", SESSION_KEY, "/rpcz");
      statusz_handler->SetAuthMode(true, "/signin", SESSION_KEY, "/statusz");
      mstatus_handler->SetAuthMode(true, "/signin", SESSION_KEY, "/mstatus");
    }
    return server;
  }
}

int main(int argc, char** argv) {
  /* Set thread name */
  prctl(PR_SET_NAME, (unsigned long)"main");

  google::InstallFailureSignalHandler();
  google::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);
  Singleton<orbit::SystemInfo>::GetInstance()->Init();
  grpc_init();

  std::thread t1([&] () {
      orbit::RunRegistryServer();
  });
  std::thread t2([&] () {
      orbit::RunForwardStreamService();
  });
  
  // Start HTTPServer mainloop in a separate thread
  orbit::HttpServer* http_server = orbit::SetupHttpServer();
  std::thread t3([&] () {
    /* Set thread name */
    prctl(PR_SET_NAME, (unsigned long)"HttpServerInMain");
    LOG(INFO) << "Start http_server... on port" << FLAGS_http_port;
    http_server->StartServer();
    http_server->Wait();
  });

  t1.join();
  t2.join();
  t3.join();

  grpc_shutdown();
  return 0;
}
