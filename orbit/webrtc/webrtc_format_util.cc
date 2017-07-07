/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * webrtc_format_util.cc
 * ---------------------------------------------------------------------------
 * Reference the code in webrtc implementation to parse the packet.
 * The main usage for this is to determine if there is a key frame in the video.
 * ---------------------------------------------------------------------------
 */

#include "webrtc_format_util.h"
#include <assert.h>
#include <glog/logging.h>

#include "stream_service/orbit/rtp/rtp_headers.h"
#include "stream_service/orbit/webrtc/modules/rtp_rtcp/source/rtp_format_vp8.h"
#include "stream_service/orbit/webrtc/modules/rtp_rtcp/source/rtp_format_vp9.h"
#include "stream_service/orbit/webrtc/modules/rtp_rtcp/source/rtp_format_h264.h"

namespace orbit {
  webrtc::RtpDepacketizerVp8 vp8_depacketizer_parser;
  webrtc::RtpDepacketizerVp9 vp9_depacketizer_parser;
  webrtc::RtpDepacketizerH264 h264_depacketizer_parser;

  // Parse the payload_data(the media data in the RTP packet), and returns
  // the parsed_payload for further analysis
  // Reference: the parsed payload format
  // struct ParsedPayload {
  //    const uint8_t* payload;
  //    size_t payload_length;
  //    FrameType frame_type;
  //    RTPTypeHeader type;
  // };
  bool RtpDepacketizer_Parse(int payload_type,
                             webrtc::RtpDepacketizer::ParsedPayload* parsed_payload,
                             const uint8_t* payload_data,
                             size_t payload_data_length) {
    if (payload_type == VP8_90000_PT) {
      return vp8_depacketizer_parser.Parse(parsed_payload,
                                           payload_data,
                                           payload_data_length);
    } else if (payload_type == VP9_90000_PT) {
      return vp9_depacketizer_parser.Parse(parsed_payload,
                                           payload_data,
                                           payload_data_length);
    } else if (payload_type == H264_90000_PT) {
      return h264_depacketizer_parser.Parse(parsed_payload,
                                            payload_data,
                                            payload_data_length);
    }
  }

}  // namespace orbit
