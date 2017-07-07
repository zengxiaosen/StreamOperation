/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: qingyong@orangelab.cn (vellee)
 *
 * audio_buffer_manager.cc
 * ---------------------------------------------------------------------------
 * Implements a audio buffer manager class. Serve as a audio buffer, and then
 * works as a codec wrapper (to call encoding or decoding of the audio stream).
 * ---------------------------------------------------------------------------
 * Created on : Mar  31, 2016 (vellee)
 * Modified on: June 26, 2016 (chengxu)
 */

#include "audio_buffer_manager.h"
#include "stream_service/orbit/rtp/rtp_headers.h"
#include "glog/logging.h"
#include "gflags/gflags.h"

#define BUFFER_SAMPLES  8000
#define DEFAULT_SAMPLE_RATE 48000
// Default complexity is 4 in Janus, in stable, it is 8.
#define DEFAULT_COMPLEXITY 4 
#define DEFAULT_PACKET_LOSS_PERCENT 20
#define NETEQ_STATS_COUNT 10000

DEFINE_int32(default_sampling_rate, DEFAULT_SAMPLE_RATE,
             "default sampling rate for audio mixer.");

DEFINE_int32(continue_mute_packets, 10, "If it has 10 muted packets continuity, we think this stream is muted.");
DEFINE_int32(mute_packet_length, 20, "We think the length of muted packet is less than this value. Reference : ptime(SDP answer) =50ms, the muted packet length is 17 bytes, and ptime = 20ms, the length is 15 bytes.");

