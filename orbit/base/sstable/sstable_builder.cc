// Copyright 2015 (C) Orange lab. All Rights Reserved.
//
// Author: i.xucheng@gmail.com (Cheng Xu)


#include "sstable_builder.h"
#include "stream_service/orbit/base/sstable/sstable.pb.h"
#include "stream_service/orbit/base/zlib_util.h"
#include "glog/logging.h"

#include <algorithm>
#include <sys/stat.h>
#include <functional>  // for std::hash

using std::ifstream;
using std::ofstream;
using std::ios;
using std::sort;

namespace sstable {

// TODO(chengxu): Refactor this code to a standalone utils class.
// TODO(chengxu): Provide both reader and writer functions in this class.
class RecordFile {
 public:
  RecordFile() {
    fs_ = NULL;
  }

  virtual ~RecordFile() {
    if (fs_ != NULL) {
      Close();
    }
  }

  virtual void Open(const string& filename) {
    filename_ = filename;
    fs_ = new ofstream();
    fs_->open(filename.c_str(), ios::out | ios::binary);
  }


  virtual void Close() {
    if (fs_ != NULL) {
      if (fs_->is_open()) {
        fs_->close();
      }
      delete fs_;
    }
    fs_ = NULL;
  }

  virtual void WriteRecord(const string& data) {
    int data_size = data.size();
    fs_->write(data.c_str(), data_size);
  }
  
  virtual void WriteInteger(int data) {
    fs_->write((char*)&data, sizeof(int));
  }

  virtual int GetOffset() {
    return fs_->tellp();
  }
 private:

