/*
 * audio_mixer_element.cc
 *
 *  Created on: Mar 31, 2016
 *      Author: vellee
 */

#include <time.h>
#include <stdlib.h>

#include <sys/prctl.h>
#include "audio_mixer_element.h"
#include "stream_service/orbit/rtp/rtp_packet_queue.h"
#include "stream_service/orbit/rtp/rtp_headers.h"
#include "stream_service/orbit/audio_processing/audio_energy.h"
#include "stream_service/orbit/base/timeutil.h"
#include "stream_service/orbit/base/strutil.h"
#include "stream_service/orbit/network_status.h"

#include "gflags/gflags.h"


DEFINE_bool(audio_mixer_with_multi_thread, true,
            "If set, it will use mixer method with multi thread to encode .");
DEFINE_bool(audio_mixer_use_janus,   true,
            "If set, it will use janus like mixer method or new mixer method.");
DEFINE_bool(audio_mixer_skip_low,    false,
            "If set, streams whose volume are too low will not be encoded.");
DEFINE_bool(audio_repeat,  true,
            "If set, we will send repeated audio packets when loss rate is high.");
DEFINE_bool(audio_mixer_use_stable3_loop, true,
            "If set, we will use stable3's mix loop.");
DEFINE_bool(audio_mixer_use_stable1, false,
            "If set, we will use the code of stable1 to mix audio."
            "This is set true by default.");
