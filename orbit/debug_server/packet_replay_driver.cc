/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * packet_replay_driver.cc
 * ---------------------------------------------------------------------------
 * Implements the replay driver class.
 * ---------------------------------------------------------------------------
 */

#include "packet_replay_driver.h"
// For Gflags and Glog
#include "gflags/gflags.h"
#include "glog/logging.h"

#include "rtp_capture.h"
#include "stream_service/orbit/base/singleton.h"

#include "stream_service/orbit/video_mixer/video_mixer_plugin.h"
#include "stream_service/orbit/video_dispatcher/video_dispatcher_plugin.h"
#include "stream_service/orbit/video_dispatcher/video_dispatcher_room.h"
#include "stream_service/orbit/transport_plugin.h"
#include "stream_service/orbit/transport_delegate.h"
#include "stream_service/orbit/echo_plugin.h"
#include "stream_service/orbit/video_bridge_plugin.h"
#include "stream_service/orbit/audio_conference_plugin.h"
#include "replay_transport_delegate.h"
#include "stream_service/orbit/base/timeutil.h"
#include "stream_service/orbit/base/strutil.h"

#include "stream_service/orbit/base/session_info.h"

// For thread
#include <thread>
#include <mutex>
#include <sys/prctl.h>
#include <condition_variable>    // std::condition_variable
#include <memory>

DEFINE_bool(enable_red_fec, true, "EnablR Video RED/FEC codecs");
DEFINE_string(replay_files, "", "Specifies multiple replay pb files, using"
              " the delimiter to separete them.. e.g. video_rtp1.pb;video_rtp2.pb");
DECLARE_string(test_plugin);

namespace orbit {

void PacketReplayDriver::SetupRoom(int session_id) {
  bool use_webcast = false;

  if(plugin_name_ == "video_mixer"){
    audio_video_room_ = std::make_shared<orbit::VideoMixerRoom>(session_id, use_webcast);
  } else if(plugin_name_ == "video_dispatcher") {
    audio_video_room_ = std::make_shared<orbit::VideoDispatcherRoom>();
  } else if(plugin_name_ == "audio_mixer"){
    audio_video_room_ = std::make_shared<orbit::AudioConferenceRoom>(session_id, false, false);
  } else {
    audio_video_room_ = std::make_shared<orbit::AudioConferenceRoom>(session_id,use_webcast, true);
  }
  
  audio_video_room_->Create();
  audio_video_room_->Start();

  // Setup the record stream
  //((orbit::VideoMixerRoomImpl*)audio_video_room_)->SetupRecordStream();
}

void PacketReplayDriver::CleanRoom() {
  sleep(1);
}

orbit::ReplayTransportDelegate*
   PacketReplayDriver::SetupTransportDelegate(int session_id,
                                              int stream_id,
                                              int transport_id) {
  // If we are going to use EchoPlugin, then create and set a Echo plugin.
  // orbit::EchoPlugin* plugin = new orbit::EchoPlugin();
  // ---------------------------------------------------------------------
  // For using VideoBridgePlugin, it should be.
  // orbit::VideoBridgePlugin* plugin = new orbit::VideoBridgePlugin(audio_video_room_,
  //                                                                stream_id);
  // ---------------------------------------------------------------------
  // orbit::AudioConferencePlugin* plugin =
  //   new orbit::AudioConferencePlugin(audio_video_room_, stream_id);
  orbit::TransportPlugin* plugin;
  if (plugin_name_ == "simple_echo") {
    plugin = new orbit::SimpleEchoPlugin();
  } else if (plugin_name_ == "echo") {
    plugin = new orbit::EchoPlugin();
  } else if (plugin_name_ == "bridge") {
    plugin = new orbit::VideoBridgePlugin(audio_video_room_,
                                          stream_id);
  } else if (plugin_name_ == "video_mixer") {
    //TODO change pulgin_impl to plugin.
    plugin = new orbit::VideoMixerPluginImpl(audio_video_room_, stream_id, 0, 0);
  } else if (plugin_name_ == "video_dispatcher") {
    plugin = new orbit::VideoDispatcherPlugin(audio_video_room_, stream_id);
  } else {
    plugin = new orbit::AudioConferencePlugin(audio_video_room_, stream_id); 
  }
  
  bool audio_enabled = true;
  bool video_enabled = true;
  bool trickle_enabled = true;
  orbit::IceConfig ice_config;
  
  ReplayTransportDelegate* delegate =
    new orbit::ReplayTransportDelegate(audio_enabled, video_enabled,
                                       trickle_enabled, ice_config,
                                       session_id, stream_id);
  delegate->setPlugin(plugin);
  plugin->set_active(true);

  delegate->bundle_ = 1;
  SdpInfo remote_sdp = sdp_infos_[transport_id];
  assert(remote_sdp != 0);
  delegate->remote_sdp_ = remote_sdp;
  
  // Add the participants and start communications.
  audio_video_room_->AddParticipant(plugin);
  has_participants_ = true;
  return delegate;
}

