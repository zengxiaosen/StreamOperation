/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * replay_pipeline_main.cc
 * ---------------------------------------------------------------------------
 * The main pipeline for replaying the video packets into a single file.
 * ---------------------------------------------------------------------------
 * Example usage:
 *  bazel-bin/stream_service/orbit/replay_pipeline/replay_pipeline_main \
 *   --replay_files=/tmp/orbit_data/1780661719.pb --record_format=mp4 \
 *   --logtostderr
 */

// For Gflags and Glog
#include "gflags/gflags.h"
#include "glog/logging.h"
#include <gstreamer-1.5/gst/gst.h>

#include "replay_pipeline.h"

DEFINE_bool(save_dot_file, false, "Used to determine  whether  need to save pipeline to dot file. Default is false.");
DEFINE_string(replay_files, "", "Specifies multiple replay pb files, using"
              " the delimiter to separete them.. e.g. video_rtp1.pb;video_rtp2.pb");
using namespace std;

int main(int argc, char** argv) {
  google::InstallFailureSignalHandler();
  google::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);

  gst_init (&argc, &argv);

  orbit::ReplayPipeline* pipeline = new orbit::ReplayPipeline(FLAGS_replay_files);
  if(1 == pipeline->Run()) {
    return 0;
  } else {
    return 1; 
  }
}