namespace orbit {

using webrtc::NetEq;
using webrtc::WebRtcRTPHeader;
using webrtc::NetEqDecoder;
using std::endl;

AudioOption::AudioOption() {
  sampling_rate_ = FLAGS_default_sampling_rate;
  complexity_ = DEFAULT_COMPLEXITY;
  channel_ = 1;
}

OpusCodec::OpusCodec(AudioOption* option) {
  if(option == NULL) {
    audio_option_ = new AudioOption;
  } else {
    audio_option_ = option;
  }
  packet_loss_percent_ = DEFAULT_PACKET_LOSS_PERCENT;

  int16_t create = WebRtcOpus_EncoderCreate(&opus_encoder_, audio_option_->GetChannel(), 0);
  VLOG(2) << "--------Create encoder result = " << create;
  WebRtcOpus_SetComplexity(opus_encoder_, audio_option_->GetComplexity());
  WebRtcOpus_EnableFec(opus_encoder_);
  WebRtcOpus_SetPacketLossRate(opus_encoder_, 20);
  WebRtcOpus_SetBitRate(opus_encoder_, audio_option_->GetSamplingRate());

  create = WebRtcOpus_DecoderCreate(&opus_decoder_, audio_option_->GetChannel());
  VLOG(2) << "--------Create decoder result = " << create;
}

OpusCodec::~OpusCodec() {
  if(opus_encoder_ != NULL) {
    WebRtcOpus_EncoderFree(opus_encoder_);
    opus_encoder_ = NULL;
  }
  if(opus_decoder_ != NULL) {
    WebRtcOpus_DecoderFree(opus_decoder_);
    opus_decoder_ = NULL;
  }
  if (audio_option_ != NULL) {
    delete audio_option_;
    audio_option_ = NULL;
  }
}

// The caller is responsible for releasing the packet's media_buf
// NOTE: it seems useless for now. stephen marked 2016-08-01.
bool OpusCodec::DecodePacket(const unsigned char* buffer, int len,
    std::shared_ptr<MediaDataPacket> packet) {
  int decode_length;
  int16_t audio_type;
  opus_int16* tmp_buf = (opus_int16*)malloc(BUFFER_SAMPLES*sizeof(opus_int16));
  if(tmp_buf == NULL) {
    LOG(ERROR)<<"AudioBufferManager::DecodePacket malloc failed.";
    return false;
  }

  memset(tmp_buf, 0, BUFFER_SAMPLES*sizeof(opus_int16));
  decode_length = WebRtcOpus_Decode(opus_decoder_,
                                    (const unsigned char *)buffer+RTP_HEADER_BASE_SIZE,
                                    len-RTP_HEADER_BASE_SIZE,
                                    tmp_buf,
                                    &audio_type);
  VLOG(3) << "decode_length=" << decode_length;
  VLOG(3) << "audio_type=" << audio_type;

  if (decode_length < 0) {
    free(tmp_buf);
    return false;
  } else {
    packet->length = decode_length;
    packet->media_buf = tmp_buf;
  }
  return true;
}

bool OpusCodec::EncodePacket(const opus_int16* buffer, int length,
    std::shared_ptr<MediaOutputPacket> packet) {
  // Allocate the tmp_buffer for encoded packet.
  unsigned char* tmp_buf =
    (unsigned char*)malloc(length*2 * audio_option_->GetChannel());
  if (tmp_buf == NULL) {
    LOG(ERROR) << "OpusCode::EncodePacket >> malloc failed.";
    return false;
  }
  memset(tmp_buf, 0, length*2 * audio_option_->GetChannel());

  int encode_length = WebRtcOpus_Encode(opus_encoder_, buffer, length,  length*2 * audio_option_->GetChannel() , tmp_buf);

  if (encode_length < 0) {
    free(tmp_buf);
    LOG(ERROR) << "OpusCode::EncodePacket >> WebRtcOpus_Encode failed.";
    return false;
  } else {
    packet->encoded_buf = tmp_buf;
    packet->length = encode_length;
  }
  return true;
}

void OpusCodec::SetPacketLossPercent(int percent) {
  if(percent < 20){
    //ignore if percent smaller than 20%
  } else {
    packet_loss_percent_ = percent;
    WebRtcOpus_SetPacketLossRate(opus_encoder_, percent);
  }
}

//-------------------------------------------------------------------------------------------------------
//            AudioBufferManager
//-------------------------------------------------------------------------------------------------------

AudioBufferManager::AudioBufferManager(AudioOption* option) {
  opus_codec_.reset(new OpusCodec(option));
  push_count_ = 0;
  Init();
}

AudioBufferManager::~AudioBufferManager() {
}

// Statistic the muted packets, if this value is more than the FLAGS_continue_mute_packets
// we think this is muted packets, we set the flag.
void AudioBufferManager::CheckAndSetMute(const int& packet_len, const int& stream_id) {
  if (packet_len < FLAGS_mute_packet_length) {
    continue_mute_packets_ ++;
    if (continue_mute_packets_ > FLAGS_continue_mute_packets) {
      Mute(true);
    }
  } else {
    continue_mute_packets_ = 0;
    Mute(false);
  }
} 
  
void AudioBufferManager::PushAudioPacket(const dataPacket& packet) {
  WebRtcRTPHeader rtp_header;
  rtp_header_parser_->Parse((const unsigned char*)&(packet.data[0]),
                            packet.length, &(rtp_header.header));
  int ret = neteq_->InsertPacket(
      rtp_header,
      rtc::ArrayView<const uint8_t>((const unsigned char*)&(packet.data[12]),
                                    packet.length - 12),
      0);
  if (ret != 0) {
    LOG(ERROR) << "NetEQ.InsertPacket() failed.";
  } else {
    push_count_++;
  }
}

bool AudioBufferManager::PopAndDecode(std::shared_ptr<MediaDataPacket>& packet) {
  webrtc::NetEqOutputType type;
  size_t num_channels;
  size_t out_len;

  int16_t* out_data = (int16_t*)malloc(kMaxBlockSize * 5);
  if (out_data == NULL) {
    LOG(ERROR) << "AudioBufferManager::PopAndDecode malloc failed."; 
    return false;
  }
  memset(out_data, 0, kMaxBlockSize * 5);

  int ret = neteq_->GetAudio(kMaxBlockSize * 2, out_data, &out_len,
                             &num_channels, &type);
  if (ret == 0) {
    if(muted_) {
      // If muted, do not return the packet back.
      free(out_data);
      return false;
    }
    packet->length = (int)out_len * num_channels;
    packet->media_buf = (opus_int16*)out_data;
    //LOG(INFO) << "GetAudio type=" << type;
    // enum NetEqOutputType {
    //   kOutputNormal,
    //   kOutputPLC,
    //   kOutputCNG,
    //   kOutputPLCtoCNG,
    //   kOutputVADPassive
    // }
    return true;
  } else {
    free(out_data);
    LOG(ERROR) << "GetAudio() failed.";
    return false;
  }
}

bool AudioBufferManager::Init(){
  config_.enable_audio_classifier = true; // Default is false
  config_.enable_post_decode_vad = true; // Default is false
  config_.max_packets_in_buffer = 50;  // Default value is 50
  config_.max_delay_ms = 2000; // Default value is 2000
  config_.background_noise_mode = webrtc::NetEq::BackgroundNoiseMode::kBgnOff;
  config_.sample_rate_hz = 48000;
  neteq_.reset(NetEq::Create(config_));

  rtp_header_parser_.reset(webrtc::RtpHeaderParser::Create());

  //int ret = neteq_->RegisterPayloadType(NetEqDecoder::kDecoderOpus_2ch,
  //                                      "opus", 111);
  int ret = neteq_->RegisterPayloadType(NetEqDecoder::kDecoderOpus,
                                        "opus", 111);
  if (ret != 0) {
    LOG(ERROR) << "kDecoderOpus module initialization failed.";
  }
  ret = neteq_->RegisterPayloadType(NetEqDecoder::kDecoderCNGnb, "cng-nb", 13);
  if (ret != 0) {
    LOG(ERROR) << "kDecoderCNGnb module initialization failed.";
  }
  ret = neteq_->RegisterPayloadType(webrtc::NetEqDecoder::kDecoderCNGwb,
                                    "cng-wb", 105);
  if (ret != 0) {
    LOG(ERROR) << "kDecoderCNGwb module initialization failed.";
  }
  ret = neteq_->RegisterPayloadType(webrtc::NetEqDecoder::kDecoderCNGswb32kHz,
                                    "cng-swb32", 106);
  if (ret != 0) {
    LOG(ERROR) << "kDecoderCNGswb32kHz module initialization failed.";
  }
  // RegisterPayloadType(neteq, webrtc::NetEqDecoder::kDecoderCNGswb48kHz,
  //     "cng-swb48", FLAGS_cn_swb48);
  return true;
}

void AudioBufferManager::MaybeDisplayNetEqStats() {
  if (push_count_ >= NETEQ_STATS_COUNT) {
    std::string stat = GetNetEqNetworkStatistics();
    LOG(INFO) << endl
              << "--------------------------------------------------" << endl
              << "NetEq network stats:" << endl
              << stat << endl
              << "--------------------------------------------------";
    std::string rtcp_stat = GetRtcpStatistics();
    LOG(INFO) << "Rtcp stats:" << endl
              << rtcp_stat << endl << "-------------------------";
    push_count_ = 0;
  }
}

std::string AudioBufferManager::GetRtcpStatistics() {
  webrtc::RtcpStatistics rtcp_stats;
  neteq_->GetRtcpStatistics(&rtcp_stats);

  std::ostringstream oss;  
  oss << "fraction_lost:"
      << rtcp_stats.fraction_lost
      << endl;
  oss << "cumulative_lost:"
      << rtcp_stats.cumulative_lost
      << endl;
  oss << "extended_max_sequence_number:"
      << rtcp_stats.extended_max_sequence_number
      << endl;
  oss << "jitter:"
      << rtcp_stats.jitter
      << endl;
  return oss.str();
}

std::string AudioBufferManager::GetNetEqNetworkStatistics() {
  webrtc::NetEqNetworkStatistics neteq_stat;
  // NetEq function always returns zero, so we don't check the return value.
  neteq_->NetworkStatistics(&neteq_stat);

  std::ostringstream oss;
  oss << "Current jitter buffer size in ms:"
      << neteq_stat.current_buffer_size_ms
      << endl;
  oss << "Target buffer size in ms.:"
      << neteq_stat.preferred_buffer_size_ms
      << endl;
  if (neteq_stat.jitter_peaks_found) {
    oss << "jitter_peaks_found: adding extra delay due to peaky" << endl;
  } else {
    oss << "jitter_peaks_found: no" << endl;
  }
  oss << "packet_loss_rate:" << neteq_stat.packet_loss_rate << endl;
  oss << "packet_discard_rate:" << neteq_stat.packet_discard_rate << endl;
  oss << "expand_rate(synthesized audio expansion):" << neteq_stat.expand_rate << endl;
  oss << "speech_expand_rate(speech expansion):" << neteq_stat.speech_expand_rate << endl;
  oss << "preemptive_rate:" << neteq_stat.preemptive_rate << endl;
  oss << "accelerate_rate:" << neteq_stat.accelerate_rate << endl;
  oss << "secondary_decoded_rate:" << neteq_stat.secondary_decoded_rate << endl;
  oss << "clockdrift_ppm:" << neteq_stat.clockdrift_ppm << endl;
  oss << "added_zero_samples:" << neteq_stat.added_zero_samples << endl;

  oss << "mean_waiting_time_ms:" << neteq_stat.mean_waiting_time_ms << endl;
  oss << "max_waiting_time_ms:" << neteq_stat.max_waiting_time_ms << endl;

  return oss.str();
}






//-------------------------------------------------------------------------------------------------------
//            AudioBufferManagerOfStable1
//-------------------------------------------------------------------------------------------------------

AudioBufferManagerOfStable1::AudioBufferManagerOfStable1(AudioOption* option) {
  audio_option_ = option;
  Init();
}

AudioBufferManagerOfStable1::~AudioBufferManagerOfStable1() {
  if(opus_encoder_ != NULL) {
    WebRtcOpus_EncoderFree(opus_encoder_);
    opus_encoder_ = NULL;
  }
  if(opus_decoder_ != NULL) {
    WebRtcOpus_DecoderFree(opus_decoder_);
    opus_decoder_ = NULL;
  }
}

int AudioBufferManagerOfStable1::audio_queue_size() {
  return audio_queue_.getSize();
}

void AudioBufferManagerOfStable1::Mute(bool muted){
  muted_ = muted;
  prebuffering_ = !muted;
}

void AudioBufferManagerOfStable1::PushAudioPacket(const dataPacket& packet){
  if(muted_) {
    //Unqueue packet when participant is muted.
    return;
  }
  const RtpHeader* h = reinterpret_cast<const RtpHeader*>(packet.data);
  if(h->getPayloadType() == PCMU_8000_PT){
    audio_queue_.setTimebase(8000);
  } else if (h->getPayloadType() == OPUS_48000_PT) {
    audio_queue_.setTimebase(48000);
  }
  {
    audio_queue_.pushPacket((const char *)&(packet.data[0]), packet.length);
    if (audio_queue_.getSize() > DEFAULT_PREBUFFERING) {
      prebuffering_ = false;
    } else {
      prebuffering_ = true;
    }
  }
}

std::shared_ptr<dataPacket> AudioBufferManagerOfStable1::PopAudioPacket() {
  if(!HasPacket()){
    return NULL;
  }
  RtpPacketQueue* inbuf = &audio_queue_;
  std::shared_ptr<orbit::dataPacket> rtp_packet = inbuf->getNextPacket(true);
  const RtpHeader* h = reinterpret_cast<const RtpHeader*>(rtp_packet->data);
  int seq = h->getSeqNumber();
  if(current_decode_index_ - seq > 0x8000 || seq - current_decode_index_ == 1 || current_decode_index_ == -1 || seq - current_decode_index_ != 2){
    inbuf->popPacket(true);
    return rtp_packet;
  }
  return rtp_packet;
}

void AudioBufferManagerOfStable1::DecodePacket(const unsigned char* buffer, int len, std::shared_ptr<MediaDataPacket> packet) {
  const RtpHeader* h = reinterpret_cast<const RtpHeader*>(buffer);
  int current = h->getSeqNumber();
  int decode_length;
  int16_t audio_type;
  opus_int16* tmp_buf = (opus_int16*)malloc(BUFFER_SAMPLES*sizeof(opus_int16));
  if(tmp_buf == NULL) {
    LOG(ERROR)<<"AudioBufferManager::DecodePacket malloc buf result is NULL";
    packet->media_buf = NULL;
    packet->length = -1;
    return;
  }
  memset(tmp_buf, 0, BUFFER_SAMPLES*sizeof(opus_int16));
  if(current_decode_index_!= -1 && current - current_decode_index_ == 2 ){
    if(WebRtcOpus_PacketHasFec((const unsigned char *)buffer+RTP_HEADER_BASE_SIZE, len-RTP_HEADER_BASE_SIZE)){
      decode_length = WebRtcOpus_DecodeFec(opus_decoder_, (const unsigned char *)buffer+RTP_HEADER_BASE_SIZE,
                                           len-RTP_HEADER_BASE_SIZE ,tmp_buf, &audio_type);
    } else {
      decode_length = WebRtcOpus_DecodePlc(opus_decoder_, tmp_buf, 1);
    }
    current_decode_index_ ++;
  } else {
    decode_length = WebRtcOpus_Decode(opus_decoder_, (const unsigned char *)buffer+RTP_HEADER_BASE_SIZE,
                                      len-RTP_HEADER_BASE_SIZE ,tmp_buf, &audio_type);
    current_decode_index_ = current;
  }
  VLOG(2)<<decode_length;
  if (decode_length < 0) {
    packet->media_buf = NULL;
    packet->length = -1;
    free(tmp_buf);
  } else {
    packet->length = decode_length;
    packet->media_buf = tmp_buf;
  }
}

bool AudioBufferManagerOfStable1::EncodePacket(opus_int16* buffer, int length, MediaOutputPacket* packet) {
  unsigned char* tmp_buf = (unsigned char*)malloc(length*2);
  memset(tmp_buf, 0, length*2);
  int encode_length = WebRtcOpus_Encode(opus_encoder_, buffer, length,  length*2 , tmp_buf);
  if (encode_length < 0) {
    free(tmp_buf);
    packet->encoded_buf = NULL;
    packet->length = -1;
    return false;
  } else {
    packet->encoded_buf = tmp_buf;
    packet->length = encode_length;
  }
  return true;
}

bool AudioBufferManagerOfStable1::Init() {
  int16_t create = WebRtcOpus_EncoderCreate(&opus_encoder_, audio_option_->GetChannel(), 0);
  LOG(INFO)<<"--------------------------------Create encoder result = "<<create;
  WebRtcOpus_SetComplexity(opus_encoder_, audio_option_->GetComplexity());
  WebRtcOpus_EnableFec(opus_encoder_);
  WebRtcOpus_SetPacketLossRate(opus_encoder_, 20);
  WebRtcOpus_SetBitRate(opus_encoder_, audio_option_->GetSamplingRate());

  create = WebRtcOpus_DecoderCreate(&opus_decoder_, audio_option_->GetChannel());
  LOG(INFO)<<"Create decoder result = "<<create;

  return true;
}

bool AudioBufferManagerOfStable1::Destroy() {
  return true;
}

bool AudioBufferManagerOfStable1::HasPacket() {
  bool has_packet = false;
  if(!prebuffering_ && !audio_queue_.IsEmpty()){
    has_packet = true;
  }
  return has_packet;
}


} /* namespace orbit */
