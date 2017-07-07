/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * status_http_handler.h
 * ---------------------------------------------------------------------------
 * Defines the http handler for status page
 * -- Status page is used to monitor the status of the process and module
 * ---------------------------------------------------------------------------
 */

#ifndef STATUSZ_HANDLER_H__
#define STATUSZ_HANDLER_H__

#include "stream_service/orbit/webrtc/base/httpcommon.h"
#include "stream_service/orbit/http_server/http_handler.h"

#include "glog/logging.h"

namespace orbit {

class StatuszHandler : public HttpHandler {
 public:
  explicit StatuszHandler();
  virtual std::shared_ptr<HttpResponse> HandleRequest(const HttpRequest& request);
};

}  // namespace orbit

#endif  // STATUSZ_HANDLER_H__
