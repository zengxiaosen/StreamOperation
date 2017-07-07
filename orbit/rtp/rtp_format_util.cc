/*
 * rtp_format_util.cc
 * 
 *  Created on: Aug 16, 2016
 *      Author: chenteng
 */

#include "rtp_format_util.h"
#include "glog/logging.h"
#include "stream_service/orbit/rtp/rtp_headers.h"
#include "stream_service/orbit/media_definitions.h"
#include "stream_service/orbit/webrtc/webrtc_format_util.h"

namespace orbit {
  bool IsValidVideoPacket(const orbit::dataPacket& rtp_packet) {
    webrtc::RtpDepacketizer::ParsedPayload payload;
    const unsigned char* buf = 
      reinterpret_cast<const unsigned char*>(&(rtp_packet.data[0]));
    int length = rtp_packet.length;

    const RtpHeader* h = reinterpret_cast<const RtpHeader*>(buf);
    int video_payload = (int)(h->getPayloadType());

    bool ret = RtpDepacketizer_Parse(video_payload, &payload, buf + 12, length - 12);
    return ret;
  }

  bool IsKeyFramePacket(std::shared_ptr<orbit::dataPacket> rtp_packet) {
    webrtc::RtpDepacketizer::ParsedPayload payload;
    const unsigned char* buf = 
        reinterpret_cast<const unsigned char*>(&(rtp_packet->data[0]));
    int length = rtp_packet->length;

    const RtpHeader* h = reinterpret_cast<const RtpHeader*>(buf);
    int video_payload = (int)(h->getPayloadType());

    bool ret = RtpDepacketizer_Parse(video_payload, &payload, buf + 12, length - 12);
    if (!ret) {
      LOG(ERROR) << "Parse the payload failed.";
      return false;
    } else {
     //PrintParsedPayload(payload);
      if (payload.frame_type == webrtc::kVideoFrameKey) {
        return true;
      }
    }
    return false;
  }
} // end of namespace orbit
