// Copyright 2015 (C) Orange lab. All Rights Reserved.
//
// Author: i.xucheng@gmail.com (Cheng Xu)
//
// Module description:
//   This module provides a useful key-value persistent mapping data structure.
// It is persistent and immutable as the disk file. The index is mostly in memory
// to make it very quickly to lookup. It is *not* a real sstable in that it does
// not have the iterator of the sorted keys. But the lookup operations could be
// very quickly, says 10K lookup per second.
//
// Usage:
// * Use SimpleSSTableBuilder to build and store a sstable file. Samples code
// snippet:
//  sstable::SimpleSSTableBuilder builder(FLAGS_sstable_file);
//  builder.Add(key, value);
//  builder.Add(key2, value2);
//  ......
//  builder.Build();
//
// * Use SSTable to open and lookup in the table
//   sstable::SimpleSSTable table;
//   table.Open(FLAGS_sstable_file);
//   vector<string> values = table.Lookup(key);
//   Since there is possibility that many values are associated with one single
//   key, vector<string> is used to store the found values (we don't support
//   streaming values yet). If the vector is empty, it indicates that there is
//   no value found on this specified key.

#ifndef SSTABLE_SSTABLE_BUILDER_H__
#define SSTABLE_SSTABLE_BUILDER_H__
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include "stream_service/orbit/base/base.h"
#include "stream_service/orbit/base/file.h"
#include "stream_service/orbit/base/zlib_util.h"

using std::string;
using std::map;
using std::vector;
using std::ifstream;

namespace sstable {
class RecordFile;
class SSTableDataItem;

// SSTableBuilder interface
class SSTableBuilder {
 public:
  virtual void Add(string key, string value) = 0;
  virtual void Build() = 0;
};

// SSTable interface
class SSTable {
 public:
  virtual bool Open(const string& file) = 0;
  virtual vector<string> Lookup(const string& key) = 0;
};

class SSTableMetadata;

// Generate a sstable and add the mapping. Save it to a persistent file.
class SimpleSSTableBuilder : public SSTableBuilder {
 public:
  SimpleSSTableBuilder(const string& file_name);
  virtual ~SimpleSSTableBuilder();
  virtual void Add(string key, string value);

  template <class P>
  void AddProtocolMessage(string key, const P& proto) {
    std::string buffer;
    proto.SerializeToString(&buffer);
    Add(key, buffer);
  }
  
  virtual void Build();
  void set_use_compression(bool use_compression) {
    use_compression_ = use_compression;
  }

 private:
  std::string Compress(const std::string& input) const;
  
  string file_name_;
  RecordFile* file_;
  map<int, vector<SSTableDataItem* > > holder_;
  bool use_compression_;

  DISALLOW_COPY_AND_ASSIGN(SimpleSSTableBuilder);
};

// Open a sstable and lookup keys from it.
class SimpleSSTable : public SSTable {
 public:
  SimpleSSTable();
  virtual ~SimpleSSTable();
  virtual bool Open(const string& file);
  virtual vector<string> Lookup(const string& key);
  virtual string LookupFirst(const string& key);

  template <class P>
  vector<P*> LookupProtocolMessage(const string& key) {
    vector<string> values = Lookup(key);
    vector<P*> ret;
    for (string value : values) {
      P* proto = new P;
      proto->ParseFromString(value);
      ret.push_back(proto);
    }
    return ret;
  }

  void set_use_compression(bool use_compression) {
    use_compression_ = use_compression;
  }

  // TODO(chengxu): Add method to iterate the sorted keys
  // virtual SSTable_Iterator GetIterator(const string fromKeys);
 private:
  int FindOffset(int key_hash);
  void Uncompress(const char* const source, uint64 source_size,
                  char* const output_buffer, uint64 output_size) const;
  void CreateMap();
  map<int, int> offset_map_;

  SSTableMetadata* metadata_;
  ifstream* fs_;
  bool use_compression_;
  DISALLOW_COPY_AND_ASSIGN(SimpleSSTable);
};

}  // namespace sstable

#endif  // SSTABLE_SSTABLE_BUILDER_H__
