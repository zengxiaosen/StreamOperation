/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * http_handler.cc
 * ---------------------------------------------------------------------------
 * Implements the http handlers.
 * ---------------------------------------------------------------------------
 */
#include "http_handler.h"
#include "gflags/gflags.h"
#include "html_writer.h"

DEFINE_string(template_dir, "./stream_service/orbit/http_server/html/", "The html template directory.");

namespace orbit {
 
#define PAGE "libmicrohttpd demo"

std::shared_ptr<HttpResponse> SimpleHttpHandler::HandleRequest(const HttpRequest& request) {
  std::shared_ptr<HttpResponse> http_response(new HttpResponse());
  http_response->set_code(HTTP_OK);
  http_response->set_content(PAGE);
  http_response->set_content_type("text/plain");
  return http_response;
}

SimpleFileHandler::SimpleFileHandler() {
  filepath_root_ = FLAGS_template_dir;
}

SimpleFileHandler::SimpleFileHandler(const std::string& filepath_root) {
  filepath_root_ = filepath_root;
  if (HasSuffixString(filepath_root_, ".html") ||
      HasSuffixString(filepath_root_, ".htm")) {
    single_file_mode_ = true;
  }
}

//@virtual
std::shared_ptr<HttpResponse> SimpleFileHandler::HandleRequest(const HttpRequest& request) {
  std::shared_ptr<HttpResponse> http_response(new HttpResponse());
  string filename;
  string value;
  if (request.GetQueryValue("file", &value)) {
    //char* newkey = HTUnEscape(const_cast<char*>(value.c_str()));
    filename = value;
    if (!filename.empty()) {
      filename = StringReplace(filename, "..", "", true);
    }
  } else {
    // If no query "file=xxx" is present in the request URL, assume the file is the filepath_root_
    filename = request.url();
    LOG(INFO) << "value=" << filename;
    if (single_file_mode_) {
      filename = "";
    } else if (HasPrefixString(filename, "/")) {
      filename = StripPrefixString(filename, "/");
    }
  }

  string out;
  SimpleFileLineReader reader(filepath_root_ + filename);
  reader.ReadFile(&out);
  if (!out.empty()) {
    http_response->set_code(HTTP_OK);
    http_response->set_content(out);
    if (filename.empty()) {
      http_response->set_content_type("text/html");
    } else {
      // TODO(chengxu): determine the content_type based on the filename.
      if (HasSuffixString(filename, ".js")) {
        http_response->set_content_type("text/javascript");
      } else if (HasSuffixString(filename, ".css")) {
        http_response->set_content_type("text/css");
      } else {
        http_response->set_content_type("text/plain");
      }
    }
    return http_response;
  } else {
    string html_body = "<html><meta http-equiv=\"content-type\" "
      "content=\"text/html; charset=utf-8\"><body>";
    string html_ending = "File not found.</body></html>";
    html_body += html_ending;;
    
    http_response->set_code(HTTP_NOT_FOUND);
    http_response->set_content(html_body);
    http_response->set_content_type("text/html");
    return http_response;
  }
}

}  // namespace orbit
