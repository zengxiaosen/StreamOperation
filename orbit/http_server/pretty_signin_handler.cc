/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * pretty_signin_handler.cc
 * ---------------------------------------------------------------------------
 * Implements the http handler for /signin for single sign on.
 * ---------------------------------------------------------------------------
 */
#include "pretty_signin_handler.h"

#include "html_writer.h"
#include "gflags/gflags.h"

DECLARE_string(template_dir);

namespace orbit {
using namespace std;

namespace {
}  // annoymous namespace

void PrettySigninHandler::HandleRequest(const HttpRequest& request,
                                        std::shared_ptr<HttpResponse> http_response) {
  LOG(INFO) << "HandleRequest" << request.url();
  string username;
  string password;
  if (request.GetQueryValue("inputEmail", &username) &&
      request.GetQueryValue("inputPassword", &password)) {
    LOG(INFO) << "query received.";
    bool success = false;
    for (pair<string, string> item : username_passwords_) {
      if (password == item.second &&
          username == item.first) {
        success = true;
        break;
      }
    }
    if (success) {
      // Login success. redirect to homepage "/".
      string html_body = "login success.";
      http_response->SetSessionDataValue(session_key_, username);
      http_response->set_code(HTTP_MOVED_PERMANENTLY); // 301 REDIRECT
      //http_response->set_code(HTTP_TEMPORARY_REDIRECT); // 307 REDIRECT
      http_response->set_content(html_body);

      string next_url;
      if (!request.GetSessionData().GetValue("next_url", &next_url)) {
        next_url = "/";
      }
      LOG(INFO) << "next_url=" << next_url;
      http_response->set_location(next_url);
      http_response->set_content_type("text/html");
      return;
    }
  }

  string root_dir = FLAGS_template_dir;
  TemplateHTMLWriter temp;
  string template_file = root_dir + "signin.tpl";
  temp.LoadTemplateFile(template_file);
  temp.SetValue("SERVER_NAME", server_name_);

  string html_body = temp.Finalize();
  http_response->set_code(HTTP_OK);
  http_response->set_content(html_body);
  http_response->set_content_type("text/html");
  return;
}

}  // namespace orbit
