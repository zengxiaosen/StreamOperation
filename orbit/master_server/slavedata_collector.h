/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: zhihan@orangelab.cn (Zhihan He)
 *
 * slavedata_collector.h
 * ---------------------------------------------------------------------------
 * Defines the collector for collector slave data and show out.
 * ---------------------------------------------------------------------------
 */

#pragma once

#include "stream_service/orbit/base/singleton.h"
#include "stream_service/proto/registry.grpc.pb.h"

#include "server_stat.h"

#include <stdio.h>
#include <string>
#include <vector>

namespace leveldb {
  class DB;
}

namespace orbit {
  using namespace registry;

  namespace health {
    class HealthStatus;
  }

  class SlaveDataCollector {
    public:
      bool Add(std::string key, health::HealthStatus stat);
      std::vector<health::HealthStatus> Find(std::string start,std::string limit);
      
    private:
      leveldb::DB* db_;
      DEFINE_AS_SINGLETON_WITHOUT_CONSTRUCTOR(SlaveDataCollector);
  };
  
}
