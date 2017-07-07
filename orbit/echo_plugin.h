/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * echo_plugin.h
 * ---------------------------------------------------------------------------
 * Defines the interface of an echo audio/video plugin.
 * The echo plugin is used mostly for a program to test new application.
 * ---------------------------------------------------------------------------
 */

#ifndef ECHO_PLUGIN_H__
#define ECHO_PLUGIN_H__

#include "media_definitions.h"
#include "transport_plugin.h"
#include "rtp/rtp_packet_queue.h"
#include "audio_conference_plugin.h"
#include "modules/media_packet.h"

#include <opus/opus.h>

namespace orbit {

  class EchoPlugin : public TransportPlugin {
  public:
    EchoPlugin() {
      sampling_rate_ = 48000;  // HACK(chengxu): for now, we set it to 48000.
      encoder_ = NULL;
      decoder_ = NULL;

      if (!Init()) {
        LOG(ERROR) << "Create the audioRoom, init failed.";
      }
    }
    ~EchoPlugin() {
      Release();
    }

    void IncomingRtpPacket(const dataPacket& packet);
    void IncomingRtcpPacket(const dataPacket& packet);

    void RelayMediaOutputPacket(const MediaOutputPacket& packet);

  private:
    void DecodePacket(const unsigned char* buffer, int len,
                      MediaDataPacket* packet);
    void EncodePacket(opus_int16* buffer, int length,
                      MediaOutputPacket* packet);

    bool Init();
    void Release();

    void DecodePacketLoop();

    int sampling_rate_;
    OpusEncoder *encoder_;		/* Opus encoder instance */
    OpusDecoder *decoder_;		/* Opus decoder instance */

    int16_t video_seq = -1;
    int32_t video_ts = -1;
    int16_t audio_seq = -1;
    int32_t audio_ts = -1;
    int32_t fir_seq_ = 0;

    bool running_;
    RtpPacketQueue audio_queue_;  // inbuf
    boost::scoped_ptr<boost::thread> sender_thread_;
  };
}  // namespace orbit

#endif  // AUDIO_ENERGY_H__
