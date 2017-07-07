/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * echo_plugin.cc
 * ---------------------------------------------------------------------------
 * Implements the echo plugin.
 * ---------------------------------------------------------------------------
 */

#include "echo_plugin.h"
#include <thread>
#include <sys/prctl.h>

#include "stream_service/orbit/audio_processing/audio_energy.h"
#include "rtp/janus_rtcp_processor.h"
#include "stream_service/orbit/webrtc/webrtc_format_util.h"

#define USE_FEC			0
#define	BUFFER_SAMPLES	8000
#define RTP_HEADER_BASE_SIZE 12

using namespace std;
namespace orbit {
  bool EchoPlugin::Init() {
    int error = 0;
    running_ = false;

    // WARNING: those following setting should be configurable.
    sampling_rate_ = 48000;  // HACK(chengxu): for now, we set it to 48000.
    int opus_complexity = 8;  // set to default quality.

    if (encoder_ == NULL) {
      encoder_ = opus_encoder_create(sampling_rate_, 1, OPUS_APPLICATION_VOIP, &error);
      if(error != OPUS_OK) {
        LOG(ERROR) << "Error creating Opus encoder";
        encoder_ = NULL;
        return false;
      }
      if(sampling_rate_ == 8000) {
        opus_encoder_ctl(encoder_, OPUS_SET_MAX_BANDWIDTH(OPUS_BANDWIDTH_NARROWBAND));
      } else if(sampling_rate_ == 12000) {
        opus_encoder_ctl(encoder_, OPUS_SET_MAX_BANDWIDTH(OPUS_BANDWIDTH_MEDIUMBAND));
      } else if(sampling_rate_ == 16000) {
        opus_encoder_ctl(encoder_, OPUS_SET_MAX_BANDWIDTH(OPUS_BANDWIDTH_WIDEBAND));
      } else if(sampling_rate_ == 24000) {
        opus_encoder_ctl(encoder_, OPUS_SET_MAX_BANDWIDTH(OPUS_BANDWIDTH_SUPERWIDEBAND));
      } else if(sampling_rate_ == 48000) {
        opus_encoder_ctl(encoder_, OPUS_SET_MAX_BANDWIDTH(OPUS_BANDWIDTH_FULLBAND));
      } else {
        LOG(WARNING) << "Unsupported sampling rate " << sampling_rate_ << ", setting 16kHz";
        sampling_rate_ = 16000;
        opus_encoder_ctl(encoder_, OPUS_SET_MAX_BANDWIDTH(OPUS_BANDWIDTH_WIDEBAND));
      }
      opus_encoder_ctl(encoder_, OPUS_SET_INBAND_FEC(USE_FEC));
      opus_encoder_ctl(encoder_, OPUS_SET_COMPLEXITY(opus_complexity));
    }

    if (decoder_ == NULL) {
      error = 0;
      decoder_ = opus_decoder_create(sampling_rate_, 1, &error);
      if(error != OPUS_OK) {
        LOG(ERROR) << "Error creating Opus decoder";
        decoder_ = NULL;
        return false;
      }      
    }

    
    std::thread t([&, this] () {
      /* Set thread name */
      prctl(PR_SET_NAME, (unsigned long)"DecodeLoopInEcho");

      this->DecodePacketLoop();
      });
    t.detach();
    

    if (!running_) {
      running_ = true;
    }

    //sender_thread_.reset(new boost::thread(boost::bind(&EchoPlugin::DecodePacketLoop, this)));

    return true;
  }

  void EchoPlugin::Release() {
    opus_encoder_destroy(encoder_);
    encoder_ = NULL;
    opus_decoder_destroy(decoder_);
    decoder_ = NULL;
  }

  // The caller is responsible for releasing the packet's media_buf
  void EchoPlugin::DecodePacket(const unsigned char* buffer, int len,
                                MediaDataPacket* packet) {
    opus_int16* tmp_buf = (opus_int16*)malloc(BUFFER_SAMPLES*sizeof(opus_int16));
    memset(tmp_buf, 0, BUFFER_SAMPLES*sizeof(opus_int16));
    int decode_length = opus_decode(decoder_,
                                    (const unsigned char *)buffer+RTP_HEADER_BASE_SIZE,
                                    len-RTP_HEADER_BASE_SIZE,
                                    tmp_buf, BUFFER_SAMPLES, USE_FEC);
    if (decode_length < 0) {
      packet->media_buf = NULL;
      packet->length = -1;
      free(tmp_buf);
      LOG(ERROR) << "DecodePacket(opus) failed.Error Code=" << opus_strerror(decode_length);
    } else {
      packet->length = decode_length;
      packet->media_buf = tmp_buf;
      VLOG(3) << "packet->length=" << packet->length;
    }
  }