  orbit::ReplayTransportDelegate* PacketReplayDriver::GetTransportDelegate(int session_id, int transport_id) {
  orbit::ReplayTransportDelegate* delegate;
  if (delegate_pool_.find(transport_id) != delegate_pool_.end()) {
    delegate = delegate_pool_[transport_id];
  } else {
    orbit::SessionInfoManager* session_info = 
      Singleton<orbit::SessionInfoManager>::GetInstance();

    // This is a new participant.
    int stream_id = rand();
    orbit::SessionInfoPtr session = session_info->GetSessionInfoById(session_id);
    if(session != NULL) {
      session->AddStream(stream_id);
    }
    delegate = SetupTransportDelegate(session_id, stream_id, transport_id);
    delegate_pool_[transport_id] = delegate;
    delegate->updateState(TRANSPORT_READY);
  }
  return delegate;
}

void PacketReplayDriver::StartReplay(int session_id, string replay_file) {
  orbit::ReplayTransportDelegate* delegate;
  //RtpReplay* replay = Singleton<RtpReplay>::GetInstance();
  std::unique_ptr<RtpReplay> replay(new RtpReplay);
  replay->Init(replay_file);
  int i = 0;
  std::shared_ptr<StoredPacket> packet;
  long ts = 0;
  do {
    packet = replay->Next();
    if (packet != NULL) {
      // If this is a new Delegate:
      delegate = GetTransportDelegate(session_id, packet->transport_id());
      if (ts != 0) {
        long long now = packet->ts();
        int sleep_time = (int)(now - ts);
        LOG(INFO) << "sleep_time=" << sleep_time << " ms";
        if (sleep_time > 0) {
          usleep(sleep_time * 1000);
        }
      }
      ts = packet->ts();
      
      LOG(INFO) << "times=" << times_ << " i=" << i;
      LOG(INFO) << "packet_type=" << StoredPacketType_Name(packet->packet_type())
                << " type=" << StoredMediaType_Name(packet->type())
                << " length=" << packet->packet_length();
      delegate->onTransportData(const_cast<char*>(packet->data().c_str()), packet->packet_length(), NULL);
    }
    ++i;
  } while(packet != NULL);

  for (auto iter : delegate_pool_) {
    delegate = iter.second;
    int stream_id = delegate->stream_id();
    orbit::SessionInfoManager* session_info = 
      Singleton<orbit::SessionInfoManager>::GetInstance();
    orbit::SessionInfoPtr session = session_info->GetSessionInfoById(session_id);
    if(session != NULL) {
      session->RemoveStream(stream_id);
    }
    audio_video_room_->RemoveParticipant(delegate->getPlugin());
  }
}

void PacketReplayDriver::PrepareReplay(int session_id, string replay_file) {
  std::unique_ptr<RtpReplay> replay(new RtpReplay);
  replay->Init(replay_file);
  std::shared_ptr<StoredPacket> packet;
  int i = 0;
  do {
    i++;
    packet = replay->Next();
    if (packet != NULL) {
      char* buf = const_cast<char*>(packet->data().c_str());
      RtcpHeader* chead = (RtcpHeader*)(buf);
      if (!chead->isRtcp()) {
        RtpHeader *head = reinterpret_cast<RtpHeader*> (buf);
        uint32_t recvSSRC = head->getSSRC();
        int transport_id = packet->transport_id();
        SdpInfo remote_sdp = sdp_infos_[transport_id];
        if (packet->type() == AUDIO) {
          remote_sdp.setAudioSsrc(recvSSRC);
        } else if (packet->type() == VIDEO) {
          remote_sdp.setVideoSsrc(recvSSRC);
        } else if (packet->type() == VIDEO_RTX) {
          remote_sdp.setVideoRtxSsrc(recvSSRC);
        }
        sdp_infos_[transport_id] = remote_sdp;
      }
    }
  } while(packet != NULL);
}

int PacketReplayDriver::RunReplayPipeline() {
  int session_id = rand();

  plugin_name_ = FLAGS_test_plugin;
  SetupRoom(session_id);

  vector<string> replay_files;
  orbit::SplitStringUsing(FLAGS_replay_files, ";", &replay_files);
  vector<std::thread*> threads;
  
  is_ready_ = false;
  has_participants_ = false;

  orbit::SessionInfoManager* session_info = 
    Singleton<orbit::SessionInfoManager>::GetInstance();
  session_info->AddSession(session_id);

  // First step: prepare for the replay using the replay files. 
  // To setup the transport_plugins and other room stuff.
  for (string file : replay_files) {
    PrepareReplay(session_id, file);
  }

  // Start to replay the RTP files...
  for (string file : replay_files) {
    std::thread* t = new std::thread([file, session_info, session_id, this] () {
        // Start a thread to replay the file.
        while (!this->is_ready_) {
          usleep(1000);
        }
        this->StartReplay(session_id, file);
      });
    threads.push_back(t);
  }

  if (FLAGS_test_plugin == "audio_conference") {
    std::thread* t = new std::thread([&] () {
        while (!this->is_ready_) {
          usleep(1000);
        }
        bool exit = false;
        int viewer = 0;
        
        bool start_time_init = false;
        time_t start_time;
        time_t end_time;
        while (!exit) {
          int participants_size = audio_video_room_->GetParticipants().size();
          if (has_participants_ && participants_size == 0) {
            LOG(INFO) << "All participants clear. Exit";
            exit = true;
            break;
          }
          unsigned long diff;
          {
            if (!start_time_init) {
              start_time = time(NULL);
              start_time_init = true;
            }
            end_time = time(NULL);
            diff = end_time -start_time;
          }
          // If the time has passed for 1 seconds. we should switch the publisher.
          if (diff > 1) {
            start_time = end_time;
            if (participants_size > 1) {
              viewer++;
              if (viewer >= participants_size) {
                viewer = 0;
              }
              std::shared_ptr<AudioConferenceRoom> av_room = std::static_pointer_cast<orbit::AudioConferenceRoom>(audio_video_room_);
              av_room->SwitchPublisher(viewer);
            }
          } else {
            usleep(1000 * 1000);
          }
        }
      });
    threads.push_back(t);
  }

  this->is_ready_ = true;
  for (auto mythread : threads) {
    mythread->join();
  }

  session_info->RemoveSession(session_id);
  CleanRoom();
  return 1;
}

int PacketReplayDriver::RunMain(int run_times) {
  for (int i = 0; i < run_times; i++) {
    times_ = i;
    RunReplayPipeline();
    LOG(INFO)<<"===========TimeIndex=============="<<i;
  }
  return 1;
}

}  // namespace orbit
