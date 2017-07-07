/*
 * Copyright 2016 (C) Orange Labs Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * packet_replay_driver.h
 * ---------------------------------------------------------------------------
 * Defines the interface and functions to a replay driver class. The replay driver
 * class is to simulate the RTP transportion process to the pipeline and then
 * replay the packets into the system.
 * ---------------------------------------------------------------------------
 */
#pragma once
// c++ std libraries
#include <vector>
#include <string>
#include <map>
#include <memory>

#include "stream_service/orbit/sdp_info.h"

namespace orbit {
  // Forward declaration
  class Room;
  class ReplayTransportDelegate;

  class PacketReplayDriver {
  public:
    void SetupRoom(int session_id);
    void CleanRoom();
    int RunReplayPipeline();

    ReplayTransportDelegate* SetupTransportDelegate(int session_id,
                                                    int stream_id,
                                                    int transport_id);
    ReplayTransportDelegate* GetTransportDelegate(int session_id,
                                                  int transport_id);

    void StartReplay(int session_id, std::string replay_file);
    void PrepareReplay(int session_id, std::string replay_file);

    int RunMain(int run_times);
  private:
    std::shared_ptr<Room> audio_video_room_;
    std::string plugin_name_;

    std::map<int, ReplayTransportDelegate*> delegate_pool_;
    std::map<int, SdpInfo> sdp_infos_;

    bool is_ready_;
    bool has_participants_;
    int times_;
  };  // class PacketReplayDriver
}  // namespace orbit

