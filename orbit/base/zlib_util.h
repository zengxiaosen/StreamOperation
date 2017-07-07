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

#pragma once

#include <string>
#include "file.h"

namespace orbit {
std::string ZlibCompress(const std::string& input);
std::string ZlibUncompress(const std::string& input, int max_output_size);
unsigned long ZlibUncompress(const char* const source, uint64 source_size,
                             char* const output_buffer, uint64 output_size);
 
}  // namespace orbit
