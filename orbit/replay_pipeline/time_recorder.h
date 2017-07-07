/*
 * Copyright 2016 (C) Orange Labs Inc. All Rights Reserved.
 * Author: qingyong@orangelab.cn (Qingyong Zhang)
 *
 * time_recorder.h
 * ---------------------------------------------------------------------------
 * Defines the interface and functions to a util class which is used to record
 * replay events.
 * ---------------------------------------------------------------------------
 */

#pragma once

#include "stream_service/orbit/replay_pipeline/session_timer.pb.h"
#include "stream_service/orbit/base/recordio.h"
#include "stream_service/orbit/media_definitions.h"
#include "stream_service/third_party/json2pb/json2pb.h"
#include <mutex>

namespace orbit {

class TimeRecorder {
public:
  TimeRecorder(const string& export_folder ,uint32_t session_id);
  ~TimeRecorder();
  void AddStream(uint32_t stream_id, StreamType type);
  void RemoveStream(uint32_t stream_id);
  void UpdateStream(uint32_t stream_id);
  void AddEvent(uint32_t stream_id, EventType type);
  void SetSessionId(uint32_t session_id);
  string ToJson(const google::protobuf::Message &msg);
private:  
  void Destroy();
  void WriteToJsonFile();
  void WriteToPBFile();
  void UpdateSessionInfo();

  File* file_ = NULL;
  string export_folder_;
  mutable std::mutex mutex_;

  int64 start_real_time_ = 0;
  SessionTimer session_timer_;
};

}  // namespace orbit
