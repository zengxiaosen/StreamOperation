/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * master_status_handler.h
 * ---------------------------------------------------------------------------
 * Defines the http handler for /mstatus page.
 *   - mstatus is used to track all the registered slave server and master status.
 * ---------------------------------------------------------------------------
 */
#pragma once

#include "stream_service/orbit/webrtc/base/httpcommon.h"
#include "stream_service/orbit/http_server/http_handler.h"
#include "slavedata_collector.h"

#include "glog/logging.h"
#include <string>

namespace orbit {
class OrbitMasterServerManager;

class MasterStatusHandler : public HttpHandler {
 public:
  explicit MasterStatusHandler(const string& root_dir) {
    root_dir_ = root_dir;
  };
  virtual std::shared_ptr<HttpResponse> HandleRequest(const HttpRequest& request);

 private:
  virtual std::shared_ptr<HttpResponse> HandleGetJsonData(const HttpRequest& request);
  virtual std::shared_ptr<HttpResponse> HandleHomepage(const HttpRequest& request);

  std::string RenderCpuStatus(const health::HealthStatus& status);
  std::string RenderMemoryStatus(const health::HealthStatus& status);
  std::string RenderLoadStatus(const health::HealthStatus& status);

  std::string RenderSlaveServers(OrbitMasterServerManager* manager);
  std::string RenderSessionServerInfo(OrbitMasterServerManager* manager);

  string root_dir_;
};

}  // namespace orbit
