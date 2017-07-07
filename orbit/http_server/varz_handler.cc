/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * varz_http_handler.cc
 * ---------------------------------------------------------------------------
 * Implements the http handler for /varz page
 * ---------------------------------------------------------------------------
 */
#include "varz_handler.h"
#include "exported_var.h"
#include "stream_service/orbit/base/singleton.h"
#include "gflags/gflags.h"

namespace orbit {
using namespace std;

std::shared_ptr<HttpResponse> VarzHandler::HandleRequest(const HttpRequest& request) {
  std::shared_ptr<HttpResponse> http_response(new HttpResponse());
  http_response->set_code(HTTP_OK);
  orbit::ExportedVarManager* var_manager = Singleton<orbit::ExportedVarManager>::GetInstance();
  string varz_key;
  string body;
  if (request.GetQueryValue("key", &varz_key)) {
    if (HasPrefixString(varz_key, "\"")) {
      varz_key = StripPrefixString(varz_key, "\"");
    }
    if (HasSuffixString(varz_key, "\"")) {
      varz_key = StripSuffixString(varz_key, "\"");
    }
    body = var_manager->GetVarByName(varz_key);
  } else {
    body = var_manager->DumpExportedVars();
  }
  http_response->set_content(body);
  http_response->set_content_type("text/plain");
  return http_response;
}

std::shared_ptr<HttpResponse> FlagzHandler::HandleRequest(const HttpRequest& request) {
  std::shared_ptr<HttpResponse> http_response(new HttpResponse());
  http_response->set_code(HTTP_OK);
  string body;
  vector<GFLAGS_NAMESPACE::CommandLineFlagInfo> flags;
  GFLAGS_NAMESPACE::GetAllFlags(&flags);
  vector<GFLAGS_NAMESPACE::CommandLineFlagInfo>::const_iterator i;
  for (i = flags.begin(); i != flags.end(); ++i) {
    std::ostringstream oss;
    //oss << "// Defined in file:" << i->filename << "<br>";
    if (HasPrefixString(i->filename, "third_party")) {
      continue;
    }
    oss << "// " << i->description << " ";
    oss << "(Type:" << i->type << " ";
    oss << "Default value:" << i->default_value << ")<br>";
    oss << "FLAGS_" << i->name << "=" << i->current_value << "<br>";
    body += oss.str();
  }
  http_response->set_content(body);
  http_response->set_content_type("text/html");
  return http_response;
}


}  // namespace orbit
