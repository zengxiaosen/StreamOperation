/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * http_handler.h
 * ---------------------------------------------------------------------------
 * Defines the HttpHandler model. The HttpHandler handles the requests.
 * ---------------------------------------------------------------------------
 */
#ifndef ORBIT_HTTP_HANDLER_H__
#define ORBIT_HTTP_HANDLER_H__

#include "http_common.h"
#include <memory>
#include "stream_service/orbit/http_server/http_common.h" // for HttpRequest and HttpResponse

namespace orbit {
// Common example usage of the HttpHandler:
// Derive a sub class from HttpHandler and then define the handler's
// HandleRequest function as follows:
//
// std::shared_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
//   GURL absolute_url = test_server_->GetURL(request.relative_url);
//   if (absolute_url.path() != "/test")
//     return scoped_ptr<HttpResponse>();
//
//   std::shared_ptr<HttpResponse> http_response(new HttpResponse());
//   http_response->set_code(SUCCESS);
//   http_response->set_content("hello");
//   http_response->set_content_type("text/plain");
//   return http_response;
// }

class HttpHandler {
 public:
  HttpHandler() {
    SetAuthMode(false, "", "", "");
  }
  void SetAuthMode(bool auth_mode,
                   const std::string& auth_location,
                   const std::string& auth_session_key,
                   const std::string& next_url) {
    auth_mode_ = auth_mode;
    auth_location_ = auth_location;
    auth_session_key_ = auth_session_key;
    next_url_ = next_url;
    if (next_url_.empty()) {
      next_url_ = "/";
    }
  }

  // Deprecated soon. We should use HandleRequest(request, response) instead.
  virtual std::shared_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    return NULL;
  }
  virtual void HandleRequest(const HttpRequest& request, std::shared_ptr<HttpResponse> response) {
    std::shared_ptr<HttpResponse> response1 = HandleRequest(request);
    if (response1 != NULL) {
      response1->SetSessionData(request.GetSessionData());
      response->CopyFrom(response1.get());
    }
  }

  void HandleRequestInternal(const HttpRequest& request, std::shared_ptr<HttpResponse> response) {
    std::string value;
    if (auth_mode_) {
      if (!request.GetSessionData().GetValue(auth_session_key_, &value) || value.empty()) {
        LOG(INFO) << "next_url=" << next_url_;
        response->SetSessionDataValue("next_url", next_url_);
        //response->set_code(HTTP_MOVED_PERMANENTLY); // 301 REDIRECT
        response->set_code(HTTP_TEMPORARY_REDIRECT); // 307 REDIRECT
        response->set_location(auth_location_);
        response->set_content_type("text/html");
        return;
      }
    }
    HandleRequest(request, response);
  }
 private:
  bool auth_mode_;
  std::string auth_location_;
  std::string auth_session_key_;
  std::string next_url_;
};

class SimpleHttpHandler : public HttpHandler {
 public:
  virtual std::shared_ptr<HttpResponse> HandleRequest(const HttpRequest& request) override;
};

class SimpleFileHandler : public HttpHandler {
 public:
  SimpleFileHandler();
  SimpleFileHandler(const std::string& filepath_root);

  ~SimpleFileHandler() {
  }
  virtual std::shared_ptr<HttpResponse> HandleRequest(const HttpRequest& request) override;

 private:
  std::string filepath_root_;
  bool single_file_mode_ = false;
};

}  // namespace orbit
#endif  // ORBIT_HTTP_HANDLER_H__

