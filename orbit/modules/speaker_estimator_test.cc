/*
 * Copyright 2016 (C) Orange Labs Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * speaker_estimator_test.cc
 */

#include "gtest/gtest.h"
#include "glog/logging.h"

#include "speaker_estimator.h"

namespace orbit {
namespace {
class SpeakerEstimatorTest : public testing::Test {
 protected:
  virtual void SetUp() override {
  }

  virtual void TearDown() override {
  }
};

TEST_F(SpeakerEstimatorTest, BaseTest) {
  SpeakerEstimator estimator(3);
  estimator.NextFrame();

  estimator.UpdateAudioEnergy(100, 12000000);
  estimator.UpdateAudioEnergy(102, 200000);
  estimator.UpdateAudioEnergy(101, 14000000);
  estimator.NextFrame();

  estimator.UpdateAudioEnergy(100, 13000000);
  estimator.UpdateAudioEnergy(102, 300000);
  estimator.UpdateAudioEnergy(101, 14000000);
  estimator.NextFrame();

  estimator.UpdateAudioEnergy(100, 19000000);
  estimator.UpdateAudioEnergy(102, 200000);
  estimator.UpdateAudioEnergy(101, 52000000);
  estimator.NextFrame();

  SpeakerEstimator::SPEAKER_INFO infos = estimator.GetLastSpeakers();
  for (auto info : infos) {
    LOG(INFO) << info.first << ":" << info.second;
  }
  EXPECT_EQ(1, infos.size());
  EXPECT_EQ(101, infos[0].first);
  EXPECT_EQ(3, infos[0].second->max_times);
}

TEST_F(SpeakerEstimatorTest, BaseTestCase2) {
  SpeakerEstimator estimator(3);
  estimator.NextFrame();
  // frame 1
  estimator.UpdateAudioEnergy(100, 1000);
  estimator.UpdateAudioEnergy(102, 20000);
  estimator.UpdateAudioEnergy(101, 3000);
  estimator.NextFrame();
  // frame 2
  estimator.UpdateAudioEnergy(100, 5000);
  estimator.UpdateAudioEnergy(102, 2220000);
  estimator.UpdateAudioEnergy(101, 300000);
  estimator.NextFrame();
  // frame 3
  estimator.UpdateAudioEnergy(100, 19000);
  estimator.UpdateAudioEnergy(102, 500000);
  estimator.UpdateAudioEnergy(101, 2000000);
  estimator.NextFrame();
  {
    SpeakerEstimator::SPEAKER_INFO infos = estimator.GetLastSpeakers();
    for (auto info : infos) {
      LOG(INFO) << info.first << ":" << info.second->max_times;
    }
    // No one should be the Speaker since all audio energy is lower.
    EXPECT_EQ(0, infos.size());
  }

  LOG(INFO) << "next 3 frame";
  // frame 4
  estimator.UpdateAudioEnergy(100, 20000000);
  estimator.UpdateAudioEnergy(102, 500000);
  estimator.UpdateAudioEnergy(101, 2000000);
  estimator.NextFrame();
  {
    SpeakerEstimator::SPEAKER_INFO infos = estimator.GetLastSpeakers();
    for (auto info : infos) {
      LOG(INFO) << info.first << ":" << info.second->max_times;
    }
    // Though we have add one more frame, but we will still get the last speaker infomation.
    EXPECT_EQ(0, infos.size());
  }
  // frame 5
  estimator.UpdateAudioEnergy(100, 1200000);
  estimator.UpdateAudioEnergy(101, 500000);
  estimator.UpdateAudioEnergy(102, 4200000);
  estimator.NextFrame();
  {
    SpeakerEstimator::SPEAKER_INFO infos = estimator.GetLastSpeakers();
    for (auto info : infos) {
      LOG(INFO) << info.first << ":" << info.second;
    }
    // Though we have add one more frame, but we will still get the last speaker infomation.
    EXPECT_EQ(0, infos.size());
  }
  // frame 6
  estimator.UpdateAudioEnergy(100, 11000000);
  estimator.UpdateAudioEnergy(101, 500000);
  estimator.UpdateAudioEnergy(102, 45000000);
  estimator.NextFrame();
  {
    SpeakerEstimator::SPEAKER_INFO infos = estimator.GetLastSpeakers();
    for (auto info : infos) {
      LOG(INFO) << info.first << ":" << info.second->max_times << ":"<< info.second->total_energy;
    }
    // Though we have add one more frame, but we will still get the last speaker infomation.
    EXPECT_EQ(2, infos.size());
    //TODO fix test for audio energy calculate. info size is 2 max time is the same.
    EXPECT_EQ(102, estimator.CurrentSpeaker());
  }
}


}  // annoymous namespace
}  // namespace orbit
