/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * simple_demo_http_server.cc
 * ---------------------------------------------------------------------------
 * Demostrate the code to run a simple demo-level http server.
 * Provides the following functionalities:
 *   - Create a embeded http server at the specified port.
 *   - Create a demo page and a demo login page. The demo page need to login first.
 * ---------------------------------------------------------------------------
 */

// For Gflags and Glog
#include "gflags/gflags.h"
#include "glog/logging.h"

#include <thread>
#include <chrono> 
#include <sys/prctl.h>

#include "http_server.h"
#include "statusz_handler.h"
#include "varz_handler.h"
#include "exported_var.h"
#include "stream_service/orbit/base/sys_info.h"

#include "stream_service/orbit/webrtc/base/httpcommon.h"
#include "stream_service/orbit/http_server/http_handler.h"
#include "stream_service/orbit/http_server/html_writer.h"
#include "stream_service/orbit/http_server/pretty_signin_handler.h"

DEFINE_string(server_pem, "./stream_service/orbit/server/certs/intviu.pem", "The server's secure certificates.");
DEFINE_string(server_key, "./stream_service/orbit/server/certs/intviu.key", "The server's secure key.");
DEFINE_bool(use_ssl, true, "A flag to set if using SSL connection.");

namespace orbit {
  namespace {
const string kHomeTemplate = "" \
"      <html><body> " \
"        Hello, Welcome <b>{{USER_NAME}} </b>to system.<br> " \
"      </body></html>";

const string kLoginTemplate = "" \
"      <html><body> " \
"        Login to system.</br> " \
"      <form action=\"/login\" method=\"get\">" \
"        <input type=\"text\" name=\"username\" /><br> " \
"        <input type=\"text\" name=\"password\" /> <br>" \
"        <input type=\"submit\" value=\"Submit\" /> <br>" \
"      </form> " \
"      </body></html>";
  }
class DemoPageHandler : public HttpHandler {
 public:
  virtual void HandleRequest(const HttpRequest& request,
                             std::shared_ptr<HttpResponse> response) {
    string uri = request.url();
    if (uri == "/") {
      HandleHomepage(request, response);
    } else if (uri == "/login") {
      HandleLoginpage(request, response);
    }
  }
 private:
  virtual void HandleLoginpage(const HttpRequest& request,
                               std::shared_ptr<HttpResponse> http_response) {
    string user_name;
    string password;
    if (request.GetQueryValue("username", &user_name) &&
        request.GetQueryValue("password", &password)) {
      if (password == "letmein" && user_name == "xucheng") {
        // Login success. redirect to homepage "/".
        string html_body = "login success.";
        http_response->SetSessionDataValue("user_name", user_name);
        http_response->set_code(HTTP_MOVED_PERMANENTLY); // 301 REDIRECT
        http_response->set_content(html_body);
        http_response->set_location("/");
        http_response->set_content_type("text/html");
        return;
      }
    }

    TemplateHTMLWriter temp;
    temp.LoadTemplate(kLoginTemplate);
    string html_body = temp.Finalize();
    http_response->set_code(HTTP_OK);
    http_response->set_content(html_body);
    http_response->set_content_type("text/html");
    return;
  }

  virtual void HandleHomepage(const HttpRequest& request,
                               std::shared_ptr<HttpResponse> http_response) {
    string user_name;
    request.GetSessionData().GetValue("user_name", &user_name);

    TemplateHTMLWriter temp;
    temp.LoadTemplate(kHomeTemplate);
    temp.SetValue("USER_NAME", user_name);
    string html_body = temp.Finalize();
    http_response->set_code(HTTP_OK);
    http_response->set_content(html_body);
    http_response->set_content_type("text/html");
  }
};
}

int main(int argc, char** argv) {
  google::InstallFailureSignalHandler();
  google::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);

  orbit::HttpServer server(9100, FLAGS_use_ssl ? orbit::HttpServer::TYPE_HTTPS : orbit::HttpServer::TYPE_HTTP);

  orbit::SystemInfo* sys_info = Singleton<orbit::SystemInfo>::GetInstance();
  sys_info->Init();

  // Start HTTPServer mainloop in a separate thread
  std::thread t([&] () {
    /* Set thread name */
    prctl(PR_SET_NAME, (unsigned long)"HTTPServerMainloop");

    server.SetSecureCertificates(FLAGS_server_pem, FLAGS_server_key);
    server.SetThreadPool(4);
    orbit::DemoPageHandler* demo_page_handler = new orbit::DemoPageHandler();
    demo_page_handler->SetAuthMode(true, "/signin", "user_name", "/");
    server.RegisterHandler("/", demo_page_handler);
    server.RegisterHandler("/signin", new orbit::PrettySigninHandler("SimpleDemoServer",
                                                                      "xucheng@orangelab.cn",
                                                                      "uxy12345ab",
                                                                      "user_name"));
    server.RegisterHandler("/login", new orbit::DemoPageHandler);
    orbit::VarzHandler* varz_handler = new orbit::VarzHandler();
    varz_handler->SetAuthMode(true, "/signin", "user_name", "/varz");
    server.RegisterHandler("/varz", varz_handler);
    server.RegisterHandler("/statusz", new orbit::StatuszHandler);
    server.RegisterHandler("/file", new orbit::SimpleFileHandler);
    server.StartServer();
    server.Wait();
  });

  // Test test....
  orbit::ExportedVar var("test_file_count");
  var.Set(120);

  // Wait for the thread to resume.
  t.join();

  return 0;
}
