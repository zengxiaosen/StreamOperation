/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * webrtc_format_util.h
 * ---------------------------------------------------------------------------
 * Reference the code in webrtc implementation to parse the packet.
 * The main usage for this is to determine if there is a key frame in the video.
 * ---------------------------------------------------------------------------
 */

#pragma once

#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format.h"
namespace orbit {
bool RtpDepacketizer_Parse(int payload_type,
                             webrtc::RtpDepacketizer::ParsedPayload* parsed_payload,
                             const uint8_t* payload_data,
                             size_t payload_data_length);
}  // namespace orbit
