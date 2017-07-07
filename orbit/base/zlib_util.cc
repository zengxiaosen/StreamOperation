/*
 * Copyright 2016 (C) Orange Labs Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * zlib_util.h
 * ---------------------------------------------------------------------------
 * Defines the interface and functions to use the zlib to compress/uncompress
 * the data.
 * ---------------------------------------------------------------------------
 */
#include "zlib_util.h"

#include <string>
#include <zlib.h>
#include <memory>

#include "glog/logging.h"

namespace orbit {
std::string ZlibCompress(std::string const& s) {
  const unsigned long source_size = s.size();  // NOLINT
  const char* source = s.c_str();

  unsigned long dsize = compressBound(source_size); // NOLINT
  std::unique_ptr<char[]> destination(new char[dsize]);
  // Use compress() from zlib.h.
  const int result =
      compress(reinterpret_cast<unsigned char*>(destination.get()), &dsize,
               reinterpret_cast<const unsigned char*>(source), source_size);

  if (result != Z_OK) {
    LOG(FATAL) << "Compress error occured! Error code: " << result;
  }
  return std::string(destination.get(), dsize);
}

unsigned long ZlibUncompress(const char* const source, uint64 source_size,
                    char* const output_buffer,
                    uint64 output_size) {
  unsigned long result_size = output_size;  // NOLINT
  // Use uncompress() from zlib.h
  const int result =
      uncompress(reinterpret_cast<unsigned char*>(output_buffer), &result_size,
                 reinterpret_cast<const unsigned char*>(source), source_size);
  if (result != Z_OK) {
    LOG(FATAL) << "Uncompress error occured! Error code: " << result;
  }
  CHECK_LE(result_size, static_cast<unsigned long>(output_size));  // NOLINT
  return result_size;
}

std::string ZlibUncompress(const std::string& input, int max_output_size) {
  std::unique_ptr<char[]> buffer(new char[max_output_size + 1]);
  unsigned long result_size = ZlibUncompress(input.c_str(), input.size(), buffer.get(), max_output_size);
  return std::string(buffer.get(), result_size);
}    


}  // namespace orbit
