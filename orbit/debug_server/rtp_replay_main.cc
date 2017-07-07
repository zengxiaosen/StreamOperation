/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * videomixer_replay_main.cc
 * ---------------------------------------------------------------------------
 * Implements a test program to replay multiple streams of the captured packets
 * into the video_mixer system.
 * ---------------------------------------------------------------------------
 * Usage command line:
 *   bazel-bin/stream_service/orbit/debug_server/videomixer_replay_main --logtostderr --replay_files="./record5.pb;./record6.pb" --v=3
 */

// For Gflags and Glog
#include "gflags/gflags.h"
#include "glog/logging.h"

#include "stream_service/orbit/server/gst_util.h"
#include "packet_replay_driver.h"

DEFINE_bool(only_public_ip, false, "use public ip only on candidate.");
DEFINE_string(packet_capture_directory, "", "Specify the directory to store the capture file. e.g. /tmp/");
DEFINE_string(packet_capture_filename, "", "If any filename is specified, "
              "the rtp packet capture will be enabled and stored into the file."
              " e.g. video_rtp.pb");
DEFINE_string(test_plugin, "video_mixer", "Specifies the plugin and room. "
              "e.g. video_mixer or audio_conference ");
DECLARE_bool(video_mixer_use_record_stream);
DECLARE_bool(audio_conference_use_record_stream);
DEFINE_int32(run_times, 1, "How many times of the program to run on the replay data.");

using namespace std;

int main(int argc, char** argv) {
  google::InstallFailureSignalHandler();
  google::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);

  olive::InitGstreamer(argc,argv);

  if (FLAGS_test_plugin == "video_mixer") {
    FLAGS_video_mixer_use_record_stream = true;
  } else if (FLAGS_test_plugin == "audio_conference") {
    FLAGS_audio_conference_use_record_stream = true;
  } else {
    LOG(FATAL) << "Not implement for plugin:" << FLAGS_test_plugin;
  }

  orbit::PacketReplayDriver driver;
  return driver.RunMain(FLAGS_run_times);
}
