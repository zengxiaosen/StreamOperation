/*
 * Copyright 2016 (C) Orange Labs Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * rtp_capture.h
 * ---------------------------------------------------------------------------
 * Impletes util class which is used to capture and replay the RTP packets.
 * ---------------------------------------------------------------------------
 */

#include "time_recorder.h"
#include "glog/logging.h"

#include "stream_service/orbit/rtp/rtp_headers.h"
#include "stream_service/orbit/base/timeutil.h"
#include "stream_service/orbit/base/session_info.h"

namespace orbit {
TimeRecorder::TimeRecorder(const string& export_folder, uint32_t session_id) {
  start_real_time_ = GetCurrentTime_MS();
  session_timer_.set_start_time(start_real_time_);
  export_folder_ = export_folder;
  SetSessionId(session_id);
}


TimeRecorder::~TimeRecorder() {
  std::lock_guard < std::mutex > lock(mutex_);
  WriteToJsonFile();
  Destroy();
}

void TimeRecorder::Destroy() {
}

void TimeRecorder::RemoveStream(uint32_t stream_id) {

  std::lock_guard < std::mutex > lock(mutex_);
  VLOG(10)<<"===============================================RemoveStream";
  /*
   * 1. Check already exist stream.
   */
  int size = session_timer_.stream_items_size();
  for (int i = 0; i < size; i++) {
    StreamItem* item = session_timer_.mutable_stream_items(i);
    if (item->stream_id() == stream_id) {
      /*
       * 2.Add levave event.
       */
      StreamEvent* event = item->add_stream_events();
      event->set_event_type(LEAVE);
      event->set_event_time(GetCurrentTime_MS() - start_real_time_);
      break;
    }
  }
}

void TimeRecorder::AddStream(uint32_t stream_id, StreamType type) {
  VLOG(10)<<"=====Add Stream";
  /*
   * 1. Check already exist stream.
   */
  std::lock_guard < std::mutex > lock(mutex_);
  int size = session_timer_.stream_items_size();
  bool already_exist = false;
  for (int i = 0; i < size; i++) {
    StreamItem item = session_timer_.stream_items(i);
    if (item.stream_id() == stream_id) {
      already_exist = true;
      break;
    }
  }
  /*
   * 2.Insert stream item and add start&leave event to current stream.
   */
  if (!already_exist) {
    StreamItem* stream_item = session_timer_.add_stream_items();
    stream_item->set_stream_id(stream_id);
    stream_item->set_stream_type(type);

    StreamEvent* enter_event = stream_item->add_stream_events();
    enter_event->set_event_type(ENTER);
    int64 time = GetCurrentTime_MS() - start_real_time_;
    enter_event->set_event_time(time);

    StreamEvent* start_event = stream_item->add_stream_events();
    start_event->set_event_type(START);

    StreamEvent* stop_event = stream_item->add_stream_events();
    stop_event->set_event_type(STOP);
  }
}
void TimeRecorder::UpdateStream(uint32_t stream_id) {
  std::lock_guard < std::mutex > lock(mutex_);
  int size = session_timer_.stream_items_size();
  /*
   * 1. Check already exist stream.
   */
  for (int i = 0; i < size; i++) {
    StreamItem* item = session_timer_.mutable_stream_items(i);
    if (item->stream_id() == stream_id) {
      int event_size = item->stream_events_size();
      VLOG(10)<<"Size is "<<event_size;
      for (int j = 0; j < event_size; j++) {
        StreamEvent* event = item->mutable_stream_events(j);
        VLOG(10)<<"Event type is "<<j <<" = "<<event->event_type()
            << " is start = "<<(event->event_type() == START)
            << " is stop = "<<(event->event_type() == STOP);
        if (event->event_type() == START && event->event_time() == 0) {
          int64 time = GetCurrentTime_MS() - start_real_time_;
          event->set_event_time(time);
        } else if (event->event_type() == STOP) {
          int64 time = GetCurrentTime_MS() - start_real_time_;
          event->set_event_time(time);
          VLOG(10)<<"Destroy result is "<<ToJson(session_timer_);
        }
      }
      break;
    }
  }

}

void TimeRecorder::AddEvent(uint32_t stream_id, EventType type) {
  VLOG(10)<<"=====AddEvent";
  std::lock_guard<std::mutex> lock(mutex_);
  int size = session_timer_.stream_items_size();
  for (int i = 0; i < size; i++) {
    StreamItem* item = session_timer_.mutable_stream_items(i);
    if (item->stream_id() == stream_id) {
      StreamEvent* start_event = item->add_stream_events();
      start_event->set_event_type(type);
      start_event->set_event_time(GetCurrentTime_MS() - start_real_time_);
      break;
    }
  }

}

string TimeRecorder::ToJson(const google::protobuf::Message &msg) {
  return jsonpb::pb2json(msg);
}

void TimeRecorder::UpdateSessionInfo() {

  uint32_t session_id = session_timer_.session_id();
  orbit::SessionInfoManager* session_info =
       Singleton<orbit::SessionInfoManager>::GetInstance();
  orbit::SessionInfoPtr session = session_info->GetSessionInfoById(session_id);
  int size = session_timer_.stream_items_size();
  for (int i = 0; i < size; i++) {
    StreamItem *item = session_timer_.mutable_stream_items(i);
    int stream_id = item->stream_id();
    StreamInfoPtr ptr = session->GetStreamInfoById(stream_id);
    UserInfo* user_info = item->mutable_user_info();
    user_info->set_user_id(ptr->GetUserID());
    user_info->set_user_name(ptr->GetUserName());
    user_info->set_origin_user_id(ptr->GetOriginUserID());
  }
}

void TimeRecorder::WriteToPBFile() {

  UpdateSessionInfo();
  uint32_t session_id = session_timer_.session_id();
  orbit::SessionInfoManager* session_info =
       Singleton<orbit::SessionInfoManager>::GetInstance();
  orbit::SessionInfoPtr session = session_info->GetSessionInfoById(session_id);
  string room_id = "";
  if(session != NULL) {
    room_id = session->GetRoomID();
  }
  string folder = StringPrintf("%s/%s_%d",
         export_folder_.c_str(),
         room_id.c_str(), session_id);
  File::RecursivelyCreateDir(folder);
  string file_path = StringPrintf("%s/%s_%d_%lld.timeline.json.pb",
      folder.c_str(), room_id.c_str(), session_id, start_real_time_);

  if (!file::Open(file_path, "wb", &file_, file::Defaults()).ok()) {
    LOG(FATAL)<< "Cannot open " << file_path;
  }
  RecordWriter* writer_ = new RecordWriter(file_);
  writer_->set_use_compression(false);
  writer_->WriteProtocolMessage(session_timer_);
  writer_->Close();
  delete writer_;
  writer_ = NULL;
}

void TimeRecorder::WriteToJsonFile() {

  UpdateSessionInfo();
  uint32_t session_id = session_timer_.session_id();
  orbit::SessionInfoManager* session_info =
       Singleton<orbit::SessionInfoManager>::GetInstance();
  orbit::SessionInfoPtr session = session_info->GetSessionInfoById(session_id);
  string room_id = "";
  if(session != NULL) {
    room_id = session->GetRoomID();
  }
  string folder = StringPrintf("%s/%s_%d",
         export_folder_.c_str(),
         room_id.c_str(), session_id);
  File::RecursivelyCreateDir(folder);
  string file_path = StringPrintf("%s/%s_%d_%lld.timeline.json",
      folder.c_str(), room_id.c_str(), session_id, start_real_time_);
  if (!file::Open(file_path, "wb", &file_, file::Defaults()).ok()) {
    LOG(FATAL)<< "Cannot open " << file_path;
  }
  string json = ToJson(session_timer_);
  const uint64 json_size = json.size();

  if (file_->Write(json.c_str(), json_size) !=
      json_size) {
    LOG(FATAL)<<"TimeRecorder write file error";
  }
  file_->Flush();
  file_->Close();
  delete file_;
  file_ = NULL;

}

void TimeRecorder::SetSessionId(uint32_t session_id) {
  std::lock_guard < std::mutex > lock(mutex_);
  session_timer_.set_session_id(session_id);
}
}
 // namespace orbit
