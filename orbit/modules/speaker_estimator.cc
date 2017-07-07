/*
 * Copyright 2016 (C) Orange Labs Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * speaker_estimator.cc
 * ---------------------------------------------------------------------------
 */

#include "speaker_estimator.h"

#include "glog/logging.h"

namespace orbit {
SpeakerEstimator::SpeakerEstimator() {
  frame_count_ = 0;
  defined_frame_size_ = kEstimateFrameSize;
}

SpeakerEstimator::SpeakerEstimator(int frame_size) {
  frame_count_ = 0;
  defined_frame_size_ = frame_size;
}

SpeakerEstimator::~SpeakerEstimator() {
  LOG(INFO)<<"~SpeakerEstimator";
}

int SpeakerEstimator::CurrentSpeaker() {
  int max_times_stream_id = -1;
  int max_times = 0;
  int max_total_energy = 0;
  for (auto it : last_speaker_info){
    int stream_id = it.first;
    if (max_times_stream_id == -1) {
      max_times_stream_id = stream_id;
    }
    std::shared_ptr<EnergyHolder>  holder = it.second;
    int audio_max_times = holder->max_times;
    long total_energy = holder->total_energy;
    //If max time is equal ,we update stream id to later one.
    if (audio_max_times > max_times || (audio_max_times == max_times && total_energy > max_total_energy)) {
      max_times = audio_max_times;
      max_times_stream_id = stream_id;
      max_total_energy = total_energy;
    }
  }
  return max_times_stream_id;
}

void SpeakerEstimator::UpdateAudioEnergy(int stream_id, int audio_energy) {
  intra_frame_count_.push_back(std::make_pair(stream_id, audio_energy));
}

void SpeakerEstimator::NextFrame() {
  if (intra_frame_count_.size() == 0) {
    return;
  }

  int max_stream_id = -1;
  int max_audio_energy = 0;

  for (auto counter : intra_frame_count_) {
    int stream_id = counter.first;
    int audio_energy = counter.second;
    if (audio_energy > max_audio_energy) {
      max_stream_id = stream_id;
      max_audio_energy = audio_energy;
    }
  }

  // Prepare for next frame.
  intra_frame_count_.clear();

  if (max_audio_energy > 10000000) {
    //LOG(INFO) << "max_audio_energy=" << max_audio_energy;
    //LOG(INFO) << "max_stream_id=" << max_stream_id;
    std::shared_ptr<EnergyHolder> holder;
    map<int, std::shared_ptr<EnergyHolder>>::const_iterator iter = speaker_counter_map_.find(max_stream_id);
    if (iter != speaker_counter_map_.end()) {
      holder = iter->second;
    } else {
      holder = std::make_shared<EnergyHolder>();
      speaker_counter_map_[max_stream_id] = holder;
    }
    holder->max_times = holder->max_times + 1;
    holder->total_energy = holder->total_energy + max_audio_energy;
  } else {
    // Consider as silent
  }

  frame_count_++;
  //LOG(INFO) << "frame_count=" << frame_count_ << " defined_frame_size=" << defined_frame_size_;
  if (frame_count_ >= defined_frame_size_) {
    frame_count_ = 0;
    last_speaker_info.clear();
    for (auto it : speaker_counter_map_) {
      VLOG(4) << "stream_id:" << it.first << " :" << it.second;
      last_speaker_info.push_back(std::make_pair(it.first, it.second));
    }
    if (speaker_change_listener_ != NULL) {
      int current_speaker = CurrentSpeaker();
      if (current_speaker != -1) {
        speaker_change_listener_->OnSpeakerChanged(current_speaker);
      }
    }
    speaker_counter_map_.clear();
  }

}

}  // namespace orbit
