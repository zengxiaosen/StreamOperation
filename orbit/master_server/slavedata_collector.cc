/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: zhihan@orangelab.cn (Zhihan He)
 *
 * slavedata_collector.cc
 * ---------------------------------------------------------------------------
 * Defines the exporte_var for the variables exported to /varz handler.
 * ---------------------------------------------------------------------------
 */

#include "slavedata_collector.h"
// For Gflags and Glog
#include "gflags/gflags.h"
#include "glog/logging.h"

#include "leveldb/db.h"

DEFINE_string(slave_data_db_path, "/tmp/slavedatadb", "save slave data folder path");

namespace orbit {
  SlaveDataCollector::SlaveDataCollector() {
    leveldb::Options options;
    options.create_if_missing = true;
    //options.create_if_missing = false;
    leveldb::Status status;
    status = leveldb::DB::Open(options, FLAGS_slave_data_db_path, &db_);
    if(!status.ok()) {
      LOG(ERROR) << "Open levelDB failed";
      db_ = NULL;
    }
  }

  SlaveDataCollector::~SlaveDataCollector() {
    if (db_ != NULL) {
      delete db_;
      db_ = NULL;
    }
  }


 bool SlaveDataCollector::Add(string key, health::HealthStatus stat) {
    // Like ip:port_time (192.168.1.126:10001_1465033376)
    VLOG(2) << "save to level db key:" << key << ";";
    //LOG(INFO) << "save to level db value:" << stat.jsonData();

    if(db_ == NULL) {
      LOG(ERROR) << "save slave data error at " << key << " failed;db is null";
      return false;
    }
    string buffer;
    stat.SerializeToString(&buffer);
    leveldb::Status status = db_->Put(leveldb::WriteOptions(), key, buffer);

    if(!status.ok()) {
      LOG(ERROR) << "save slave data error at " << key << " failed when save data";
      return false;
    }
    return true;
  }

  std::vector<health::HealthStatus> SlaveDataCollector::Find(std::string start,std::string limit) {
    vector<health::HealthStatus> return_vector;
    if(db_ == NULL) {
      LOG(ERROR) << "search error;db is NULL";
      return return_vector;
    }

    leveldb::Iterator* it = db_->NewIterator(leveldb::ReadOptions());

    // string start = "192.168.1.126:10001_1465033376";
    // string limit = "192.168.1.126:10001_1465033400";

    for (it->Seek(start);
         it->Valid() && it->key().ToString() <= limit;
         it->Next()) {
      //LOG(INFO) << "find data:" << it->key().ToString() << " : "  << it->value().ToString();
      health::HealthStatus health_status;
      health_status.ParseFromString(it->value().ToString());
      return_vector.push_back(health_status);
    }

    if(it->status().ok()) {
      delete it;
    }
    return return_vector;
  }
 
}
