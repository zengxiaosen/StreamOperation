/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * leveldb_example.cc
 * ---------------------------------------------------------------------------
 * This program demostrates the basic usage of leveldb:
 *  - Create a database
 *  - Put/Get operation of the databas
 *  - Iterates the key range
 * ---------------------------------------------------------------------------
 * Usage command line:
 * ---------------------------------------------------------------------------
 *   bazel-bin/stream_service/orbit/leveldb_example --logtostderr
 *  - First example: create a database:
 *   bazel-bin/stream_service/orbit/examples/leveldb_example --logtostderr --test_op=Create
 * ---------------------------------------------------------------------------
 * - Second example: read the database:
 *  bazel-bin/stream_service/orbit/examples/leveldb_example --logtostderr --test_op=Read  
 * I0516 15:59:17.575115 22952 leveldb_example.cc:87] Get key:xucheng value:130
 * I0516 15:59:17.575860 22952 leveldb_example.cc:92] Get key:majia value:115
 * ---------------------------------------------------------------------------
 * - Third examples: iterate the database:
 * bazel-bin/stream_service/orbit/examples/leveldb_example --logtostderr --test_op=Iterate
 * I0516 16:00:28.360244 23661 leveldb_example.cc:108] daxing->201
 * I0516 16:00:28.360882 23661 leveldb_example.cc:108] majia->115
 * I0516 16:00:28.361436 23661 leveldb_example.cc:108] qingyong->250
 * I0516 16:00:28.362053 23661 leveldb_example.cc:108] xucheng->130
 */

// For Gflags and Glog
#include "gflags/gflags.h"
#include "glog/logging.h"

#include "leveldb/db.h"
#include <string>

using namespace std;

DEFINE_string(test_op, "Read", "Test Ops: could be Create, Read or Iterate");

namespace orbit {
  void CreateDatabase() {
    leveldb::DB* db;
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, "/tmp/testdb", &db);
    assert(status.ok());

    string key = "xucheng";
    string value = "110";
    string key2 = "zhihan";
    string value2 = "120";
    string key3 = "xucheng";
    string value3 = "130";

    status = db->Put(leveldb::WriteOptions(), key, value);
    assert(status.ok());
    status = db->Put(leveldb::WriteOptions(), key2, value2);
    assert(status.ok());
    status = db->Put(leveldb::WriteOptions(), key3, value3);
    assert(status.ok());

    {
      string key = "majia";
      string value = "115";
      status = db->Put(leveldb::WriteOptions(), key, value);
      assert(status.ok());
    }

    {
      string key = "daxing";
      string value = "201";
      status = db->Put(leveldb::WriteOptions(), key, value);
      assert(status.ok());
    }
    
    {
      string key = "qingyong";
      string value = "250";
      status = db->Put(leveldb::WriteOptions(), key, value);
      assert(status.ok());
    }

    status = db->Delete(leveldb::WriteOptions(), key2);
    assert(status.ok());

    delete db;
  }
  
  void ReadDatabase() {
    leveldb::DB* db;
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, "/tmp/testdb", &db);
    assert(status.ok());

    string key = "xucheng";
    string value;
    status = db->Get(leveldb::ReadOptions(), key, &value);
    assert(status.ok());
    LOG(INFO) << "Get key:" << key << " value:" << value;

    key = "majia";
    status = db->Get(leveldb::ReadOptions(), key, &value);
    assert(status.ok());
    LOG(INFO) << "Get key:" << key << " value:" << value;
    
    delete db;
  }

  void IterateDatabase() {
    leveldb::DB* db;
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, "/tmp/testdb", &db);
    assert(status.ok());

    leveldb::ReadOptions read_options;
    leveldb::Iterator* it = db->NewIterator(read_options);
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
      string result = it->key().ToString() + "->" + it->value().ToString();
      LOG(INFO) << result;
    }
    delete it;
    delete db;
  }

  int RunMain() {
    if (FLAGS_test_op == "Create") {
      CreateDatabase();
    } else if (FLAGS_test_op == "Read") {    
      ReadDatabase();
    } else if (FLAGS_test_op == "Iterate") {
      IterateDatabase();
    } else {
      LOG(ERROR) << "Not implemented.";
      return -1;
    }
    return 0;    
  }
} // namespace orbit

using namespace std;

int main(int argc, char** argv) {
  google::InstallFailureSignalHandler();
  google::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);

  return orbit::RunMain();
}
