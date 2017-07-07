/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * varz_handler.h
 * ---------------------------------------------------------------------------
 * Defines the http handler for varz page
 * -- Varz page is used to track all the exported variables from the server.
 * ---------------------------------------------------------------------------
 */


#ifndef VARZ_HANDLER_H__
#define VARZ_HANDLER_H__

#include "stream_service/orbit/webrtc/base/httpcommon.h"
#include "stream_service/orbit/http_server/http_handler.h"

#include "glog/logging.h"
#include <string>

namespace orbit {
/*
 * Varz handler for server, example usages:
 *  https://{host}:{port}/varz
 *  https://{host}:{port}/varz="VAR_NAME"
 *  
 */
class VarzHandler : public HttpHandler {
 public:
  explicit VarzHandler() {};
  virtual std::shared_ptr<HttpResponse> HandleRequest(const HttpRequest& request);
};

class FlagzHandler : public HttpHandler {
 public:
  explicit FlagzHandler() {};
  virtual std::shared_ptr<HttpResponse> HandleRequest(const HttpRequest& request);
};

}  // namespace orbit

#endif  // VARZ_HANDLER_H__
