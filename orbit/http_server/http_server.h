/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * http_server.h
 * ---------------------------------------------------------------------------
 * Defines the abstract interface of a simple embeded http server.
 * ---------------------------------------------------------------------------
 */
#ifndef ORBIT_HTTP_SERVER_H__
#define ORBIT_HTTP_SERVER_H__

#include <string>
#include <map>
#include <memory>

// libmicrohttpd library 
#include <microhttpd.h>

// Http handlers and HttpRequest/HttpResponse
#include "http_handler.h"
#include "http_common.h"

// For thread
#include <thread>
// For chrono::seconds
#include <chrono> 

namespace orbit {

using std::string;
using std::map;

/*
 HttpServer is a embeded http server that is super simple and efficient. The
 common use cases for a embeded http server:
 * Distributed system status monitor
 * System dashboard 
 * JSON-RPC server 
--------------------------------
 Common example usage of the HttpServer:
    orbit::HttpServer server(9100, FLAGS_use_ssl ? orbit::HttpServer::TYPE_HTTPS : orbit::HttpServer::TYPE_HTTP);
    server.SetSecureCertificates(FLAGS_server_pem, FLAGS_server_key);
    server.SetThreadPool(4);
    server.RegisterHandler("/varz", new orbit::VarzHandler);
    server.RegisterHandler("/statusz", new orbit::StatuszHandler);
    server.RegisterHandler("/file", new orbit::SimpleFileHandler);
    server.StartServer();
    server.Wait();

  Use RegisterHandler to register the HTTP handler for URL endpoint handlers.
  Note that StartServer is just starting an daemon in the system. It will not block
  the main thread. If you want to wait until the server is finished, use Wait().
*/
class HttpServer {
 public:
  typedef std::map<std::string, HttpHandler*> HttpHandlerMap;
  enum Type {
    TYPE_HTTP,
    TYPE_HTTPS,
  };

  enum ServerCertificate {
    CERT_OK,
    CERT_MISMATCHED_NAME,
    CERT_EXPIRED,
    // A certificate with invalid notBefore and notAfter times. Windows'
    // certificate library will not parse this certificate.
    CERT_BAD_VALIDITY,
    // Cross-signed certificate to test PKIX path building. Contains an
    // intermediate cross-signed by an unknown root, while the client (via
    // TestRootStore) is expected to have a self-signed version of the
    // intermediate.
    CERT_CHAIN_WRONG_ROOT,
    // Causes the testserver to use a hostname that is a domain
    // instead of an IP.
    CERT_COMMON_NAME_IS_DOMAIN,
  };

  HttpServer();
  HttpServer(int port, Type type);

  ~HttpServer();

  void SetSecureCertificates(const string& server_pem,
                             const string& server_key) {
    server_pem_ = server_pem;
    server_key_ = server_key;
  }

  int GetPort() {
    return port_;
  }

  void SetPort(int port) {
    port_ = port;
  }

  bool StartServer();
  bool IsRunning() {
    return started_;
  }
  void Wait() {
    while(IsRunning()) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }
  bool Stop();

  void SetThreadPool(int thread_number);
  bool RegisterHandler(const string& uri,
                       HttpHandler* handler);
  bool RegisterDefaultHandler();
  HttpHandler* GetHttpHandler(const string& uri);

  std::string CreateSession(HttpSession* session_value) {
    return session_manager_->CreateSession(session_value);
  }
  bool FindSessionByCookie(std::string cookie_value, HttpSession* session_value) {
    return session_manager_->FindSessionByCookie(cookie_value, session_value);
  }
  bool UpdateSession(std::string cookie_value, const HttpSession& session_value) {
    return session_manager_->UpdateSession(cookie_value, session_value);
  }
  void DebugHttpSessions() {
    session_manager_->DebugHttpSessions();
  }

 private:
  // The port of the http server.
  int port_;
  // Specifies the thread number using for accepting http connection/requests.
  int thread_number_;
  // Indicates that the server is running or stopped.
  bool started_;
  // The http server type: HTTPS/HTTP
  Type type_;

  // The secure certificates of http server. If the server type is HTTPS,
  // The two parameters are required to set up a secure server.
  string server_pem_;
  string server_key_;

  std::unique_ptr<HttpHandlerMap> handlers_;
  std::unique_ptr<HttpSessionManager> session_manager_;
  struct MHD_Daemon* daemon_;
};

}  // namespace orbit

#endif  // ORBIT_HTTP_SERVER_H__
