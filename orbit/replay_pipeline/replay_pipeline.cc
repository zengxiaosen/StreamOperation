/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * replay_pipeline.cc
 * ---------------------------------------------------------------------------
 * Implements the replay pipeline.
 * ---------------------------------------------------------------------------
 */

#include "replay_pipeline.h"

// For Gflags and Glog
#include "gflags/gflags.h"
#include "glog/logging.h"

#include "stream_service/orbit/debug_server/rtp_capture.h"
#include "stream_service/orbit/modules/stream_recorder_element.h"
#include "stream_service/orbit/base/file.h"
#include "stream_service/orbit/base/strutil.h"

DECLARE_string(record_format);
namespace orbit {
DEFINE_string(export_directory, "/tmp/orbit_data/", "Specifies the export directory for output video file.");
DEFINE_bool(sync_time, false, "If set to true, it will sleep for cause the delay for the pipeline");
DEFINE_bool(debug_console, true, "Set to true for more debugging information");

string GetSessionFromPBFile(const string& replay_file) {
  vector<string> file_parts;
  orbit::SplitStringUsing(replay_file, "/", &file_parts);
  assert(replay_file.size() > 0);
  string session = file_parts.back();
  if (HasSuffixString(session, ".pb")) {
    session = StripSuffixString(session, ".pb");
  }
  return session;
}
int ReplayPipeline::Run() {
  LOG(INFO) << "Start running the replay_pipeline.";
  vector<string> replay_files;
  orbit::SplitStringUsing(file_, ";", &replay_files);
  if (replay_files.size() == 0) {
//    LOG(ERROR) << "Please speicfy the --replay_files flag.";
    return -1;
  }
  for (string file : replay_files) {
    StartReplay(file);
  }
  return 1;
}

void ReplayPipeline::PushPacket(StreamRecorderElement* stream_recorder,
                                std::shared_ptr<StoredPacket> pkt) {
  packetType packet_type;
  if (pkt->type() == AUDIO) {
    packet_type = AUDIO_PACKET;
  } else if (pkt->type() == VIDEO) {
    packet_type = VIDEO_PACKET;
  } else {
    LOG(INFO) << "Unknown packet type. Return";
    return;
  }
  
  dataPacket p;
  p.comp = 0;
  p.type = packet_type;
  p.remote_ntp_time_ms = pkt->remote_ntp_time_ms();
    
  memcpy(&(p.data[0]), const_cast<char*>(pkt->data().c_str()), pkt->packet_length());
  p.length = pkt->packet_length();
  
  if (packet_type == VIDEO_PACKET) {
//    RtpHeader* h = reinterpret_cast<RtpHeader*>(&(p.data[0]));
//    switch(h->getPayloadType()) {
//    case RED_90000_PT:
//      LOG(INFO) << "RED/90000";
//      break;
//    case VP8_90000_PT:
//      LOG(INFO) << "VP8/90000";
//      break;
//    case VP9_90000_PT:
//      LOG(INFO) << "VP9/90000";
//      break;
//    default:
//      LOG(INFO) << "Unkown payload;";
//    }
//    LOG(INFO) << "seqn=" << (int)h->getSeqNumber()
//              << ", pt=" << (int)h->getPayloadType();
    stream_recorder->PushVideoPacket(p);
  } else {
    stream_recorder->PushAudioPacket(p);
  }
}

StreamRecorderElement* ReplayPipeline::GetRecorder(int transport_id, const string& replay_file) {
  StreamRecorderElement* recorder = NULL;
  if (recorder_pool_.find(transport_id) != recorder_pool_.end()) {
    recorder = recorder_pool_[transport_id];
    return recorder;
  }

  int video_encoding = GetVideoEncoding(transport_id, replay_file);

  string session = GetSessionFromPBFile(replay_file);
  // If this is a new transport_id.
  std::string export_dir = dest_folder_;
  if(dest_folder_ == ""){
    export_dir = FLAGS_export_directory;
    File::RecursivelyCreateDir(export_dir);
  }

  std::string export_file_name = StringPrintf("%s/%d.%s",
                                              export_dir.c_str(),
                                              transport_id,
                                              FLAGS_record_format.c_str());
  recorder = new StreamRecorderElement(export_file_name, video_encoding);
  LOG(INFO)<<"===========================================File name "<<export_file_name;
  recorder_pool_[transport_id] = recorder;
  return recorder;
}

void ReplayPipeline::StartReplay(std::string replay_file) {
  std::unique_ptr<RtpReplay> replay(new RtpReplay);
  replay->Init(replay_file);
  std::shared_ptr<StoredPacket> packet;
  long ts = 0;
  int i = 0;
  StreamRecorderElement* stream_recorder = NULL;
  do {
    packet = replay->Next();
    if (packet != NULL && packet->packet_type() != RTCP_PACKET) {
      stream_recorder = GetRecorder(packet->transport_id(), replay_file);
      assert( stream_recorder != NULL);

      if (FLAGS_sync_time) {
        if (ts != 0) {
          long long now = packet->ts();
          int sleep_time = (int)(now - ts);
          LOG(INFO) << "sleep_time=" << sleep_time << " ms";
          if (sleep_time > 0) {
            usleep(sleep_time * 1000);
          }
        }
      }

      ts = packet->ts();
      if (FLAGS_debug_console) {
        LOG(INFO) << " i=" << i
                  << " packet_type=" << StoredPacketType_Name(packet->packet_type())
                  << " type=" << StoredMediaType_Name(packet->type())
                  << " length=" << packet->packet_length()
                  << " ts=" << ts;
      }
      PushPacket(stream_recorder, packet);
    }
    ++i;
  } while(packet != NULL);

  // Clean up
  for (auto iter : recorder_pool_) {
    delete iter.second;
  }
  /*
   * Add finish tag.
   */
  File *file;
  if (!file::Open(dest_folder_+"/finish.tag", "wb", &file, file::Defaults()).ok()) {
    LOG(ERROR)<<"Replay write finish tag error.";
  } else {
    file->WriteLine("");
    file->Flush();
    file->Close();
    delete file;
  }
}

int ReplayPipeline::GetVideoEncoding(int stream_id, std::string replay_file) {

  //1.Init rtpreplay.
  std::unique_ptr<RtpReplay> replay(new RtpReplay);
  replay->Init(replay_file);

  //2.Get video payload type.
  uint32_t paylod_type = VP8_90000_PT;
  std::shared_ptr<StoredPacket> packet;
  do{
    packet = replay->Next();
     if (packet != NULL && packet->type() == VIDEO && packet->transport_id() == stream_id) {
       dataPacket p;
       memcpy(&(p.data[0]), const_cast<char*>(packet->data().c_str()), packet->packet_length());
       RtpHeader* head = reinterpret_cast<RtpHeader*>(&(p.data[0]));
       paylod_type = head->getPayloadType();
       if(paylod_type == RED_90000_PT){
         RedHeader* red_header = reinterpret_cast<RedHeader*>(&p.data[12]);
         paylod_type = red_header->payloadtype;
       }
       break;
     }
  } while(packet != NULL);

  LOG(INFO)<<"ReplayPipelien::GetVideoPayloadType = "<<paylod_type;
  return paylod_type;
}

}  // namespace orbit
