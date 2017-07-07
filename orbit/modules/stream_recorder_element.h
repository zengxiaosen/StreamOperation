/*
 * Copyright © 2016 Orangelab Inc. All Rights Reserved.
 * 
 * Author: qingyong@orangelab.cn (Qingyong Zhang)
 * Created on: Apr 25, 2016
 *
 *
 * stream_recorder_element.h
 * --------------------------------------------------------
 * Element used for record audio and video stream.
 * --------------------------------------------------------
 */

#ifndef MODULES_STREAM_RECORDER_ELEMENT_H_
#define MODULES_STREAM_RECORDER_ELEMENT_H_

#include "stream_service/orbit/modules/rtp_packet_buffer.h"
#include "stream_service/orbit/rtp/rtp_headers.h"
#include "stream_service/orbit/media_definitions.h"
#include "stream_service/orbit/modules/media_packet.h"
#include "stream_service/orbit/rtp/rtp_format_util.h"
#include "glog/logging.h"

#include "gflags/gflags.h"
#include "gst/gst.h"
#include "gst/app/gstappsrc.h"

#include <string>

namespace orbit {
class StreamRecorderElement {
public:
  StreamRecorderElement(const std::string& file_name, int video_encoding = VP8_90000_PT);
  void Start();
  bool IsStoped() { return !running_; };
  virtual ~StreamRecorderElement();

  std::shared_ptr<dataPacket> PopVideoPacket() {
    if(running_) {
      std::shared_ptr<dataPacket> data_packet = video_queue_->PopPacket();
      if (data_packet == NULL) {
        return data_packet;
      }
      const RtpHeader* h = reinterpret_cast<const RtpHeader*>(data_packet->data);
      uint16_t seq = h->getSeqNumber();
      if (last_video_seq_ == -1) {
        last_video_seq_ = seq;
        return data_packet;
      }
      if (seq > last_video_seq_) {
        last_video_seq_ = seq;
        return data_packet;
      }
      if (seq < last_video_seq_ && abs(seq - last_video_seq_) > 8000) {
        last_video_seq_ = seq;
        return data_packet;
      } else {
        LOG(INFO) << "Lost video late sample seq=" << seq;
        return NULL;
      }
    }
  }

  std::shared_ptr<dataPacket> PopAudioPacket() {
    std::shared_ptr<dataPacket> data_packet = audio_queue_->PopPacket();
    if (data_packet == NULL) {
      return data_packet;
    }
    const RtpHeader* h = reinterpret_cast<const RtpHeader*>(data_packet->data);
    uint16_t seq = h->getSeqNumber();
    if (last_audio_seq_ == -1) {
      last_audio_seq_ = seq;
      return data_packet;
    }
    if (seq > last_audio_seq_) {
      last_audio_seq_ = seq;
      return data_packet;
    }
    if (seq < last_audio_seq_ && abs(seq - last_audio_seq_) > 8000) {
      last_audio_seq_ = seq;
      return data_packet;
    } else {
      LOG(INFO) << "Lost audio late sample seq=" << seq;
      return NULL;
    }
  }

  void PushVideoPacket(const dataPacket& packet);

  void PushAudioPacket(const dataPacket& packet);

  void RelayMediaOutputPacket(const std::shared_ptr<MediaOutputPacket> packet, packetType packet_type);

  GstElement* GetVideoSrc() {
    return video_src_;
  }
  GstElement* GetAudioSrc() {
    return audio_src_;
  }
  GstElement* GetPipeline() {
    return pipeline_;
  }
private:
  long audio_start_time_ = 0;
  long audio_delay_time_ = 0;
  long audio_ntp_start_time_ = 0;

  long video_start_time_ = 0;
  long video_delay_time_ = 0;
  long video_ntp_start_time_ = 0;
  bool running_ = false;
  bool video_queue_data_ = false;

  int video_encoding_;
  
  void SetupAudioElements_webm();
  void SetupVideoElements_webm();
  void SetupRecorderElements_webm(const std::string& file_location);

  void SetupAudioElements_mp4();
  void SetupVideoElements_mp4();
  void SetupRecorderElements_mp4(const std::string& file_location);
  void FlushData();
  void Destroy();

  void _PushAudioPacket(std::shared_ptr<dataPacket> dataPacket);
  void _PushVideoPacket(std::shared_ptr<dataPacket> dataPacket);

  std::shared_ptr<RtpPacketBuffer> video_queue_;  // inbuf
  std::shared_ptr<RtpPacketBuffer> audio_queue_;  // inbuf
  int last_video_seq_ = -1;
  int last_audio_seq_ = -1;

  char* save_path_;
  GstElement* file_sink_;
  GstElement* muxer_;

  GstElement* audio_src_ = NULL;
  GstElement* audio_jitter_buffer_;
  GstElement* rtp_opus_depay_;

  GstElement* video_src_;
  GstElement* video_jitter_buffer_;
  GstElement* rtp_depay_;

  GstElement* pipeline_;
};

} /* namespace orbit */

#endif /* MODULES_STREAM_RECORDER_ELEMENT_H_ */
