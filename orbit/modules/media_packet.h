/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: qingyong@orangelab.cn (Qingyong Zhang)
 *
 * media_packet.h
 * ---------------------------------------------------------------------------
 * Defines the data structure in the media processor:
 * - MediaDataPacket -- represents the input data packet with the media data
 * - MediaOutputPacket -- represents the output data packet, media data and
 * -                      the RTP related information.
 * ---------------------------------------------------------------------------
 */
#ifndef MEDIA_PACKET_H__
#define MEDIA_PACKET_H__
#include "stream_service/orbit/media_definitions.h"
#include <opus/opus.h>

#include <vector>

namespace orbit{

  struct MediaDataPacket {
    packetType type;
    opus_int16* media_buf = NULL;
    int length;
    MediaDataPacket() {
      type = OTHER_PACKET;
      media_buf = NULL;
      length = -1;
    }
    ~MediaDataPacket() {
      if(media_buf != NULL) {
        free(media_buf);
        media_buf = NULL;
      }
    }
  };

  struct MediaOutputPacket {
    unsigned char* encoded_buf = NULL;
    int length;
    uint32_t ssrc;
    uint32_t timestamp;
    uint16_t seq_number;

    // VP8 only field. Indicates if the packet is end of a frame.
    // 0 - end of a frame, 1 - continuation of a frame
    uint8_t end_frame;

    // Audio only field. Represents the energy of the audio stream.
    opus_int32 audio_energy;

    MediaOutputPacket() {
      encoded_buf = NULL;
      length = -1;
      ssrc = 0;
      timestamp = 0;
      seq_number = 0;
      end_frame = 0;
    }
    ~MediaOutputPacket() {
      if(encoded_buf != NULL) {
        free(encoded_buf);
        encoded_buf = NULL;
      }
    }
  };

  struct UnencodedPacket {
      opus_int16* unencoded_buf = NULL;
      int length;
      std::vector<uint32_t> stream_vec;
      uint32_t ssrc;
      uint32_t timestamp;
      uint16_t seq_number;

      // VP8 only field. Indicates if the packet is end of a frame.
      // 0 - end of a frame, 1 - continuation of a frame
      uint8_t end_frame;

      // Audio only field. Represents the energy of the audio stream.
      opus_int32 audio_energy;

      UnencodedPacket() {
        unencoded_buf = NULL;
        length = -1;
        ssrc = 0;
        timestamp = 0;
        seq_number = 0;
        end_frame = 0;
      }
      ~UnencodedPacket() {
        if(unencoded_buf != NULL) {
          free(unencoded_buf);
          unencoded_buf = NULL;
        }
      }
    };
}
#endif
