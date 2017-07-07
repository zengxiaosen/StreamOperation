/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * orbit_stream_server.cc
 * ---------------------------------------------------------------------------
 * Implements the stream server based on Orbit webrtc gateway.
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

#include "gst_util.h"
#include <google/protobuf/text_format.h>

// For thread
#include <thread>
#include <sys/prctl.h>
// For chrono::seconds
#include <chrono> 

// For the service implementation and methods
#include "orbit_stream_service_impl.h"

// For sys_info.h
#include "stream_service/orbit/base/sys_info.h"

// Singleton
#include "stream_service/orbit/base/singleton.h"

// For GetCurrentTime_MS
#include "stream_service/orbit/base/timeutil.h"

// For get the public ip.
#include "stream_service/orbit/stun_server_prober.h"

// For the http server and handler with implementation
#include "stream_service/orbit/http_server/statusz_handler.h"
#include "stream_service/orbit/http_server/varz_handler.h"
#include "stream_service/orbit/http_server/rpcz_handler.h"
#include "stream_service/orbit/http_server/pretty_signin_handler.h"
#include "stream_service/orbit/http_server/http_server.h"
#include "stream_service/orbit/production/machine_db.h"
#include "stream_service/orbit/http_server/zk_status_handler.h"

// For slave mode.
#include "stream_service/proto/registry.grpc.pb.h"

// For port check
#include "stream_service/orbit/http_server/port_checker.h"

// For gRpc client
#include <grpc++/channel.h>
#include <grpc++/client_context.h>
#include <grpc++/create_channel.h>
#include <grpc++/security/credentials.h>
#include <grpc++/support/status.h>
#include <grpc++/support/status_code_enum.h>

// Build info and build timestamp.
#include <ctime>
#include "stream_service/orbit/http_server/build_info.h"

// Initialize the nice debugging.
#include "stream_service/orbit/nice_connection.h"

#include "stream_service/orbit/server/orbit_zk_client.h"
#include "stream_service/orbit/http_server/zk_status_handler.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using grpc::StatusCode;

DEFINE_int32(port, 10000, "Listening port of RPC service");
DEFINE_int32(http_port, 11000, "Port to listen on with HTTP protocol");
DEFINE_int32(threads, 0, "Number of threads to listen on. Numbers <= 0 "
             "will use the number of cores on this machine.");

DEFINE_bool(only_public_ip, false, "use public ip only on candidate.");
DEFINE_string(stun_server_ip, "101.200.238.173", "The stun server's ip");
DEFINE_int32(stun_server_port, 3478, "The stun server's port");

DEFINE_string(turn_server_ip, "", "The turn server's ip");
DEFINE_int32(turn_server_port, 3478, "The turn server's port");
DEFINE_string(turn_server_username, "", "The turn server's username");
DEFINE_string(turn_server_pass, "", "The turn server's password");

DEFINE_string(server_pem, "./stream_service/orbit/server/certs/intviu.pem", "The server's secure certificates.");
DEFINE_string(server_key, "./stream_service/orbit/server/certs/intviu.key", "The server's secure key.");

DECLARE_string(network_interface);
DEFINE_bool(enable_red_fec, true, "EnablR Video RED/FEC codecs");
DEFINE_string(packet_capture_directory, "", "Specify the directory to store the capture file. e.g. /tmp/");
DEFINE_string(packet_capture_filename, "", "If any filename is specified, "
              "the rtp packet capture will be enabled and stored into the file."
              " e.g. video_rtp.pb");

DEFINE_bool(slave_mode, false, "Work as slave mode. Need to connect to master server.");
DEFINE_bool(use_zookeeper, true, "Work as zookeeper mode, the orbit need to regist itself to zookeeper when the orbit started.");
DEFINE_string(master_server, "localhost:12350", "The master server ip and port");
DEFINE_string(set_local_address, "", "If set, we will not use public ip as the server "
              "address. We will use the specified local address,instead.");

DEFINE_int32(nice_min_port, 0,
              "Indicate the min port value for all nice connection.");
DEFINE_int32(nice_max_port, 0,
              "Indicate the max port value for all nice connection.");

DEFINE_bool(use_http_auth, true, "If set to true, will use the http_auth for all handlers.");
DEFINE_int32(leave_time, 600000, "After the leave_time(ms), the stream has no data coming in, then close the stream and update the statusz");
DEFINE_int32(mute_time, 30000, "The audio energy is always lower than ENERGY_LOW(1000000) after mute_time(ms), set the value of ProbeNetTestResult.");

DEFINE_bool(nice_debug, false, "Set to true if the nice debugging is enabled.");

