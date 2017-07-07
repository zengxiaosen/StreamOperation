/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * audio_mixer_test_main.cc
 * ---------------------------------------------------------------------------
 * Implements a test program to replay multiple streams of the audio packets
 * into the audio_mixer system.
 * ---------------------------------------------------------------------------
 * Usage command line:
 *   bazel-bin/stream_service/orbit/modules/audio_mixer_test_main --logtostderr --use_janus_audio_mixer=false --mixer_count=100
 */

// For Gflags and Glog
#include "gflags/gflags.h"
#include "glog/logging.h"

#include "stream_service/orbit/server/gst_util.h"
#include "audio_mixer_test.h"

DEFINE_int32(mixer_count, 10, "How many participant need to mix.");

using namespace std;

int main(int argc, char** argv) {
  google::InstallFailureSignalHandler();
  google::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);

  orbit::AudioMixerTest driver;
  return driver.run(FLAGS_mixer_count);
}
