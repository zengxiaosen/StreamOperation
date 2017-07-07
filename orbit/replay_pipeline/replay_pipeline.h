/*
 * Copyright 2016 (C) Orange Labs Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * replay_pipeline.h
 * ---------------------------------------------------------------------------
 * Defines the interface and functions to replay the RTP packets and decode the
 * videos
 * ---------------------------------------------------------------------------
 */

#pragma once
// c++ std libraries
#include <vector>
#include <string>
#include <map>
#include <memory>

#include "stream_service/orbit/sdp_info.h"
#include "stream_service/orbit/replay_pipeline/time_recorder.h"
#include "stream_service/orbit/debug_server/stored_rtp.pb.h"

namespace orbit {
  class StreamRecorderElement;

  class ReplayPipeline {
  public:
    ReplayPipeline(const string& file) {
      file_ = file;
    }
    void SetDestFolder(const string& dest_folder) {
      dest_folder_ = dest_folder;
    }
    int Run();
  private:
    void StartReplay(std::string replay_file);
    void PushPacket(StreamRecorderElement* stream_recorder,
                    std::shared_ptr<StoredPacket> pkt);
    StreamRecorderElement* GetRecorder(int transport_id,
                                       const std::string& replay_file);
    int GetVideoEncoding(int stream_id, std::string replay_file);

    // Variables.
    std::map<int, StreamRecorderElement*> recorder_pool_;
    string file_;
    string dest_folder_="";
  };  // class ReplayPipeline
}  // namespace orbit