#define SERVER_NAME "Orbit Stream Server"
#define SESSION_KEY "user_name"

using orbit::HttpServer;
using orbit::registry::RegistryService;

bool running_ = true;

namespace olive {

class StubFactory {
public:
  // InitStub - initializes the gRPC stub if necessary.
  std::shared_ptr<RegistryService::Stub> InitStub(const string& server) {
    if (stub_ != NULL) {
      return stub_;
    }
    std::shared_ptr<Channel>
      channel(grpc::CreateChannel(server,
                                  grpc::InsecureCredentials()));
    stub_ = RegistryService::NewStub(channel);
    return stub_;
  }
  // Destroy the stub.
  void DestroyStub() {
    stub_ = NULL;
  }
private:
  std::shared_ptr<RegistryService::Stub> stub_;
  DEFINE_AS_SINGLETON(StubFactory);
};

class OrbitMainController {
public:
  HttpServer* SetupHttpServer() {
    if (FLAGS_threads <= 0) {
      FLAGS_threads = sysconf(_SC_NPROCESSORS_ONLN);
      CHECK(FLAGS_threads > 0);
    }

    HttpServer* server = new HttpServer(FLAGS_http_port, orbit::HttpServer::TYPE_HTTPS);
    server->SetThreadPool(FLAGS_threads);
    server->SetSecureCertificates(FLAGS_server_pem, FLAGS_server_key);

    std::vector<pair<string, string>> userpass =
      { { "xucheng@orangelab.cn", "uxy12345ab" },
        { "s", "s8192" }, 
      };

    // Unsecure endpoints
    server->RegisterHandler("/signin", new orbit::PrettySigninHandler(SERVER_NAME,
                                                                      userpass,
                                                                      SESSION_KEY));
    server->RegisterHandler("/file", new orbit::SimpleFileHandler);
    
    // Other endpoints
    orbit::VarzHandler* varz_handler = new orbit::VarzHandler();
    server->RegisterHandler("/varz", varz_handler);
    orbit::FlagzHandler* flagz_handler = new orbit::FlagzHandler();
    server->RegisterHandler("/flagz", flagz_handler);
    orbit::RpczHandler* rpcz_handler = new orbit::RpczHandler();
    server->RegisterHandler("/rpcz", rpcz_handler);
    orbit::StatuszHandler* statusz_handler = new orbit::StatuszHandler();
    server->RegisterHandler("/statusz", statusz_handler);
    orbit::ZkStatuszHandler* zk_statusz_handler = new orbit::ZkStatuszHandler();
    server->RegisterHandler("/zkstatus", zk_statusz_handler);
    
    // Secure endpoints if flag is enabled.
    if (FLAGS_use_http_auth) {
      varz_handler->SetAuthMode(true, "/signin", SESSION_KEY, "/varz");
      rpcz_handler->SetAuthMode(true, "/signin", SESSION_KEY, "/rpcz");
      statusz_handler->SetAuthMode(true, "/signin", SESSION_KEY, "/statusz");
      zk_statusz_handler->SetAuthMode(true, "/signin", SESSION_KEY, "/zkstatus");
    }

    return server;
  }

  void RegisterSlaveClient(std::string server, string public_ip) {
    orbit::registry::RegisterRequest request;
  
    request.mutable_server()->set_name("OrbitStreamService");
    if (!FLAGS_set_local_address.empty()) {
      request.mutable_server()->set_host(FLAGS_set_local_address);
    } else {
      orbit::MachineDb* machine_db = Singleton<orbit::MachineDb>::get();
      vector<string> public_hosts = machine_db->ReverseAddressToMachine(public_ip);
      if (public_hosts.size() >= 1) {
        request.mutable_server()->set_host(public_hosts[0]);
      } else {
        request.mutable_server()->set_host(public_ip);
      }
    }
    request.mutable_server()->set_port(FLAGS_port);
    request.mutable_server()->set_http_port(FLAGS_http_port);
    
    // Check if we have the updated system health status. If we have, put it in the request.
    orbit::health::HealthStatus health_status = 
      Singleton<orbit::SystemInfo>::GetInstance()->GetHealthStatus();
    
    if (last_health_check_ts_ != health_status.ts()) {
      // We have updated system health status.
      last_health_check_ts_ = health_status.ts();
      request.mutable_status()->CopyFrom(health_status);
    }
    //long long start = orbit::getTimeMS();
    int retries = 0;
    while (retries < 3) {
      ClientContext context;
      std::chrono::system_clock::time_point deadline =
        std::chrono::system_clock::now() + std::chrono::seconds(2);
      context.set_deadline(deadline);
      orbit::registry::RegisterResponse response;
      StubFactory* stub_factory = Singleton<StubFactory>::GetInstance();
      std::shared_ptr<RegistryService::Stub> stub = stub_factory->InitStub(server);
      Status status = stub->Register(&context, request, &response);
      //LOG(INFO) << "elapsed time=" << orbit::getTimeMS() - start << " ms";
      retries++;
      if (status.ok()) {
        // The Register RPC operation is ok. Exit the retries loop.
        break;
      } else {
        // The status of the RPC operation is not ok. See if this is deadline exceed.
        LOG(INFO) << "Rpc failed. errmsg=" << status.error_message();
        if (status.error_code() == StatusCode::DEADLINE_EXCEEDED) {
          LOG(INFO) << "Destroy the current stub. Renew a stub for server.";
          stub_factory->DestroyStub();
        }
      }
    }
  }
private:
  int last_health_check_ts_ = 0;
};


}  // namespace orbit

