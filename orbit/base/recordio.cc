/*
 * Copyright 2016 (C) Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 * --------------------------------------------------------------------------
 * recordio.cc
 * -- Implements the class for reading and writting ProtocolBuffer object into
 *    recordio files.
 *
 * Reference from: https://github.com/google/or-tools
 * --------------------------------------------------------------------------
 */

#include <memory>
#include <string>

#include "zlib_util.h"
#include "recordio.h"
#include "glog/logging.h"

namespace orbit {

const int RecordWriter::kMagicNumber = 0x3ed7230a;

RecordWriter::RecordWriter(File* const file)
    : file_(file), use_compression_(true) {}

bool RecordWriter::Close() { return file_->Close(); }

void RecordWriter::set_use_compression(bool use_compression) {
  use_compression_ = use_compression;
}

std::string RecordWriter::Compress(std::string const& s) const {
  return ZlibCompress(s);
}

RecordReader::RecordReader(File* const file) : file_(file) {}

bool RecordReader::Close() { return file_->Close(); }

void RecordReader::Uncompress(const char* const source, uint64 source_size,
                              char* const output_buffer,
                              uint64 output_size) const {
  ZlibUncompress(source, source_size, output_buffer, output_size);
}

}  // namespace orbit
