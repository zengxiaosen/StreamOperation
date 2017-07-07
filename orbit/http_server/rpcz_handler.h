/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * rpcz_handler.h
 * ---------------------------------------------------------------------------
 * Defines the http handler for /rpcz page
 *  Rpcz page tracks all the RPC call statsitics including
 *  - each method of the service: call number, fail count
 *  - the call latencies of each methods in the service.
 * ---------------------------------------------------------------------------
 */
#pragma once

#include "stream_service/orbit/webrtc/base/httpcommon.h"
#include "stream_service/orbit/http_server/http_handler.h"

namespace orbit {
class RpczHandler : public HttpHandler {
 public:
  explicit RpczHandler() {};
  virtual std::shared_ptr<HttpResponse> HandleRequest(const HttpRequest& request);
};

}  // namespace orbit