int main(int argc, char** argv) {
  /* Set thread name */
  prctl(PR_SET_NAME, (unsigned long)"main");

  google::InstallFailureSignalHandler();
  google::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);

  if (FLAGS_nice_debug) {
    orbit::NiceConnection::EnableDebug();
  }

  grpc_init();
  olive::InitGstreamer(argc,argv);
  Singleton<orbit::SystemInfo>::GetInstance()->SetNetworkInterface(FLAGS_network_interface);

  // Get the public ip to check whether the Stun Server is usable.
  if (FLAGS_only_public_ip) {
    orbit::StunServerProber* prober = Singleton<orbit::StunServerProber>::get();
    std::string public_ip = prober->GetPublicIP(FLAGS_stun_server_ip, FLAGS_stun_server_port);
    if (public_ip.empty()) {
      LOG(INFO) <<"Not find public ip,the server shutdown.";
      return 0;
    }
  }

  // Create the RPC server
  std::stringstream server_address;
  server_address << "0.0.0.0:" << FLAGS_port;
  olive::OrbitStreamServiceImpl service;
  service.Init();

  LOG(INFO) << "gRPC version:" << grpc_version_string();

  LOG(INFO) << "Server listening on " << server_address.str();

  // PortChecker class is used to check the port if is in use.
  // When the port is in used, BuildAndStart is blocked, and after
  // timeout seconds check the flag of port_is_used. 
  bool port_is_used = true;
  int check_time_out = 3;
  orbit::PortChecker port_checker(port_is_used, FLAGS_port, check_time_out);

  grpc::ServerBuilder builder;
  builder.AddListeningPort(server_address.str(),
                           grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  port_is_used = false;

  // Start HTTPServer mainloop in a separate thread
  olive::OrbitMainController controller;
  HttpServer* http_server = controller.SetupHttpServer();
  std::thread t([&] () {
    /* Set thread name */
    prctl(PR_SET_NAME, (unsigned long)"HttpServerInMain");

    http_server->StartServer();
    http_server->Wait();
  });

  std::unique_ptr<std::thread> t2;
  // Regist itself to zookeeper
  orbit::zookeeper::ZookeeperRegister* zk_register = Singleton<orbit::zookeeper::ZookeeperRegister>::GetInstance();
  std::unique_ptr<std::thread> zk_t;

  if (FLAGS_slave_mode) {
    if (FLAGS_use_zookeeper) {
      zk_register->Init();
      zk_register->Enter();
      zk_t.reset(new std::thread([&] () {
            zk_register->RegistDataLoop();
          }));
    }


    t2.reset(new std::thread([&] () {
        /* Set thread name */
        prctl(PR_SET_NAME, (unsigned long)"SlaveModeThread");

        std::string server = FLAGS_master_server;  // "localhost:10000"
        LOG(INFO) << "Start to connect to master server:'" << server << "'.";
        orbit::StunServerProber* prober = Singleton<orbit::StunServerProber>::get();
        std::string public_ip = prober->GetPublicIP(FLAGS_stun_server_ip, FLAGS_stun_server_port);
        assert(!public_ip.empty());
        LOG(INFO) << "Get public ip=" << public_ip;

        while(running_) {
          controller.RegisterSlaveClient(server, public_ip);
          //std::this_thread::sleep_for(std::chrono::microseconds(3000));
          std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        }));
  }

  // Wait for the server to shutdown.
  server->Wait();

  // Wait for the thread to resume.
  t.join();

  if (FLAGS_slave_mode) {
    if (zk_t) {
      zk_t->join();
    }
    t2->join();
    // Close zookeeper register
    zk_register->CloseServer();
  }

  grpc_shutdown();
  return 0;
}
