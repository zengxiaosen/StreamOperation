/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * pretty_signin_handler.h
 * ---------------------------------------------------------------------------
 * Defines the http handler for /signin page
 *   -- display the signin page from template
 *   -- verify the email and password
 * ---------------------------------------------------------------------------
 */
#pragma once

#include "stream_service/orbit/base/base.h"
#include "stream_service/orbit/webrtc/base/httpcommon.h"
#include "stream_service/orbit/http_server/http_handler.h"

#include <vector>

namespace orbit {
class PrettySigninHandler : public HttpHandler {
 public:
  explicit PrettySigninHandler(const string& server_name,
                               const std::vector<std::pair<string, string> > pass,
                               const string& session_key) {
    server_name_ = server_name;
    username_passwords_ = pass;
    session_key_ = session_key;
  };
  virtual void HandleRequest(const HttpRequest& request,
                             std::shared_ptr<HttpResponse> response);
 private:
  string server_name_;
  std::vector<std::pair<string, string> > username_passwords_;
  string session_key_;
};

}  // namespace orbit