  void EchoPlugin::EncodePacket(opus_int16* buffer, int length,
                                MediaOutputPacket* packet) {
    unsigned char* tmp_buf = (unsigned char*)malloc(length*2);
    memset(tmp_buf, 0, length*2);
    int encode_length = opus_encode(encoder_, buffer, length, tmp_buf, length*2);
    if (encode_length < 0) {
      free(tmp_buf);
      packet->encoded_buf = NULL;
      packet->length = -1;
      LOG(ERROR) << "EncodePacket(opus) failed.Error Code=" << opus_strerror(encode_length);
    } else {
      VLOG(3) << "encode success=" << encode_length;
      packet->encoded_buf = tmp_buf;
      packet->length = encode_length;
    }
  }

  void EchoPlugin::RelayMediaOutputPacket(const MediaOutputPacket& packet) {
    if (packet.encoded_buf == NULL) {
      LOG(ERROR) << "OutputPacket is invalid.";
      return;
    }

    dataPacket p;
    p.comp = 0;
    p.type = AUDIO_PACKET;

    RtpHeader rtp_header;
    rtp_header.setTimestamp(packet.timestamp);
    rtp_header.setSeqNumber(packet.seq_number);
    rtp_header.setSSRC(packet.ssrc);
    rtp_header.setPayloadType(OPUS_48000_PT);
    
    memcpy(&(p.data[0]), &rtp_header, RTP_HEADER_BASE_SIZE);
    memcpy(&(p.data[RTP_HEADER_BASE_SIZE]), packet.encoded_buf, packet.length);
    p.length = packet.length + RTP_HEADER_BASE_SIZE;

    free(packet.encoded_buf);

    // Call the gateway to relay the RTP packet to the participant.
    RelayRtpPacket(p);
  }

  static int key_frame_count = 0;
  static int delta_frame_count = 0;

  void PrintParsedPayload(const webrtc::RtpDepacketizer::ParsedPayload& payload) {
    // VideoHeader
    LOG(INFO) << " width=" << payload.type.Video.width
              << " height=" << payload.type.Video.height
              << " isFirstPacket=" << payload.type.Video.isFirstPacket;

    LOG(INFO) << " partId=" << payload.type.Video.codecHeader.VP8.partitionId
              << " pictureId=" << payload.type.Video.codecHeader.VP8.pictureId
              << " beginningOfPartition=" << payload.type.Video.codecHeader.VP8.beginningOfPartition;
    LOG(INFO) << "payload=" << payload.type.Video.codec;
    LOG(INFO) << "frametype=" << payload.frame_type << " key_frame_count=" << key_frame_count << " delta_frame_count=" << delta_frame_count;
    string frame_type = "Unknown";
    switch (payload.frame_type) {
    case webrtc::kEmptyFrame: {
      frame_type = "kEmptyFrame";
      break;
    }
    case webrtc::kAudioFrameSpeech: {
      frame_type = "kAudioFrameSpeech";
      break;
    }
    case webrtc::kAudioFrameCN: {
      frame_type = "kAudioFrameCN";
      break;
    }
    case webrtc::kVideoFrameKey: {
      frame_type = "kVideoFrameKey";
      key_frame_count++;
      break;
    }
    case webrtc::kVideoFrameDelta: {
      frame_type = "kVideoFrameDelta";
      delta_frame_count++;
      break;
    }
    }
    LOG(INFO) << "frame_type=" << frame_type;
  }

  void EchoPlugin::IncomingRtpPacket(const dataPacket& packet) {
    TransportPlugin::IncomingRtpPacket(packet);
    if (packet.type == AUDIO_PACKET) {
      // Let's also take a moment to set our audio queue timebase.
      const RtpHeader* h = reinterpret_cast<const RtpHeader*>(packet.data);
      if(h->getPayloadType() == PCMU_8000_PT){
        audio_queue_.setTimebase(8000);
      } else if (h->getPayloadType() == OPUS_48000_PT) {
        audio_queue_.setTimebase(48000);
      }
      //LOG(INFO) << "h.ssrc=" << h->getSSRC() << " payload=" << (int)(h->getPayloadType());
      {
        audio_queue_.pushPacket((const char *)&(packet.data[0]), packet.length);
      }
    } else if (packet.type == VIDEO_PACKET) {
      {
        const RtpHeader* h = reinterpret_cast<const RtpHeader*>(packet.data);
        const unsigned char* buf = reinterpret_cast<const unsigned char*>(packet.data);
        int16_t _seq = h->getSeqNumber();
        int32_t _ts = h->getTimestamp();
        if (video_seq == -1) {
          video_seq = _seq;
        }
        if (video_ts == -1) {
          video_ts = _ts;
        }
        LOG(INFO) << " VideoPacket payload=" << (int)(h->getPayloadType())
                  << " ts=" << _ts - video_ts
                  << " seq=" << _seq - video_seq
                  << " size=" << packet.length;

        if (_seq % 1000 == 0) {
          // Send another FIR/PLI packet to the sender.
          /* Send a FIR to the new RTP forward publisher */
          char buf[20];
          memset(buf, 0, 20);
          janus_rtcp_fir((char *)&buf, 20, &fir_seq_);
          /* Send a PLI too, just in case... */
          memset(buf, 0, 12);
          janus_rtcp_pli((char *)&buf, 12);
        }

        const RtpHeader* rtp_header_ptr = reinterpret_cast<const RtpHeader*>(buf);
        int video_payload_type = (int)(rtp_header_ptr->getPayloadType());
        webrtc::RtpDepacketizer::ParsedPayload payload;
        bool ret = RtpDepacketizer_Parse(video_payload_type, &payload, buf + 12, packet.length - 12);
        if (!ret) {
          LOG(INFO) << "Parse the payload failed.";
        } else {
          PrintParsedPayload(payload);
        }
      }
      RelayRtcpPacket(packet);
    }
  }

