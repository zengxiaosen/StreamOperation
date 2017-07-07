/*
 * Copyright 2016 (C) Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 * --------------------------------------------------------------------------
 * file.h
 * -- defines the classes for reading, writting and manipulating basic files.
 *
 * Reference from: https://github.com/google/or-tools
 * --------------------------------------------------------------------------
 */

#ifndef ORBIT_BASE_FILE_H__
#define ORBIT_BASE_FILE_H__

#include <cstdlib>
#include <cstdio>
#include <string>

#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/io/tokenizer.h"
#include "status.h"

#include "glog/logging.h"

#define uint64 uint64_t
#define int64 int64_t

// This file defines some IO interfaces to compatible with Google
// IO specifications.
namespace orbit {

class File {
 public:
  static const int DEFAULT_FILE_MODE = 0777;

  // Opens file "name" with flags specified by "flag".
  // Flags are defined by fopen(), that is "r", "r+", "w", "w+". "a", and "a+".
  static File* Open(const char* const name, const char* const flag);

#ifndef SWIG  // no overloading
  inline static File* Open(const std::string& name, const char* const mode) {
    return Open(name.c_str(), mode);
  }
#endif  // SWIG

  // Opens file "name" with flags specified by "flag"
  // If open failed, program will exit.
  static File* OpenOrDie(const char* const name, const char* const flag);

#ifndef SWIG  // no overloading
  inline static File* OpenOrDie(const std::string& name, const char* const flag) {
    return OpenOrDie(name.c_str(), flag);
  }
#endif  // SWIG

  // Reads "size" bytes to buff from file, buff should be pre-allocated.
  size_t Read(void* const buff, size_t size);

  // Reads "size" bytes to buff from file, buff should be pre-allocated.
  // If read failed, program will exit.
  void ReadOrDie(void* const buff, size_t size);

  // Reads a line from file to a std::string.
  // Each line must be no more than max_length bytes
  char* ReadLine(char* const output, uint64 max_length);

  // Reads the whole file to a std::string, with a maximum length of 'max_length'.
  // Returns the number of bytes read.
  int64 ReadToString(std::string* const line, uint64 max_length);

  // Writes "size" bytes of buff to file, buff should be pre-allocated.
  size_t Write(const void* const buff, size_t size);

  // Writes "size" bytes of buff to file, buff should be pre-allocated.
  // If write failed, program will exit.
  void WriteOrDie(const void* const buff, size_t size);

  // Writes a std::string to file.
  size_t WriteString(const std::string& line);

  // Writes a std::string to file and append a "\n".
  bool WriteLine(const std::string& line);

  // Closes the file.
  bool Close();
  util::Status Close(int flags);

  // Flushes buffer.
  bool Flush();

  // Returns file size.
  size_t Size();

  // Inits internal data structures.
  static void Init();

  // Returns the file name.
  std::string filename() const;

  // Deletes a file.
  static bool Delete(const char* const name);
  static bool Delete(const std::string& name) {
    return Delete(name.c_str());
  }

  // Tests if a file exists.
  static bool Exists(const char* const name);

  bool Open() const;

  // NOTE(chengxu): the following statci functions are borrowed from
  // protobuf library: //thirdparty/protobuf/upstream/src/google/protobuf/testing/file.h
  // Create a directory.
  static bool CreateDir(const string& name, int mode = DEFAULT_FILE_MODE);

  // Create a directory and all parent directories if necessary.
  static bool RecursivelyCreateDir(const string& path, int mode = DEFAULT_FILE_MODE);

  // If "name" is a file, we delete it.  If it is a directory, we
  // call DeleteRecursively() for each file or directory (other than
  // dot and double-dot) within it, and then delete the directory itself.
  static void RecursivelyDeleteDirOrFile(const string& name);

 private:
  File(FILE* const descriptor, const std::string& name);

  FILE* f_;
  const std::string name_;
};

namespace file {
inline int Defaults() { return 0xBABA; }

// As of 2016-01, these methods can only be used with flags = file::Defaults().
util::Status Open(const std::string& filename, const std::string& mode,
                  File** f, int flags);
util::Status SetTextProto(const std::string& filename, const google::protobuf::Message& proto,
                          int flags);
util::Status SetBinaryProto(const std::string& filename,
                            const google::protobuf::Message& proto, int flags);
util::Status SetContents(const std::string& filename, const std::string& contents,
                         int flags);
util::Status GetContents(const std::string& filename, std::string* output, int flags);
util::Status WriteString(File* file, const std::string& contents, int flags);

bool ReadFileToString(const std::string& file_name, std::string* output);
bool WriteStringToFile(const std::string& data, const std::string& file_name);
bool ReadFileToProto(const std::string& file_name, google::protobuf::Message* proto);
void ReadFileToProtoOrDie(const std::string& file_name, google::protobuf::Message* proto);
bool WriteProtoToASCIIFile(const google::protobuf::Message& proto,
                           const std::string& file_name);
void WriteProtoToASCIIFileOrDie(const google::protobuf::Message& proto,
                                const std::string& file_name);
bool WriteProtoToFile(const google::protobuf::Message& proto, const std::string& file_name);
void WriteProtoToFileOrDie(const google::protobuf::Message& proto,
                           const std::string& file_name);

util::Status Delete(const std::string& path, int flags);

}  // namespace file

}  // namespace orbit

#endif  // ORBIT_BASE_FILE_H__
