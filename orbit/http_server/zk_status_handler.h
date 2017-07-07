/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: zhanghao@orangelab.cn
 *
 * zk_status_handler.h
 * ---------------------------------------------------------------------------
 * Defines the http handler for zookeeper status page
 * ---------------------------------------------------------------------------
 */

#ifndef ZK_STATUSZ_HANDLER_H__
#define ZK_STATUSZ_HANDLER_H__

#include "stream_service/orbit/webrtc/base/httpcommon.h"
#include "stream_service/orbit/http_server/http_handler.h"
#include "stream_service/proto/registry.grpc.pb.h"

#include "glog/logging.h"
#include <string>

namespace orbit {

class ZkStatuszHandler : public HttpHandler {
 public:
  explicit ZkStatuszHandler() {
  };
  virtual std::shared_ptr<HttpResponse> HandleRequest(const HttpRequest& request);

 private:
  //virtual std::shared_ptr<HttpResponse> HandleGetJsonData(const HttpRequest& request);
  virtual std::shared_ptr<HttpResponse> HandleHomepage(const HttpRequest& request);

  std::string RenderCpuStatus(const health::HealthStatus& status);
  std::string RenderMemoryStatus(const health::HealthStatus& status);

  std::string RenderSlaveServers();
};

}  // namespace orbit

#endif  // ZK_STATUSZ_HANDLER_H__