  ofstream* fs_;
  string filename_;
  DISALLOW_COPY_AND_ASSIGN(RecordFile);
};

SimpleSSTableBuilder::SimpleSSTableBuilder(const string& file_name) {
  use_compression_ = true;
  file_name_ = file_name;
  file_ = new RecordFile();
  file_->Open(file_name_);
}

SimpleSSTableBuilder::~SimpleSSTableBuilder() {
}

// TODO(chengxu): Replace this hash_func, this is very simple and easy to
// collide with other keys.
  /*
int hash_func(const string& key) {
  unsigned long h = 0;
  const char* s = key.c_str();
  for ( ; *s; ++s)
    h = 5*h + *s;

  return h % 65535;
  }*/
  
int hash_func(const string& key) {
  std::size_t h1 = std::hash<std::string>()(key);
  return h1;
}

void SimpleSSTableBuilder::Add(string key, string value) {
  SSTableDataItem* item = new SSTableDataItem;
  item->set_key(key);
  item->set_data(value);
  int hash_key = hash_func(key);
  if (holder_.find(hash_key) != holder_.end()) {
    vector<SSTableDataItem*> key_chain = holder_[hash_key];
    key_chain.push_back(item);
    holder_[hash_key] = key_chain;
  } else {
    vector<SSTableDataItem*>* new_key_chain = new vector<SSTableDataItem*>;
    new_key_chain->push_back(item);
    holder_[hash_key] = (*new_key_chain);
  }
}

std::string SimpleSSTableBuilder::Compress(std::string const& s) const {
  return orbit::ZlibCompress(s);
}
  
void SimpleSSTableBuilder::Build() {
  vector<int> hash_key_set;
  {
    map<int, vector<SSTableDataItem* > >::const_iterator iter =
        holder_.begin();
    for (; iter != holder_.end(); ++iter) {
      hash_key_set.push_back((*iter).first);
    }

    sort(hash_key_set.begin(), hash_key_set.end());
  }

  SSTableMetadata metadata;
  vector<int>::const_iterator iter = hash_key_set.begin();

  int file_offset = 0;
  for (; iter != hash_key_set.end(); ++iter) {
    vector<SSTableDataItem* > key_chain = (*(holder_.find(*iter))).second;
    // ASSERT(key_chain != null);
    vector<SSTableDataItem* >::const_iterator item_iter =
        key_chain.begin();
    SSTableBlock* block = new SSTableBlock;
    block->set_algorithm(SSTableBlock::FLAT_STRING);
    //block->set_algorithm(SSTableBlock::ZIPPY_COMPRESSION);
    for (; item_iter != key_chain.end(); ++item_iter) {
      SSTableDataItem* item = block->add_data();
      item->CopyFrom(*(*item_iter));
    }

    string out;
    block->SerializeToString(&out);
    const std::string compressed_buffer =
      use_compression_ ? Compress(out) : "";
    file_->WriteInteger(out.size());
    file_->WriteInteger(compressed_buffer.size());
    if (use_compression_) {
      file_->WriteRecord(compressed_buffer);
    } else {
      file_->WriteRecord(out);
    }
    
    SSTableIndex* index = metadata.add_index();
    index->set_key_range(*iter);
    index->set_offset(file_offset);
    file_offset = file_->GetOffset();
  }

  {
    string out;
    metadata.SerializeToString(&out);
    const std::string compressed_buffer =
      use_compression_ ? Compress(out) : "";
    if (use_compression_) {
      file_->WriteRecord(compressed_buffer);
    } else {
      file_->WriteRecord(out);
    }
    file_->WriteInteger(out.size());
    file_->WriteInteger(compressed_buffer.size());
  }

  file_->Close();
}

SimpleSSTable::SimpleSSTable() {
  use_compression_ = true;
  metadata_ = new SSTableMetadata();
}

SimpleSSTable::~SimpleSSTable() {
  delete metadata_;
}

bool SimpleSSTable::Open(const string& file) {
  fs_ = new ifstream();
  fs_->open(file.c_str(), ios::in | ios::binary);
  if (!fs_->is_open()) {
    LOG(ERROR) << "Open file:" << file << " not exist.";
    return false;
  }
  struct stat result;
  int file_size;
  if (stat(file.c_str(), &result) == 0) {
    file_size = result.st_size;
  } else {
    LOG(ERROR) << "Stat file:" << file << " error.";
    return false;
  }
  fs_->seekg(file_size - 2 * sizeof(int));

  int metadata_size = 0;
  int metadata_compressed_size = 0;
  fs_->read((char*)&metadata_size, sizeof(int));
  fs_->read((char*)&metadata_compressed_size, sizeof(int));

  if (metadata_compressed_size != 0 && !use_compression_) {
    LOG(ERROR) << "Read a compressed sstable file using uncompressed mode.";
    return false;
  }

  if (metadata_size <= 0 || metadata_size > file_size) {
    LOG(ERROR) << "Metadata of the file:" << file << " is corrupted.";
    return false;
  }

  if (!use_compression_) {
    int metadata_offset = file_size - metadata_size - 2 * sizeof(int);
    char* metadata_str = new char[metadata_size];
    fs_->seekg(metadata_offset);
    fs_->read(metadata_str, metadata_size);
    string metadata_ser(metadata_str, metadata_size);
    metadata_->ParseFromString(metadata_ser);
    delete metadata_str;
  } else {
    int metadata_offset = file_size - metadata_compressed_size - 2 * sizeof(int);
    char* metadata_compressed_str = new char[metadata_compressed_size];
    char* metadata_str = new char[metadata_size];
    fs_->seekg(metadata_offset);
    fs_->read(metadata_compressed_str, metadata_compressed_size);
    Uncompress(metadata_compressed_str, metadata_compressed_size, metadata_str, metadata_size);
    string metadata_ser(metadata_str, metadata_size);
    metadata_->ParseFromString(metadata_ser);
    delete metadata_str;
    delete metadata_compressed_str;
  }
  CreateMap();
  return true;
}

void SimpleSSTable::Uncompress(const char* const source, uint64 source_size,
                               char* const output_buffer,
                               uint64 output_size) const {
  orbit::ZlibUncompress(source, source_size, output_buffer, output_size);
}


string SimpleSSTable::LookupFirst(const string& key) {
  vector<string> results = Lookup(key);
  if (results.size() < 1) {
    return "";
  }
  return results[0];
}

void SimpleSSTable::CreateMap() {
  for (int i = 0; i < metadata_->index_size(); ++i) {
    const SSTableIndex& index = metadata_->index(i);
    offset_map_[index.key_range()] = index.offset();
  }
}

  
int SimpleSSTable::FindOffset(int key_hash) {
  int offset = -1;
  if (offset_map_.find(key_hash) != offset_map_.end()) {
    offset = offset_map_[key_hash];
  }
  return offset;
}
  
vector<string> SimpleSSTable::Lookup(const string& key) {
  int key_hash = hash_func(key);

  vector<string> results;

  int offset = FindOffset(key_hash);
  if (offset == -1)
    return results;
  fs_->seekg(offset);

  int data_size = -1;
  int data_compressed_size = 0;
  fs_->read((char*)&data_size, sizeof(int));
  fs_->read((char*)&data_compressed_size, sizeof(int));

  if (!use_compression_ && data_compressed_size != 0) {
    LOG(ERROR) << "Read a compressed sstable file using uncompressed mode.";
    return results;
  }

  SSTableBlock block;
  if (use_compression_) {
    char* data_str = new char[data_compressed_size];
    fs_->read(data_str, data_compressed_size);
    char* new_data_str = new char[data_size];
    Uncompress(data_str, data_compressed_size, new_data_str, data_size);
    string data_ser(new_data_str, data_size);
    block.ParseFromString(data_ser);
    delete new_data_str;
    delete data_str;
  } else {
    char* data_str = new char[data_size];
    fs_->read(data_str, data_size);
    string data_ser(data_str, data_size);
    block.ParseFromString(data_ser);
    delete data_str;
  }
  
  for (int i = 0; i < block.data_size(); ++i) {
    const SSTableDataItem& item = block.data(i);
    if (item.key() == key) {
      results.push_back(item.data());
    }
  }
  return results;
}

}  // namespace sstable

