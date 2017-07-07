/*
 * Copyright 2016 (C) Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * base.h --- defines the base class and utils functions.
 */

#ifndef ORBIT_BASE_H__
#define ORBIT_BASE_H__

#include "stream_service/orbit/webrtc/base/constructormagic.h"
#include "stream_service/orbit/logger_helper.h"

#include <string>
#include <iostream>
#include <fstream>

// The arraysize(arr) macro returns the # of elements in an array arr.
// The expression is a compile-time constant, and therefore can be
// used in defining new arrays, for example.  If you use arraysize on
// a pointer by mistake, you will get a compile-time error.
//
// One caveat is that arraysize() doesn't accept any array of an
// anonymous type or a type defined inside a function.  In these rare
// cases, you have to use the unsafe ARRAYSIZE_UNSAFE() macro below.  This is
// due to a limitation in C++'s template system.  The limitation might
// eventually be removed, but it hasn't happened yet.

// This template function declaration is used in defining arraysize.
// Note that the function doesn't need an implementation, as we only
// use its type.
template <typename T, size_t N>
char (&ArraySizeHelper(T (&array)[N]))[N];

// That gcc wants both of these prototypes seems mysterious. VC, for
// its part, can't decide which to use (another mystery). Matching of
// template overloads: the final frontier.
#ifndef _MSC_VER
template <typename T, size_t N>
char (&ArraySizeHelper(const T (&array)[N]))[N];
#endif

#define arraysize(array) (sizeof(ArraySizeHelper(array)))

// ARRAYSIZE_UNSAFE performs essentially the same calculation as arraysize,
// but can be used on anonymous types or types defined inside
// functions.  It's less safe than arraysize as it accepts some
// (although not all) pointers.  Therefore, you should use arraysize
// whenever possible.
//
// The expression ARRAYSIZE_UNSAFE(a) is a compile-time constant of type
// size_t.
//
// ARRAYSIZE_UNSAFE catches a few type errors.  If you see a compiler error
//
//   "warning: division by zero in ..."
//
// when using ARRAYSIZE_UNSAFE, you are (wrongfully) giving it a pointer.
// You should only use ARRAYSIZE_UNSAFE on statically allocated arrays.
//
// The following comments are on the implementation details, and can
// be ignored by the users.
//
// ARRAYSIZE_UNSAFE(arr) works by inspecting sizeof(arr) (the # of bytes in
// the array) and sizeof(*(arr)) (the # of bytes in one array
// element).  If the former is divisible by the latter, perhaps arr is
// indeed an array, in which case the division result is the # of
// elements in the array.  Otherwise, arr cannot possibly be an array,
// and we generate a compiler error to prevent the code from
// compiling.
//
// Since the size of bool is implementation-defined, we need to cast
// !(sizeof(a) & sizeof(*(a))) to size_t in order to ensure the final
// result has type size_t.
//
// This macro is not perfect as it wrongfully accepts certain
// pointers, namely where the pointer size is divisible by the pointee
// size.  Since all our code has to go through a 32-bit compiler,
// where a pointer is 4 bytes, this means all pointers to a type whose
// size is 3 or greater than 4 will be (righteously) rejected.

#define ARRAYSIZE_UNSAFE(a) \
  ((sizeof(a) / sizeof(*(a))) / \
   static_cast<size_t>(!(sizeof(a) % sizeof(*(a)))))

#define DISALLOW_COPY_AND_ASSIGN RTC_DISALLOW_COPY_AND_ASSIGN


using std::ios;
using std::ofstream;
using std::ifstream;

using std::string;
using std::vector;

class SimpleFileLineReader {
 public:
  SimpleFileLineReader(const string& filename) {
    filename_ = filename;
    ifs_ = NULL;
    ifs_ = new ifstream(filename.c_str(), ifstream::in);
    if (!ifs_->good()) {
      LOG(ERROR) << "File:" << filename << " does not exist.";
    }
  }
  ~SimpleFileLineReader() {
    if (ifs_)
      delete ifs_;
  }

  bool ReadLines(vector<string>* lines) {
    if (!ifs_->good())
      return false;
    string line;
    while(getline(*ifs_, line)) {
      lines->push_back(line);
    }
    return true;
  }

  bool ReadFile(string* text) {
    vector<string> lines;
    if (!ReadLines(&lines))
      return false;
    *text = "";
    vector<string>::const_iterator iter;
    for (iter = lines.begin(); iter != lines.end(); ++iter) {
      *text += *iter;
      *text += "\n";
    }
    return true;
  }

 private:
  string filename_;
  ifstream* ifs_;
  DISALLOW_COPY_AND_ASSIGN(SimpleFileLineReader);
};

class SimpleFileWriter {
 public:
  static bool AppendFile(const string& file, const string& message) {
    ofstream fs;
    fs.open(file.c_str(), ios::app);
    return WriteToFileInternal(&fs, message);
  }

  static bool OverwriteFile(const string& file, const string& message) {
    ofstream fs;
    fs.open(file.c_str(), ios::out);
    return WriteToFileInternal(&fs, message);
  }

 private:
  static bool WriteToFileInternal(ofstream* fs, const string& message) {
    if (fs->is_open()) {
      (*fs) << message;
      fs->close();
      return true;
    }
    return false;
  }

  DISALLOW_COPY_AND_ASSIGN(SimpleFileWriter);
};

class SimpleFileInfo {
  public:
    static bool FileExist(const string& filename) {
      ifstream ifs_;
      ifs_.open(filename.c_str(), ios::in);
      if (ifs_.good()) {
        return true;
      }
      return false;
    }

  DISALLOW_COPY_AND_ASSIGN(SimpleFileInfo);
};


#endif  // ORBIT_BASE_H__

