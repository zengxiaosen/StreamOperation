/*
 * Copyright 2016 (C) Orange Labs Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * speaker_estimator.h
 * ---------------------------------------------------------------------------
 *  Use as a class to estimate the speaker in the room based on the audio
 *  energy calculated by the received audio buffer.
 * ---------------------------------------------------------------------------
 */

#pragma once

#include <vector>
#include <map>
#include <utility>      // std::pair
#include <mutex>
#include <memory>
namespace orbit {

using std::vector;
using std::map;
using std::pair;

class ISpeakerChangeListener{
public:
  virtual void OnSpeakerChanged(int speaker_id) = 0;
};

struct EnergyHolder {
  long last_max_time = 0;
  int max_times = 0;
  long total_energy = 0;
};

// Interface design:
//  * UpdateAudioEnergy()
//  * NextFrame()
//  * GetLastSpeakers()
class SpeakerEstimator {
 public:
  //const int kEstimateFrameSize = 100;
  const int kEstimateFrameSize = 50;
  typedef vector<pair<int, std::shared_ptr<EnergyHolder>> > SPEAKER_INFO;

  SpeakerEstimator();
  SpeakerEstimator(int frame_size);
  ~SpeakerEstimator();

  void UpdateAudioEnergy(int stream_id, int audio_energy);
  void NextFrame();
  int CurrentSpeaker();

  void SetSpeakerChangeListener(ISpeakerChangeListener* speaker_change_listener) {
    this->speaker_change_listener_ = speaker_change_listener;
  };

  SPEAKER_INFO GetLastSpeakers(){
    return last_speaker_info;
  }
 private:
  ISpeakerChangeListener* speaker_change_listener_ = NULL;

  // Map <stream_id, count> 
  map<int, std::shared_ptr<EnergyHolder>> speaker_counter_map_;
  vector<pair<int, int> > intra_frame_count_;

  SPEAKER_INFO last_speaker_info;
  int frame_count_;
  int defined_frame_size_;
};

}  // namespace orbit