namespace orbit {

namespace {
class Initer {
 public:
  Initer() {
    srand(time(NULL));
  }
} init;
} // anonymous namespace

//-------------------------------------------------------------------------------------------------------
//              AudioMixerFactory
//-------------------------------------------------------------------------------------------------------

AbstractAudioMixerElement *AudioMixerFactory::Make(int32_t session_id,
                                                   AudioOption *option,
                                                   IAudioMixerRawListener *mix_all_listener,
                                                   ISpeakerChangeListener *speaker_change_listener) {
  if (FLAGS_audio_mixer_use_stable1) {
    return new AudioMixerElementOfStable1(session_id,
                                          option,
                                          mix_all_listener,
                                          speaker_change_listener);
  } else {
    return new AudioMixerElement(session_id,
                                 option,
                                 mix_all_listener,
                                 speaker_change_listener);
  }
}

//-------------------------------------------------------------------------------------------------------
//              AudioMixerElement
//-------------------------------------------------------------------------------------------------------

AudioMixerElement::AudioMixerElement(int32_t session_id,
                                     AudioOption* option,
                                     IAudioMixerRawListener* mix_all_listener,
                                     ISpeakerChangeListener* speaker_change_listener)
  : session_id_(session_id) {
  opus_codec_.reset(new OpusCodec(option));
  mix_all_listener_ = mix_all_listener;

  // Set the mixer basic samples.
  mixer_samples_ = option->GetSamplingRate() / 100;
  speaker_estimator_.reset(new SpeakerEstimator());
  if (speaker_change_listener != NULL) {
    speaker_estimator_->SetSpeakerChangeListener(speaker_change_listener);
  }
}

bool AudioMixerElement::Start() {
  if(!running_) {
    running_ = true;
    if(FLAGS_audio_mixer_with_multi_thread) {
      LOG(INFO)<<"Use multi thread";
      mixer_thread_.reset(new boost::thread([this] { this->MixPacketLoopWithMultiThread(); }));
      int cpu_count = std::thread::hardware_concurrency();
      if (cpu_count > 1) {
        cpu_count = cpu_count / 2;
      }
      for(int i = 0; i<cpu_count; i++) {
        std::shared_ptr<std::thread> thread = std::make_shared<std::thread>([this] {EncodeLoop();});
        encode_threads_.push_back(thread);
      }
    } else if(FLAGS_audio_mixer_use_janus) {
      if (FLAGS_audio_mixer_use_stable3_loop) {
        mixer_thread_.reset(new boost::thread([this] { this->MixPacketLoopOnStable3(); }));
      } else {
        mixer_thread_.reset(new boost::thread([this] { this->MixPacketLoop(); }));
        decode_thread_.reset(new boost::thread([this] { DecodeLoop(); }));
      }
    } else {
      mixer_thread_.reset(new boost::thread([this] { this->MixPacketLoop_New(); }));
    }
  }
  return true;
}

bool AudioMixerElement::RegisterMixResultListener(int stream_id, IAudioMixerRtpPacketListener* event) {
  bool registed = false;
  if(event != NULL) {
    boost::mutex::scoped_lock lock(mixer_listener_mutex_);
    audio_mixer_listeners_.insert(pair<int, IAudioMixerRtpPacketListener*>(stream_id, event));
    registed = true;
  } else {
    LOG(ERROR)<<"Invalid audio mixer listener (NULL)";
  }
  return registed;
}

bool AudioMixerElement::SetMixAllRtpPacketListener(IAudioMixerRtpPacketListener* listener){
  mix_all_rtp_packet_listener_ = listener;
  return true;
}

bool AudioMixerElement::UnregisterMixResultListener(int stream_id){
  bool removed = false;
  boost::mutex::scoped_lock lock(mixer_listener_mutex_);
  auto mixer_listener = audio_mixer_listeners_.find(stream_id);
  if(mixer_listener != audio_mixer_listeners_.end()){
    audio_mixer_listeners_.erase(mixer_listener);
    removed = true;
  }
  return removed;
}

void AudioMixerElement::AddAudioBuffer(int stream_id){
  boost::mutex::scoped_lock lock(buffer_manager_mutex_);
  std::shared_ptr<AudioBufferManager> buffer_manager = std::make_shared<AudioBufferManager>(new AudioOption());
  audio_buffer_managers_.insert(pair<int, std::shared_ptr<AudioBufferManager>>(stream_id, buffer_manager));
}

void AudioMixerElement::PushPacket(int stream_id, const dataPacket& packet){
  boost::mutex::scoped_lock lock(buffer_manager_mutex_);
  auto manager = audio_buffer_managers_.find(stream_id);
  if(manager != audio_buffer_managers_.end()){
    std::shared_ptr<AudioBufferManager> buffer_manager = (*manager).second;
    buffer_manager->PushAudioPacket(packet);
    buffer_manager->MaybeDisplayNetEqStats();

    // Check if this stream's muted packets has more than continue_mute_packets,
    // we think this stream is muted, so we set the muted flag.
    buffer_manager->CheckAndSetMute(packet.length, stream_id);
  }
}

void AudioMixerElement::OnPacketLoss(int stream_id, int percent) {
  boost::mutex::scoped_lock lock(buffer_manager_mutex_);
  auto manager = audio_buffer_managers_.find(stream_id);
  if(manager != audio_buffer_managers_.end()){
    std::shared_ptr<AudioBufferManager> buffer_manager = (*manager).second;
    buffer_manager->SetPacketLossPercent(percent);
  }
}

void AudioMixerElement::RemoveAudioBuffer(int stream_id){
  boost::mutex::scoped_lock lock(buffer_manager_mutex_);
  auto manager = audio_buffer_managers_.find(stream_id);
  if(manager != audio_buffer_managers_.end()){
    std::shared_ptr<AudioBufferManager> buffer_manager = (std::shared_ptr<AudioBufferManager>)manager->second;
    audio_buffer_managers_.erase(manager);
  }
}

bool AudioMixerElement::Mute(const int stream_id, const bool mute){
  bool success = false;
  boost::mutex::scoped_lock lock(buffer_manager_mutex_);
  auto manager = audio_buffer_managers_.find(stream_id);
  if(manager != audio_buffer_managers_.end()){
    std::shared_ptr<AudioBufferManager> buffer_manager =(std::shared_ptr<AudioBufferManager>)(manager->second);
    buffer_manager->Mute(mute);
    success = true;
  }
  return success;
}

bool AudioMixerElement::Destroy() {
  running_ = false;
  LOG(INFO) << "Joining mixer thread";
  if(mixer_thread_) {
    mixer_thread_->join();
  }
  if (decode_thread_) {
    decode_thread_->join();
  }
  vector<std::shared_ptr<std::thread>>::iterator iter;
  for (iter = encode_threads_.begin(); iter!=encode_threads_.end(); iter++)
  {
    std::shared_ptr<std::thread> thread = *iter;
    thread->join();
  }
  LOG(INFO) << "Thread terminated on destructor";
  return true;
}

AudioMixerElement::~AudioMixerElement() {
  Destroy();
  LOG(INFO)<<"~AudioMixerElement";
}

void AudioMixerElement::DecodeLoop() {
  long start_time = GetCurrentTime_MS();
  while(running_) {
    long end_time = GetCurrentTime_MS();
    if (end_time - start_time < 10) {
      usleep(1000);
      continue;
    }
    start_time = end_time;

    auto map_ptr = std::make_shared<DataMap>();

    {
      boost::mutex::scoped_lock lock(buffer_manager_mutex_);

      if (audio_buffer_managers_.empty()) {
        continue;
      }

      for (auto &pair : audio_buffer_managers_) {
        int32_t stream_id = pair.first;
        std::shared_ptr<AudioBufferManager> manager = pair.second;

        auto packet = std::make_shared<MediaDataPacket>();
        bool has_data = manager->PopAndDecode(packet);

        if (has_data && packet->length >= 0 && packet->media_buf) {
          (*map_ptr)[stream_id] = packet;
        }
      }
    }

    queue_.Add(map_ptr);
  }
}


void AudioMixerElement::EncodeLoop() {
  while (running_) {
    std::shared_ptr<UnencodedPacket> map_ptr;
    if (!unencoded_queue_.TryTake(&map_ptr)) {
      usleep(5000);
      continue;
    }

    // Encode the audio packet
    bool is_encode = false;
    std::shared_ptr<MediaOutputPacket> mixed_pkt = std::make_shared<MediaOutputPacket>();
    
    std::vector<uint32_t> stream_vec = map_ptr->stream_vec;
    for (size_t i = 0; i < stream_vec.size(); i ++) {
      long begin_proc_us = GetCurrentTime_US();

      int stream_id = stream_vec[i];
      std::shared_ptr<AudioBufferManager> buffer_manager;
      {
        boost::mutex::scoped_lock lock(buffer_manager_mutex_);
        auto iter = audio_buffer_managers_.find(stream_id);
        if (iter != audio_buffer_managers_.end()) {
          buffer_manager = iter->second;
        } else {
          continue;
        }
      }

      if (!is_encode) {
        is_encode = buffer_manager->EncodePacket(map_ptr->unencoded_buf, 480, mixed_pkt);
      }
       
      boost::mutex::scoped_lock lock(mixer_listener_mutex_);
      auto listener = audio_mixer_listeners_.find(stream_id);
      if (listener != audio_mixer_listeners_.end()) {
        if (is_encode == true) {
          mixed_pkt->timestamp = map_ptr->timestamp;
          mixed_pkt->seq_number = map_ptr->seq_number;
          mixed_pkt->ssrc = -1;
          IAudioMixerRtpPacketListener* rtp_listener = (*listener).second;
          rtp_listener->OnAudioMixed(mixed_pkt);
        }
      }
      long used = GetCurrentTime_US() - begin_proc_us;
      if (used > 5000) {
        LOG(WARNING) << "processing elapsed time=" << used << " us";
      }

    }     
  }
}

void AudioMixerElement::MixPacketLoop() {
  while (running_) {
    auto map_ptr = std::make_shared<DataMap>();
    if (!queue_.TimedTake(1000, &map_ptr)) {
      continue;
    }
    std::map<int32_t, DataPtr> decoded_packet_map;
    decoded_packet_map.swap(*map_ptr);

    long begin_proc_us = GetCurrentTime_US();
    {
      boost::mutex::scoped_lock lock(buffer_manager_mutex_);
      HandleData(decoded_packet_map);
    }
    mixer_elapsed_time_us_ = GetCurrentTime_US() - begin_proc_us;
    if (mixer_elapsed_time_us_ > 5000) {
      LOG(WARNING) << "processing elapsed time=" << mixer_elapsed_time_us_ << " us";
      LOG(WARNING) << "Thread load may have problem now.";
    }
  }
}
void AudioMixerElement::HandleData(const std::map<int32_t, DataPtr> &decoded_packet_map) {
  /* Buffer (we allocate assuming 48kHz, although we'll likely use less than that) */
  const int samples = mixer_samples_;
  opus_int32 mixall_buf[samples];
  opus_int16 outbuf[samples];

  ++seq_;
  ts_ += samples;

  /* Mix all contributions */
  for (int i = 0; i < samples; ++i) {
    mixall_buf[i] = 0;
  }

  map<int32_t, std::shared_ptr<MediaDataPacket>> packet_map;
  map<int32_t, opus_int32> energy_map;

  // 1. Skip the streams whose volume are too low.
  // 2. Hold the volumes and packets of the remaining streams.
  for (auto &pair : decoded_packet_map) {
    int32_t stream_id = pair.first;
    auto packet = pair.second;
    opus_int16 *curbuf = (opus_int16 *)packet->media_buf;

    // calculate its energy value:
    int scale_factor = 0;
    opus_int32 energy = WebRtcSpl_Energy(curbuf, samples, &scale_factor);

    if (FLAGS_audio_mixer_skip_low) {
      // If energy is low, it could be muted or low energy sound.
      if (energy < ENERGY_LOW) {
        continue;
      }
    }

    // Mix all the input voice source.
    for (int i = 0; i < samples; ++i) {
      mixall_buf[i] += curbuf[i];
    }

    packet_map[stream_id] = packet;
    energy_map[stream_id] = energy;
  }

  if (speaker_estimator_) {
    speaker_estimator_->NextFrame();
  }

  // Produce and send the response packet for every stream.

  auto sum_packet = std::make_shared<MediaOutputPacket>();
  for (auto &pair : audio_buffer_managers_) {
    int32_t stream_id = pair.first;
    std::shared_ptr<AudioBufferManager> buffer_manager = pair.second;

    opus_int16 *curbuf = nullptr;
    if (packet_map.find(stream_id) != packet_map.end()) {
      curbuf = (opus_int16 *)(packet_map[stream_id]->media_buf);
    }

    //packet_collections
    if (curbuf) {
      for (int i = 0; i < samples; ++i) {
        outbuf[i] = mixall_buf[i] - curbuf[i];
      }
    } else {
      for (int i = 0; i < samples; ++i) {
        outbuf[i] = mixall_buf[i];
      }
    }

    auto netstat = NetworkStatusManager::Get(session_id_, stream_id);
    auto audio_send_loss = netstat->GetSendFractionLost(true);
    auto target_bitrate  = netstat->GetTargetBitrate();
    auto send_bitrate    = netstat->GetBitrate(true);

    boost::mutex::scoped_lock lock(mixer_listener_mutex_);
    auto listener_iter = audio_mixer_listeners_.find(stream_id);
    if (listener_iter != audio_mixer_listeners_.end()) {
      auto mixed_pkt = std::make_shared<MediaOutputPacket>();
      bool encode_ok = false;
      if (curbuf) {
        buffer_manager->SetPacketLossPercent(audio_send_loss);
        encode_ok = buffer_manager->EncodePacket(outbuf, samples, mixed_pkt);
      } else {
        if (!sum_packet->encoded_buf) {
          buffer_manager->SetPacketLossPercent(audio_send_loss);
          encode_ok = buffer_manager->EncodePacket(outbuf, samples, sum_packet);
        }
        mixed_pkt = sum_packet;
        encode_ok = true;
      }
      if (encode_ok) {
        mixed_pkt->timestamp    = ts_;
        mixed_pkt->seq_number   = seq_;
        mixed_pkt->ssrc         = -1;
        mixed_pkt->audio_energy = energy_map[stream_id];

        IAudioMixerRtpPacketListener *rtp_listener = listener_iter->second;
        if (rtp_listener) {
          if (FLAGS_audio_repeat && seq_ % 2 == 0) {
            int repeat_count = 1;
            if (audio_send_loss > 30) {
              if (target_bitrate <= 0 || target_bitrate > send_bitrate * 1.5) {
                repeat_count = 3;
              }
            }
            for (int i = 0; i < repeat_count; ++i) {
              rtp_listener->OnAudioMixed(mixed_pkt);
            }
          } else {
            rtp_listener->OnAudioMixed(mixed_pkt);
          }
        }

        // Estimate the speaker based on the audio energy.
        if (speaker_estimator_) {
          speaker_estimator_->UpdateAudioEnergy(stream_id, mixed_pkt->audio_energy);
        }
      } else {
        LOG(ERROR) << "EncodePacket failed.";
      }
    }
  }

  if (mix_all_listener_ || mix_all_rtp_packet_listener_) {
    opus_int16 outsumbuf[samples];
    for(int i = 0; i < samples; ++i) {
      outsumbuf[i] = mixall_buf[i];
    }
    if(mix_all_listener_) {
      // mixed-audio without encode
      mix_all_listener_->OnAudioMixed(reinterpret_cast<const char*>(outsumbuf),
                                      samples * sizeof(opus_int16));
    }
    if(mix_all_rtp_packet_listener_) {
      auto mixed_pkt = std::make_shared<MediaOutputPacket>();
      if (EncodePacket(outsumbuf, samples, mixed_pkt)) {
        mixed_pkt->timestamp    = ts_;
        mixed_pkt->seq_number   = seq_;
        mixed_pkt->ssrc         = -1;
        mixed_pkt->audio_energy = energy_map.begin()->second;
        mix_all_rtp_packet_listener_->OnAudioMixed(mixed_pkt);
      } else {
        LOG(ERROR) << "EncodePacket failed.";
      }
    }
  }
}

static void mixer_audio(opus_int16* first, opus_int16* second, opus_int16* result, int samples) {
  for(int i = 0 ;i < samples ;i++) {
    opus_int32 first_value =first[i] + 65536;
    opus_int32 second_value = second[i]  + 65536;
    if(first_value< 65536 && second_value< 65536){
      result[i] = first_value*second_value/65536 - 65536;
    } else {
      result[i] = 2*(first_value+second_value) - first_value*second_value/65536 - 65536 -65536;
    }
  }
}


void AudioMixerElement::MixPacketLoop_New(){
  /* Set thread name */
  prctl(PR_SET_NAME, (unsigned long)"AudioMixLoop");

  VLOG(2) << "MixPacketLoop created...";
  /* Buffer (we allocate assuming 48kHz, although we'll likely use less than that) */
  int samples = mixer_samples_;
  opus_int16 outSumBuffer[samples];

  opus_int16 allBuffer[samples];
  memset(allBuffer, 0, samples * sizeof(opus_int16));
  memset(outSumBuffer, 0, samples * sizeof(opus_int16));

  /* RTP */
  int16_t seq = 0;
  int32_t ts = 0;

  /* Loop */
  int count = 0;
  long start_time = GetCurrentTime_MS();
  while(running_) {
    long end_time = GetCurrentTime_MS();
    if (end_time - start_time < 10) {
      usleep(1000);
      continue;
    }
    start_time = end_time;

    long begin_proc_us = GetCurrentTime_US();

    /* Do we need to mix at all? */
    boost::mutex::scoped_lock lock(buffer_manager_mutex_);
    count = audio_buffer_managers_.size();
    if(count == 0) {
    //  LOG(INFO) << "No participant...";
      /* No participant, do nothing */
      continue;
    }
    /* Update RTP header information */
    seq++;
    ts += samples;

    map<int, std::shared_ptr<MediaDataPacket>> packet_collections;
    opus_int32* audio_energy = (opus_int32 *)malloc(sizeof(opus_int32) * count);
    for (int i = 0; i < count; ++i) {
      audio_energy[i] = 0;
    }
    int participantCounter = 0;

    if (speaker_estimator_) {
      speaker_estimator_->NextFrame();
    }
    for (std::map<int, std::shared_ptr<AudioBufferManager>>::iterator it=audio_buffer_managers_.begin(); it!=audio_buffer_managers_.end(); ++it){
      std::shared_ptr<AudioBufferManager>  manager = it->second;
      std::shared_ptr<MediaDataPacket> pkt = std::make_shared<MediaDataPacket>();
      bool has_data = manager->PopAndDecode(pkt);
      if (!has_data) {
        participantCounter ++;
        continue;
      }
      if(pkt->length < 0) {
        // Decode the packet has some error.
        participantCounter ++;
        continue;
      }
      packet_collections[participantCounter] = pkt;
      participantCounter ++;
    }
    participantCounter = 0;
    for (std::map<int, std::shared_ptr<AudioBufferManager>>::iterator it=audio_buffer_managers_.begin(); it!=audio_buffer_managers_.end(); ++it){
      std::shared_ptr<AudioBufferManager> manager = it->second;
      int stream_id = it->first;
      int size = packet_collections.size();
      bool first = true;
      VLOG(2)<<" stream_id is "<< stream_id;
      for(int i = 0; i<size;i++) {
        std::shared_ptr<MediaDataPacket> packet = packet_collections[i];
        if(i == participantCounter || packet->length < 1) {
          if(i == size -1) {
            //Here we mix all packet for live stream..
            if(packet->length < 1) {
              memcpy(allBuffer,outSumBuffer,samples);
              continue;
            }
            mixer_audio(allBuffer,packet->media_buf,outSumBuffer, samples);
          }
          continue;
        }
        if(packet->length < 1) {
          continue;
        }
        if(first) {
          for(int i = 0;i < samples;i++){
            allBuffer[i] = packet->media_buf[i];
          }
          first = false;
        } else {
          mixer_audio(allBuffer,packet->media_buf,allBuffer, samples);
        }
      }

      boost::mutex::scoped_lock lock(mixer_listener_mutex_);
      auto listener = audio_mixer_listeners_.find(stream_id);
      if(listener != audio_mixer_listeners_.end()){
        bool encode_result = false;
        std::shared_ptr<MediaOutputPacket> mixed_pkt = std::make_shared<MediaOutputPacket>();
        encode_result = manager->EncodePacket(allBuffer, samples, mixed_pkt);
        if (encode_result == true) {
          mixed_pkt->timestamp = ts;
          mixed_pkt->seq_number = seq;
          mixed_pkt->ssrc = -1;
          mixed_pkt->audio_energy = audio_energy[participantCounter];
          IAudioMixerRtpPacketListener* rtp_listener = (*listener).second;
          rtp_listener->OnAudioMixed(mixed_pkt);

          // Estimate the speaker based on the audio energy.
          if (speaker_estimator_) {
            speaker_estimator_->UpdateAudioEnergy(stream_id, mixed_pkt->audio_energy);
          }
        } else {
          // encode failed
        }
      }
      participantCounter++;
    }

    //Mix all data.
    if (mix_all_listener_ || mix_all_rtp_packet_listener_) {
      if(mix_all_listener_) {
        mix_all_listener_->OnAudioMixed(reinterpret_cast<const char*>(outSumBuffer),
                                               samples*sizeof(opus_int16));
      }

      if(mix_all_rtp_packet_listener_) {
        bool encode_result = false;
        std::shared_ptr<MediaOutputPacket> mixed_pkt = std::make_shared<MediaOutputPacket>();
        encode_result = EncodePacket(outSumBuffer, samples, mixed_pkt);
        if (encode_result == true) {
          mixed_pkt->timestamp = ts;
          mixed_pkt->seq_number = seq;
          mixed_pkt->ssrc = -1;
          mixed_pkt->audio_energy = audio_energy[0];
          mix_all_rtp_packet_listener_->OnAudioMixed(mixed_pkt);
        } else {
          // encode failed
        }
      }
    }

    free(audio_energy);
    audio_energy = NULL;
    mixer_elapsed_time_us_ = GetCurrentTime_US() - begin_proc_us;
    if (mixer_elapsed_time_us_ > 5000) {
      LOG(WARNING) << "processing elapsed time=" << mixer_elapsed_time_us_ << " us";
      LOG(WARNING) << "Thread load may have problem now.";
    }
  }
}

void AudioMixerElement::MixPacketLoopOnStable3() {
  /* Set thread name */
  prctl(PR_SET_NAME, (unsigned long)"AudioMixLoopOnStable3");

  VLOG(2) << "MixPacketLoop created...";
  /* Buffer (we allocate assuming 48kHz, although we'll likely use less than that) */
  int samples = mixer_samples_;
  opus_int32 buffer[samples];
  opus_int16 outBuffer[samples], *curBuffer = NULL;
  opus_int16 outSumBuffer[samples];

  memset(buffer, 0, samples * sizeof(opus_int32));
  memset(outBuffer, 0, samples * sizeof(opus_int16));

  /* RTP */
  int16_t seq = 0;
  int32_t ts = 0;

  /* Loop */
  int i=0;
  int count = 0;
  long start_time = GetCurrentTime_MS();
  while(running_) {
    long end_time = GetCurrentTime_MS();
    if (end_time - start_time < 10) {
      usleep(1000);
      continue;
    }
    start_time = end_time;
    /* Do we need to mix at all? */
    boost::mutex::scoped_lock lock(buffer_manager_mutex_);
    count = audio_buffer_managers_.size();

    if(count == 0) {
      //  LOG(INFO) << "No participant...";
      /* No participant, do nothing */
      continue;
    }

    long begin_proc_us = GetCurrentTime_US();

    /* Update RTP header information */
    seq++;
    ts += samples;

    /* Mix all contributions */
    for(i=0; i<samples; i++) {
      buffer[i] = 0;
    }

    map<int, std::shared_ptr<MediaDataPacket>> packet_collections;
    opus_int32* audio_energy = (opus_int32 *)malloc(sizeof(opus_int32) * count);
    for (int i = 0; i < count; ++i) {
      audio_energy[i] = 0;
    }

    int participantCounter = 0;

    for (std::map<int, std::shared_ptr<AudioBufferManager>>::iterator it=audio_buffer_managers_.begin(); it!=audio_buffer_managers_.end(); ++it){
      std::shared_ptr<AudioBufferManager>  manager = it->second;
      std::shared_ptr<MediaDataPacket> pkt = std::make_shared<MediaDataPacket>();
      bool has_data = manager->PopAndDecode(pkt);
      if (!has_data) {
        participantCounter ++;
        continue;
      }
      if(pkt->length < 0) {
        // Decode the packet has some error.
        participantCounter ++;
        continue;
      }
      curBuffer = (opus_int16 *)pkt->media_buf;

      // calculate its energy value:
      opus_int32 _curBufferEnergy = 0;
      int scale_factor = 0;
      _curBufferEnergy = WebRtcSpl_Energy(curBuffer, samples, &scale_factor);
      audio_energy[participantCounter] = _curBufferEnergy;

      //Update stream audio energy
      NetworkStatusManager::UpdateTimeOfStreamHighEnergy(session_id_, it->first, _curBufferEnergy);

      VLOG(2) << "_curBufferEnergy=" << _curBufferEnergy;

      if (FLAGS_audio_mixer_skip_low) {
        // If energy is low, it could be muted or low energy sound.
        if (_curBufferEnergy < 1000000) {
          participantCounter ++;
          continue;
        }
      }

      // Mix all the input voice source.
      for(i=0; i<samples; i++) {
        buffer[i] += curBuffer[i];
      }
      packet_collections[participantCounter] = pkt;
      participantCounter++;
    }
    if (packet_collections.size() == 0) {
      continue;
    }

    participantCounter = 0;

    if (speaker_estimator_) {
      speaker_estimator_->NextFrame();
    }
    std::shared_ptr<MediaOutputPacket> sum_pkt = std::make_shared<MediaOutputPacket>();
    for (std::map<int, std::shared_ptr<AudioBufferManager>>::iterator it=audio_buffer_managers_.begin(); it!=audio_buffer_managers_.end(); ++it){
      std::shared_ptr<AudioBufferManager> buffer_manager = it->second;
      int stream_id = it->first;
      if (packet_collections.find(participantCounter) == packet_collections.end()) {
        curBuffer = NULL;
      } else {
        curBuffer = (opus_int16 *)(packet_collections[participantCounter]->media_buf);
      }
      //packet_collections
      for(i=0; i<samples; i++) {
        if (curBuffer != NULL) {
          outBuffer[i] = buffer[i]- curBuffer[i];
        } else {
          outBuffer[i] = buffer[i];
        }
      }

      boost::mutex::scoped_lock lock(mixer_listener_mutex_);
      auto listener = audio_mixer_listeners_.find(stream_id);
      if(listener != audio_mixer_listeners_.end()) {
        bool encode_result = false;
        std::shared_ptr<MediaOutputPacket> mixed_pkt = std::make_shared<MediaOutputPacket>();
        if (FLAGS_audio_mixer_skip_low) {
          if (curBuffer == NULL) {
            // The stream may be muted or very low sound.
            if (sum_pkt->encoded_buf == NULL) {
              encode_result = buffer_manager->EncodePacket(outBuffer, samples, sum_pkt);
            }
            mixed_pkt = sum_pkt;
          } else {
            encode_result = buffer_manager->EncodePacket(outBuffer, samples, mixed_pkt);
          }
        } else {
          encode_result = buffer_manager->EncodePacket(outBuffer, samples, mixed_pkt);
        }
        if (encode_result == true) {
          mixed_pkt->timestamp = ts;
          mixed_pkt->seq_number = seq;
          mixed_pkt->ssrc = -1;
          mixed_pkt->audio_energy = audio_energy[participantCounter];
          IAudioMixerRtpPacketListener* rtp_listener = (*listener).second;
          rtp_listener->OnAudioMixed(mixed_pkt);

          // Estimate the speaker based on the audio energy.
          if (speaker_estimator_) {
            speaker_estimator_->UpdateAudioEnergy(stream_id, mixed_pkt->audio_energy);
          }
        }
      }
      participantCounter++;
    }

    if (mix_all_listener_ || mix_all_rtp_packet_listener_) {
      for(i=0; i<samples; i++) {
        outSumBuffer[i] = buffer[i];
      }
      if(mix_all_listener_) {
        // mixed-audio without encode
        mix_all_listener_->OnAudioMixed(reinterpret_cast<const char*>(outSumBuffer),
                                        samples * sizeof(opus_int16));
      }
      if(mix_all_rtp_packet_listener_) {
        bool encode_result = false;
        std::shared_ptr<MediaOutputPacket> mixed_pkt = std::make_shared<MediaOutputPacket>();
        encode_result = EncodePacket(outSumBuffer, samples, mixed_pkt);
        if (encode_result == true) {
          mixed_pkt->timestamp = ts;
          mixed_pkt->seq_number = seq;
          mixed_pkt->ssrc = -1;
          mixed_pkt->audio_energy = audio_energy[0];
          mix_all_rtp_packet_listener_->OnAudioMixed(mixed_pkt);
        } else {
          // encode failed
        }
      }
    }
    free(audio_energy);
    audio_energy = NULL;

    mixer_elapsed_time_us_ = GetCurrentTime_US() - begin_proc_us;
    if (mixer_elapsed_time_us_ > 5000) {
      LOG(WARNING) << "processing elapsed time=" << mixer_elapsed_time_us_ << " us";
      LOG(WARNING) << "Thread load may have problem now.";
    }
  }
}

void AudioMixerElement::MixPacketLoopWithMultiThread() {
  /* Set thread name */
  prctl(PR_SET_NAME, (unsigned long)"MixPacketLoopWithMultiThread");

  /* Buffer (we allocate assuming 48kHz, although we'll likely use less than that) */
  int samples = mixer_samples_;
  opus_int32 buffer[samples];
  opus_int16 *curBuffer = NULL;
  opus_int16 outSumBuffer[samples];

  // Vector to keep the muted audio stream_id
  std::vector<uint32_t> vec_stream;

  memset(buffer, 0, samples * sizeof(opus_int32));

  /* RTP */
  int16_t seq = 0;
  int32_t ts = 0;

  /* Loop */
  int i=0;
  int count = 0;
  long start_time = GetCurrentTime_MS();
  long next_loop_time = start_time + 10;
  while(running_) {
    start_time = GetCurrentTime_MS();
    if (start_time < next_loop_time) {
      usleep(1000);
      continue;
    }
    next_loop_time = next_loop_time + 10;
    /* Do we need to mix at all? */
    boost::mutex::scoped_lock lock(buffer_manager_mutex_);
    count = audio_buffer_managers_.size();

    if(count == 0) {
      //  LOG(INFO) << "No participant...";
      /* No participant, do nothing */
      continue;
    }

    long begin_proc_us = GetCurrentTime_US();

    /* Mix all contributions */
    for(i=0; i<samples; i++) {
      buffer[i] = 0;
    }

    map<int, std::shared_ptr<MediaDataPacket>> packet_collections;
    opus_int32* audio_energy = (opus_int32 *)malloc(sizeof(opus_int32) * count);
    for (int i = 0; i < count; ++i) {
      audio_energy[i] = 0;
    }
    int participantCounter = 0;

    // Clear vec_stream
    vec_stream.clear();

    // Add all participants audio buffer into buffer array for every participant.
    for (std::map<int, std::shared_ptr<AudioBufferManager>>::iterator it=audio_buffer_managers_.begin(); it!=audio_buffer_managers_.end(); ++it){
      std::shared_ptr<AudioBufferManager>  manager = it->second;
      std::shared_ptr<MediaDataPacket> pkt = std::make_shared<MediaDataPacket>();

      // the stream is muted stream
      if (manager->GetMute()) {
        int stream_id = it->first;
        vec_stream.push_back(stream_id);
      }

      bool has_data = manager->PopAndDecode(pkt);
      if (!has_data) {
        participantCounter ++;
        continue;
      }
      if(pkt->length < 0) {
        // Decode the packet has some error.
        participantCounter ++;
        continue;
      }
      curBuffer = (opus_int16 *)pkt->media_buf;

      // calculate its energy value:
      opus_int32 _curBufferEnergy = 0;
      int scale_factor = 0;
      _curBufferEnergy = WebRtcSpl_Energy(curBuffer, samples, &scale_factor);
      audio_energy[participantCounter] = _curBufferEnergy;

      //Update stream audio energy
      NetworkStatusManager::UpdateTimeOfStreamHighEnergy(session_id_, it->first, _curBufferEnergy);

      if (FLAGS_audio_mixer_skip_low) {
        // If energy is low, it could be muted or low energy sound.
        if (_curBufferEnergy < 1000000) {
          participantCounter ++;
          continue;
        }
      }

      // Mix all the input voice source.
      for(i=0; i<samples; i++) {
        buffer[i] += curBuffer[i];
      }
      packet_collections[participantCounter] = pkt;
      participantCounter++;
    }
    
    if (packet_collections.size() == 0) {
      free(audio_energy);
      continue;
    }

    /* Update RTP header information */
    seq++;
    ts += samples;

    participantCounter = 0;

    if (speaker_estimator_) {
      speaker_estimator_->NextFrame();
    }
    std::shared_ptr<MediaOutputPacket> sum_pkt = std::make_shared<MediaOutputPacket>();

    // We just send packet to mute streams once.
    bool mute_stream_send = false;
    for (std::map<int, std::shared_ptr<AudioBufferManager>>::iterator it=audio_buffer_managers_.begin(); it!=audio_buffer_managers_.end(); ++it){
      int stream_id = it->first;
      std::vector<uint32_t> streams;

      // If the stream_id is in vec_stream, we just encode once. Because the streams
      // in vec_stream is the muted stream, so we just send the same encode packet.
      auto ite_stream = find(vec_stream.begin(), vec_stream.end(), stream_id);
      if (ite_stream != vec_stream.end()) {
        if (mute_stream_send) {
          participantCounter ++;
          continue;
        } else {
          mute_stream_send = true;
          streams = vec_stream;
        }
      } else {
        streams.push_back(stream_id);
      }
      
      if (packet_collections.find(participantCounter) == packet_collections.end()) {
        curBuffer = NULL;
      } else {
        curBuffer = (opus_int16 *)(packet_collections[participantCounter]->media_buf);
      }

      opus_int16* tmp_buf = (opus_int16*)malloc(samples * sizeof(opus_int16));
      //packet_collections
      for(i=0; i<samples; i++) {
        if (curBuffer != NULL) {
          tmp_buf[i] = buffer[i]- curBuffer[i];
        } else {
          tmp_buf[i] = buffer[i];
        }
      }

      {
        boost::mutex::scoped_lock lock(mixer_listener_mutex_);
        auto listener = audio_mixer_listeners_.find(stream_id);
        if(listener != audio_mixer_listeners_.end()) {
          std::shared_ptr<UnencodedPacket> unencoded_pkt = std::make_shared<UnencodedPacket>();
          unencoded_pkt->stream_vec = streams;
          unencoded_pkt->timestamp = ts;
          unencoded_pkt->seq_number = seq;
          unencoded_pkt->ssrc = -1;
          unencoded_pkt->unencoded_buf = tmp_buf;
          unencoded_queue_.Add(unencoded_pkt);
          if (speaker_estimator_) {
            speaker_estimator_->UpdateAudioEnergy(stream_id, audio_energy[participantCounter]);
          }
        } else {
          free(tmp_buf);
        }
        participantCounter++;
      }

    }

    if (mix_all_listener_ || mix_all_rtp_packet_listener_) {
      for(i=0; i<samples; i++) {
        outSumBuffer[i] = buffer[i];
      }
      if(mix_all_listener_) {
        // mixed-audio without encode
        mix_all_listener_->OnAudioMixed(reinterpret_cast<const char*>(outSumBuffer),
                                        samples * sizeof(opus_int16));
      }
      if(mix_all_rtp_packet_listener_) {
        bool encode_result = false;
        std::shared_ptr<MediaOutputPacket> mixed_pkt = std::make_shared<MediaOutputPacket>();
        encode_result = EncodePacket(outSumBuffer, samples, mixed_pkt);
        if (encode_result == true) {
          mixed_pkt->timestamp = ts;
          mixed_pkt->seq_number = seq;
          mixed_pkt->ssrc = -1;
          mixed_pkt->audio_energy = audio_energy[0];
          mix_all_rtp_packet_listener_->OnAudioMixed(mixed_pkt);
        } else {
          // encode failed
        }
      }
    }
    free(audio_energy);
    audio_energy = NULL;

    mixer_elapsed_time_us_ = GetCurrentTime_US() - begin_proc_us;
    if (mixer_elapsed_time_us_ > 5000) {
      LOG(WARNING) << "processing elapsed time=" << mixer_elapsed_time_us_ << " us";
      LOG(WARNING) << "Thread load may have problem now.";
    }
  }
}
//-------------------------------------------------------------------------------------------------------
//              AudioMixerElementOfStable1
//-------------------------------------------------------------------------------------------------------

AudioMixerElementOfStable1::AudioMixerElementOfStable1(
    int32_t session_id,
    AudioOption *option,
    IAudioMixerRawListener *mix_all_listener,
    ISpeakerChangeListener *speaker_change_listener)
: session_id_(session_id) {
  if (option) {
    audio_option_ = option;
  } else {
    audio_option_ = new AudioOption;
  }
  mix_all_listener_ = mix_all_listener;
  if (speaker_change_listener) {
    speaker_estimator_.SetSpeakerChangeListener(speaker_change_listener);
  }
}

bool AudioMixerElementOfStable1::Init() {
  int16_t create = WebRtcOpus_EncoderCreate(&opus_encoder_, audio_option_->GetChannel(), 0);
  LOG(INFO) << "------------------------Create encoder result = " << create;
  WebRtcOpus_SetComplexity(opus_encoder_, audio_option_->GetComplexity());
  WebRtcOpus_EnableFec(opus_encoder_);
  WebRtcOpus_SetPacketLossRate(opus_encoder_, 20);
  WebRtcOpus_SetBitRate(opus_encoder_, audio_option_->GetSamplingRate());
  return true;
}

AudioMixerElementOfStable1::~AudioMixerElementOfStable1() {
  if(opus_encoder_ != NULL) {
    WebRtcOpus_EncoderFree(opus_encoder_);
    opus_encoder_ = NULL;
  }
  if(audio_option_) {
    delete audio_option_;
    audio_option_ = NULL;
  }
  running_ = false;
  if (mixer_thread_.get()!=NULL) {
    mixer_thread_->join();
  }
}

bool AudioMixerElementOfStable1::Start() {
  if (!running_) {
    running_ = true;
    mixer_thread_.reset(new boost::thread([this]{MixPacketLoop();}));
  }
  return true;
}

bool AudioMixerElementOfStable1::IsStarted() {
  return running_;
}

void AudioMixerElementOfStable1::AddAudioBuffer(int stream_id) {
  boost::mutex::scoped_lock lock(buffer_manager_mutex_);
  audio_buffer_managers_.insert({stream_id, new AudioBufferManagerOfStable1(audio_option_)});
}

void AudioMixerElementOfStable1::RemoveAudioBuffer(int stream_id) {
  boost::mutex::scoped_lock lock(buffer_manager_mutex_);
  auto iter = audio_buffer_managers_.find(stream_id);
  if (iter != audio_buffer_managers_.end()) {
    AudioBufferManagerOfStable1 *p = iter->second;
    delete p;
    p = nullptr;
  }
  audio_buffer_managers_.erase(stream_id);
}

void AudioMixerElementOfStable1::PushPacket(int stream_id, const dataPacket &packet) {
  boost::mutex::scoped_lock lock(buffer_manager_mutex_);
  auto iter = audio_buffer_managers_.find(stream_id);
  if (iter == audio_buffer_managers_.end()) {
    return;
  }
  AudioBufferManagerOfStable1 *buffer_manager = iter->second;
  if (!buffer_manager) {
    return;
  }
  buffer_manager->PushAudioPacket(packet);
}

void AudioMixerElementOfStable1::OnPacketLoss(int stream_id, int percent) {
  // do nothing.
}

bool AudioMixerElementOfStable1::RegisterMixResultListener(int stream_id, IAudioMixerRtpPacketListener *listener) {
  if (!listener) {
    return false;
  }
  boost::mutex::scoped_lock lock(mixer_listener_mutex_);
  audio_mixer_listeners_.insert({stream_id, listener});
  return true;
}

bool AudioMixerElementOfStable1::UnregisterMixResultListener(int stream_id) {
  boost::mutex::scoped_lock lock(mixer_listener_mutex_);
  audio_mixer_listeners_.erase(stream_id);
  return true;
}

bool AudioMixerElementOfStable1::SetMixAllRtpPacketListener(IAudioMixerRtpPacketListener *listener) {
  mix_all_rtp_packet_listener_ = listener;
  return true;
}

bool AudioMixerElementOfStable1::Mute(const int stream_id, const bool mute) {
  boost::mutex::scoped_lock lock(buffer_manager_mutex_);
  auto iter = audio_buffer_managers_.find(stream_id);
  if (iter == audio_buffer_managers_.end()) {
    return false;
  }
  AudioBufferManagerOfStable1 *buffer_manager = iter->second;
  if (!buffer_manager) {
    return false;
  }
  buffer_manager->Mute(mute);
  return true;
}

void AudioMixerElementOfStable1::MixPacketLoop() {
  /* Set thread name */
  prctl(PR_SET_NAME, (unsigned long)"AudioMixLoop");

  VLOG(2) << "MixPacketLoop created...";
  /* Buffer (we allocate assuming 48kHz, although we'll likely use less than that) */
  int samples = audio_option_->GetSamplingRate()/50;
  opus_int32 buffer[samples];
  opus_int16 outBuffer[samples], *curBuffer = NULL;
  opus_int16 outSumBuffer[samples];

  memset(buffer, 0, sizeof(buffer));
  memset(outBuffer, 0, sizeof(outBuffer));

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
  int count = 0, prev_count = 0;
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
    if(passed < 15000) {  /* Let's wait about 15ms at max */
      usleep(1000);
      continue;
    }
    /* Update the reference time */
    before.tv_usec += 20000;
    if(before.tv_usec > 1000000) {
      before.tv_sec++;
      before.tv_usec -= 1000000;
    }

    /* Do we need to mix at all? */
    boost::mutex::scoped_lock lock(buffer_manager_mutex_);
    count = audio_buffer_managers_.size();

    if(count == 0) {
      //  LOG(INFO) << "No participant...";
      /* No participant, do nothing */
      if(prev_count > 0) {
        prev_count = 0;
      }
      continue;
    }
    prev_count = count;
    /* Update RTP header information */
    seq++;
    ts += samples;

    /* Mix all contributions */
    for(i=0; i<samples; i++) {
      buffer[i] = 0;
    }

    map<int, std::shared_ptr<MediaDataPacket>> packet_collections;
    opus_int32* audio_energy = (opus_int32 *)malloc(sizeof(opus_int32) * count);
    for (int i = 0; i < count; ++i) {
      audio_energy[i] = 0;
    }

    int participantCounter = 0;

    for (auto it=audio_buffer_managers_.begin(); it!=audio_buffer_managers_.end(); ++it){
      AudioBufferManagerOfStable1 *manager = it->second;
      auto rtp_packet = manager->PopAudioPacket();
      if(rtp_packet == NULL){
        participantCounter ++;
        continue;
      }
      VLOG(2) << "raw data size==" << rtp_packet->length << " stream id is "<<it->first;
      VLOG(2) << "packet size is "<<rtp_packet->length;
      unsigned char* raw_rtp_data = reinterpret_cast<unsigned char*>(rtp_packet->data);
      auto pkt = std::make_shared<MediaDataPacket>();
      manager->DecodePacket(raw_rtp_data, rtp_packet->length, pkt);
      if(pkt->length < 0) {
        // Decode the packet has some error.
        participantCounter ++;
        continue;
      }
      VLOG(2) << "Decoded length = "<<pkt->length;
      curBuffer = (opus_int16 *)pkt->media_buf;

      // calculate its energy value:
      opus_int32 _curBufferEnergy = 0;
      int scale_factor = 0;
      _curBufferEnergy = WebRtcSpl_Energy(curBuffer, samples, &scale_factor);
      audio_energy[participantCounter] = _curBufferEnergy;

      //Update stream audio energy
      NetworkStatusManager::UpdateTimeOfStreamHighEnergy(session_id_, it->first, _curBufferEnergy);

      VLOG(2) << "_curBufferEnergy=" << _curBufferEnergy;

      // Mix all the input voice source.
      for(i=0; i<samples; i++) {
        buffer[i] += curBuffer[i];
      }
      packet_collections[participantCounter] = pkt;
      participantCounter++;
    }
    if (packet_collections.size() == 0) {
      continue;
    }

    participantCounter = 0;

    speaker_estimator_.NextFrame();

    for (auto it=audio_buffer_managers_.begin(); it!=audio_buffer_managers_.end(); ++it){
      AudioBufferManagerOfStable1 *buffer_manager = it->second;
      int stream_id = it->first;
      if (packet_collections.find(participantCounter) == packet_collections.end()) {
        curBuffer = NULL;
      } else {
        curBuffer = (opus_int16 *)(packet_collections[participantCounter]->media_buf);
      }
      //packet_collections
      for(i=0; i<samples; i++) {
        if (curBuffer != NULL) {
          outBuffer[i] = buffer[i]- curBuffer[i];
        } else {
          outBuffer[i] = buffer[i];
        }
      }

      boost::mutex::scoped_lock lock(mixer_listener_mutex_);
      auto listener = audio_mixer_listeners_.find(stream_id);
      if(listener != audio_mixer_listeners_.end()){
        auto mixed_pkt = std::make_shared<MediaOutputPacket>();
        buffer_manager->EncodePacket(outBuffer, samples, mixed_pkt.get());
        mixed_pkt->timestamp = ts;
        mixed_pkt->seq_number = seq;
        mixed_pkt->ssrc = -1;
        mixed_pkt->audio_energy = audio_energy[participantCounter];
        IAudioMixerRtpPacketListener* rtp_listener = (*listener).second;
        rtp_listener->OnAudioMixed(mixed_pkt);

        // Estimate the speaker based on the audio energy.
        speaker_estimator_.UpdateAudioEnergy(stream_id, mixed_pkt->audio_energy);
      }
      participantCounter++;
    }

    if (mix_all_listener_ || mix_all_rtp_packet_listener_) {
      for(i=0; i<samples; i++) {
        outSumBuffer[i] = buffer[i];
      }
      if(mix_all_listener_) {
        mix_all_listener_->OnAudioMixed(reinterpret_cast<const char*>(outSumBuffer),
                                        samples*sizeof(opus_int16));
      }
      if(mix_all_rtp_packet_listener_) {
        auto mixed_pkt = std::make_shared<MediaOutputPacket>();
        EncodePacket(outSumBuffer, samples, mixed_pkt);
        mixed_pkt->timestamp = ts;
        mixed_pkt->seq_number = seq;
        LOG(INFO)<<"Seq number = "<<seq;
        mixed_pkt->ssrc = -1;
        mixed_pkt->audio_energy = audio_energy[0];
        mix_all_rtp_packet_listener_->OnAudioMixed(mixed_pkt);
      }
    }
    free(audio_energy);
    audio_energy = NULL;
  }
}

bool AudioMixerElementOfStable1::EncodePacket(opus_int16 *buffer, int length,
                                              std::shared_ptr<MediaOutputPacket> packet) {
  unsigned char* tmp_buf = (unsigned char*)malloc(length*2);
  memset(tmp_buf, 0, length*2);
  int encode_length = WebRtcOpus_Encode(opus_encoder_, buffer, length,  length*2 , tmp_buf);
  if (encode_length < 0) {
    free(tmp_buf);
    packet->encoded_buf = NULL;
    packet->length = -1;
    LOG(ERROR) << "EncodePacket(opus) failed.Error Code=" << opus_strerror(encode_length);
    return false;
  } else {
    packet->encoded_buf = tmp_buf;
    packet->length = encode_length;
  }
  return true;
}

} /* namespace orbit */