  void EchoPlugin::IncomingRtcpPacket(const dataPacket& packet) {
    TransportPlugin::IncomingRtcpPacket(packet);
    if (packet.type == AUDIO_PACKET) {
      // Skip for now
    } else if (packet.type == VIDEO_PACKET) {
      const unsigned char* buf = reinterpret_cast<const unsigned char*>(packet.data);
      unsigned char* buf2 = const_cast<unsigned char*>(buf);
      char* buf3 = reinterpret_cast<char*>(buf2);
      if (janus_rtcp_has_pli(buf3, packet.length) ||
          janus_rtcp_has_fir(buf3, packet.length)) {
        LOG(INFO) << "Got PLI or FIR packet.";
      }
      RelayRtcpPacket(packet);
    }
  }

  void EchoPlugin::DecodePacketLoop() {
    VLOG(3) << "DecodePacketLoop created...";
    int samples = sampling_rate_/50;
    opus_int16 outBuffer[960], *curBuffer = NULL;
    memset(outBuffer, 0, 960*2);

    /* Timer */
    struct timeval now, before;
    gettimeofday(&before, NULL);
    now.tv_sec = before.tv_sec;
    now.tv_usec = before.tv_usec;
    time_t passed, d_s, d_us;

    /* RTP */
    int16_t seq = 0;
    int32_t ts = 0;

    /* Loop */
    int i=0;

    while(running_) {
      /* See if it's time to prepare a frame */
      gettimeofday(&now, NULL);
      d_s = now.tv_sec - before.tv_sec;
      d_us = now.tv_usec - before.tv_usec;
      if(d_us < 0) {
        d_us += 1000000;
        --d_s;
      }
      passed = d_s*1000000 + d_us;
      if(passed < 15000) {	/* Let's wait about 15ms at max */
        usleep(1000);
        continue;
      }
      /* Update the reference time */
      before.tv_usec += 20000;
      if(before.tv_usec > 1000000) {
        before.tv_sec++;
        before.tv_usec -= 1000000;
      }

      /* Update RTP header information */
      seq++;
      ts += 960;


      RtpPacketQueue* inbuf = &audio_queue_;
      if (inbuf->getSize() <= 0) {
        usleep(1000);
        continue;
      }

      std::shared_ptr<orbit::dataPacket> rtp_packet = inbuf->popPacket(true);
      VLOG(3) << "raw data size==" << rtp_packet->length;
      unsigned char* raw_rtp_data = reinterpret_cast<unsigned char*>(rtp_packet->data);

      MediaDataPacket pkt;
      DecodePacket(raw_rtp_data, rtp_packet->length, &pkt);
      curBuffer = (opus_int16 *)pkt.media_buf;
      opus_int32 _curBufferEnergy = 0;
      int scale_factor = 0;
      _curBufferEnergy = WebRtcSpl_Energy(curBuffer, samples, &scale_factor);

      {
        const RtpHeader* h = reinterpret_cast<const RtpHeader*>(raw_rtp_data);

        int16_t _seq = h->getSeqNumber();
        int32_t _ts = h->getTimestamp();
        if (audio_seq == -1) {
          audio_seq = _seq;
        }
        if (audio_ts == -1) {
          audio_ts = _ts;
        }
        LOG(INFO) << " _curBufferEnergy=" << _curBufferEnergy
                  << " ts=" << _ts - audio_ts
                  << " seq=" << _seq - audio_seq;
      }

      for(i=0; i<samples; i++) {
        outBuffer[i] = curBuffer[i];
      }

      MediaOutputPacket mixed_pkt;
      EncodePacket(outBuffer, samples, &mixed_pkt);

      mixed_pkt.timestamp = ts;
      mixed_pkt.seq_number = seq;
      mixed_pkt.ssrc = -1;
      RelayMediaOutputPacket(mixed_pkt);
    }
  }

}  // namespace orbit
