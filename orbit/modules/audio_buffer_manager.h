/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: qingyong@orangelab.cn (vellee)
 *
 * audio_buffer_manager.h
 * ---------------------------------------------------------------------------
 * Defines the interface of a audio buffer manager class. Basically it is a audio buffer
 * and then also works as a codec wrapper (to call encoding or decoding of the audio stream).
 * ---------------------------------------------------------------------------
 * Created on : Mar  31, 2016 (vellee)
 * Modified on: June 26, 2016 (chengxu)
 */

#ifndef MODULES_AUDIO_BUFFER_MANAGER_H_
#define MODULES_AUDIO_BUFFER_MANAGER_H_

#include "media_packet.h"

#include "stream_service/orbit/rtp/rtp_packet_queue.h"
#include "stream_service/orbit/rtp/rtp_headers.h"
#include "stream_service/orbit/webrtc/modules/audio_coding/codecs/opus/opus_interface.h"
#include "stream_service/orbit/webrtc/modules/audio_coding/codecs/opus/opus_inst.h"

#include "glog/logging.h"

#include "webrtc/modules/audio_coding/neteq/include/neteq.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_header_parser.h"

#include <memory>

#define BUFFER_SAMPLES  8000
#define DEFAULT_PREBUFFERING  15

namespace orbit {

class AudioOption {
public:
  AudioOption();
  int GetChannel() { return channel_;};
  void SetChannel(int channel) { channel_ = channel; };

  int GetSamplingRate() { return sampling_rate_;};
  void SetSamplingRate(int sampling_rate) { sampling_rate_ = sampling_rate;};

  int GetComplexity() {return complexity_;};
  void SetComplexity(int complexity) { complexity_ = complexity; };
private:
  int sampling_rate_;
  int complexity_;
  int channel_;
};

class OpusCodec {
 public:
  OpusCodec(AudioOption* option);
  ~OpusCodec();

  // Encode and Decode functions.
  bool EncodePacket(const opus_int16* buffer, int len, std::shared_ptr<MediaOutputPacket> packet);
  bool DecodePacket(const unsigned char* buffer, int len, std::shared_ptr<MediaDataPacket> packet);
  /**
   * @param percent ： Between 0 and 100,default 20
   */
  void SetPacketLossPercent(int percent);

 private:
  WebRtcOpusEncInst* opus_encoder_ = NULL;  
  WebRtcOpusDecInst* opus_decoder_ = NULL;  

  AudioOption* audio_option_;
  int packet_loss_percent_; // = DEFAULT_PACKET_LOSS_PERCENT;
};

/**
 * @Class AudioBufferMananger
 * TODO decode audio packet in seperate thread.(QingyongZhang)
 * TODO config audio encoder and decoder.(QingyongZhang)
 */
class AudioBufferManager {
public:
  // NetEQ must be polled for data once every 10 ms. Thus, neither of the
  // constants below can be changed.
  static const int kTimeStepMs = 10;
  static const size_t kBlockSize8kHz = kTimeStepMs * 8;
  static const size_t kBlockSize16kHz = kTimeStepMs * 16;
  static const size_t kBlockSize32kHz = kTimeStepMs * 32;
  static const size_t kBlockSize48kHz = kTimeStepMs * 48;
  static const size_t kMaxBlockSize = kBlockSize48kHz;

  /**
   * When muted is true, we set prebuffering to false;
   */
  void Mute(bool muted){
    muted_ = muted;
  }
  bool GetMute() {
    return muted_;
  }
  /**
   * When packet len is less than the FLAGS_mute_packet_length, we think this packet
   * is muted packet, and then the value is more than FLAGS_continue_mute_packets,
   * we mute this stream.
   */
  void CheckAndSetMute(const int& packet_len, const int& stream_id);

  void PushAudioPacket(const dataPacket& packet);
  bool PopAndDecode(std::shared_ptr<MediaDataPacket>& pkt);

  bool EncodePacket(opus_int16* buffer, int length, std::shared_ptr<MediaOutputPacket> packet) {
    boost::mutex::scoped_lock lock(encode_mutex_);
    return opus_codec_->EncodePacket(buffer, length, packet);
  }

  void SetPacketLossPercent(int percent) {
    int p = percent * 2;
    if (p > 100) {
      p = 100;
    }
    opus_codec_->SetPacketLossPercent(p);
  }

  void MaybeDisplayNetEqStats();
  AudioBufferManager(AudioOption* option);
  virtual ~AudioBufferManager();
private:

  boost::mutex encode_mutex_;
  bool Init();
  // Stats related code
  std::string GetNetEqNetworkStatistics();
  std::string GetRtcpStatistics();

  // Indicates that the stream has been muted or not.
  bool muted_ = false;

  // The counter of the packets that has been inserted into the NetEQ buffer.
  int push_count_;

  // NetEq related fields.
  std::unique_ptr<webrtc::RtpHeaderParser> rtp_header_parser_;
  std::unique_ptr<webrtc::NetEq> neteq_;
  webrtc::NetEq::Config config_;

  // Codecs related fields.
  std::unique_ptr<OpusCodec> opus_codec_;

  friend class AudioMixerElement;

  // Statistic the serial mute packets, if the value is more than FLAGS_continue_mute_packets, we think this stream is muted.
  int continue_mute_packets_ = 0;
};

class AudioBufferManagerOfStable1 {
 public:
  AudioBufferManagerOfStable1(AudioOption* option);
  virtual ~AudioBufferManagerOfStable1();
  int audio_queue_size();
  /**
   * When muted is true, we set prebuffering to false;
   */
  void Mute(bool muted);
  void PushAudioPacket(const dataPacket& packet);
  std::shared_ptr<dataPacket> PopAudioPacket();
  void DecodePacket(const unsigned char* buffer, int len, std::shared_ptr<MediaDataPacket> packet);
  bool EncodePacket(opus_int16* buffer, int length, MediaOutputPacket* packet);
 private:
  bool Init();
  bool Destroy();
  bool HasPacket();

  int current_decode_index_ = -1;
  RtpPacketQueue audio_queue_;  // inbuf
  bool muted_ = false;
  bool prebuffering_ = true;

  WebRtcOpusEncInst* opus_encoder_ = NULL;
  WebRtcOpusDecInst* opus_decoder_ = NULL;

  AudioOption *audio_option_ = NULL;
};


} /* namespace orbit */

#endif /* MODULES_AUDIO_BUFFER_MANAGER_H_ */
