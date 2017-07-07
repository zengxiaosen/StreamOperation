/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * classroom_plugin.cc
 * ---------------------------------------------------------------------------
 * Implements a Orbit Class Room and plugins
 * ---------------------------------------------------------------------------
 */

#include "classroom_plugin.h"
#include "stream_service/orbit/rtp/janus_rtcp_processor.h"
#include "stream_service/orbit/base/strutil.h"
#include "stream_service/orbit/base/session_info.h"
#include "stream_service/orbit/replay_pipeline/replay_exector.h"
#include "gflags/gflags.h"

DECLARE_string(packet_capture_directory);
DEFINE_bool(auto_replay_pb_file, false,
            "If set, we will auto replay recorded db to video file."
            "Default : false");

namespace orbit {
  using namespace std;
  // --------------------------------Class ClassRoomPlugin------------------------
  ClassRoomPlugin::ClassRoomPlugin(std::weak_ptr<Room> room, int stream_id) : AudioConferencePlugin(room, stream_id) {
    role_ = TEACHER;
  }

  void ClassRoomPlugin::SetExtendedVideoSsrcs(std::vector<unsigned int> extended_video_ssrcs) {
    std::lock_guard<std::mutex> p_lock(video_ssrc_mutex_);
    TransportPlugin::SetExtendedVideoSsrcs(extended_video_ssrcs);
    for (auto ex_ssrc : extended_video_ssrcs) {
      extended_ssrc_vec_.push_back(make_pair(ex_ssrc, 0));
    }
  }

  void ClassRoomPlugin::SendFirPacket() {
    // Send another FIR/PLI packet to the sender.
    /* Send a FIR to the new RTP forward publisher */
    char buf[20];
    memset(buf, 0, 20);
    int fir_tmp = key_frame_seq_;
    int len = janus_rtcp_fir((char *)&buf, 20, &fir_tmp);
    key_frame_seq_ = fir_tmp;
    
    dataPacket p;
    p.comp = 0;
    p.type = VIDEO_PACKET;
    p.length = len;
    memcpy(&(p.data[0]), buf, len);
    
    RelayRtcpPacket(p);
    VLOG(4) << "Send RTCP...FIR....";
    
    /* Send a PLI too, just in case... */
    memset(buf, 0, 12);
    len = janus_rtcp_pli((char *)&buf, 12);
    
    p.length = len;
    memcpy(&(p.data[0]), buf, len);

    RelayRtcpPacket(p);
    VLOG(4) << "Send RTCP...PLI....";
  }

  void ClassRoomPlugin::LinkToSsrc(unsigned int other_ssrc, int other_stream_id) {
    UnlinkToSsrc(other_ssrc);
    std::lock_guard<std::mutex> p_lock(video_ssrc_mutex_);
    for (auto extended_ssrc_it = extended_ssrc_vec_.begin();
         extended_ssrc_it != extended_ssrc_vec_.end(); extended_ssrc_it++) {
      if (extended_ssrc_it->second == 0) {
        // New incoming downstream (with new ssrc).
        unsigned int downstream_ssrc = extended_ssrc_it->first;
        extended_ssrc_it->second = other_ssrc;
        auto switch_contexts_iter = video_switch_contexts_.find(downstream_ssrc);
        if (switch_contexts_iter != video_switch_contexts_.end()) {
          std::shared_ptr<ClassRoomSsrcSwitchContext> ssrc_switch_context = switch_contexts_iter->second;
          ssrc_switch_context->RenewContext(downstream_ssrc, other_ssrc, other_stream_id);
          ssrc_switch_context->last_ts = ssrc_switch_context->ts + 2880;
          ssrc_switch_context->last_seq = ssrc_switch_context->seq + 1;
          ssrc_switch_context->base_ts = -1;
          ssrc_switch_context->base_seq = -1;
        } else {
          std::shared_ptr<ClassRoomSsrcSwitchContext> context = std::make_shared<ClassRoomSsrcSwitchContext>();
          video_switch_contexts_[downstream_ssrc] = context;
          context->RenewContext(downstream_ssrc, other_ssrc, other_stream_id);
        }
        break;
      } 
    }
  }

  void ClassRoomPlugin::UnlinkToSsrc(unsigned int other_ssrc) {
    std::lock_guard<std::mutex> p_lock(video_ssrc_mutex_);
    for (auto extended_ssrc_it = extended_ssrc_vec_.begin();
         extended_ssrc_it != extended_ssrc_vec_.end(); extended_ssrc_it++) {
      if (extended_ssrc_it->second == other_ssrc) {
        extended_ssrc_it->second = 0;
      }
    }
  }

  void ClassRoomPlugin::RewriteRtpHeader(dataPacket* p) {
    std::lock_guard<std::mutex> p_lock(video_ssrc_mutex_);
    RtpHeader* rtp = (RtpHeader*)(&(p->data[0]));
    unsigned int video_ssrc = rtp->getSSRC();
    
    unsigned int downstream_ssrc = 0;
    std::shared_ptr<ClassRoomSsrcSwitchContext> ssrc_switch_context = NULL;

    // Use a reverse lookup method to find out the downstream_ssrc of the
    // corresponding video_ssrc.
    for (auto extended_ssrc_it = extended_ssrc_vec_.begin();
         extended_ssrc_it != extended_ssrc_vec_.end(); extended_ssrc_it++) {
      if (extended_ssrc_it->second == video_ssrc) {
        downstream_ssrc = extended_ssrc_it->first;
        auto switch_contexts_iter = video_switch_contexts_.find(downstream_ssrc);
        assert(switch_contexts_iter != video_switch_contexts_.end());

        if (switch_contexts_iter != video_switch_contexts_.end()) {
          ssrc_switch_context = switch_contexts_iter->second;
          break;
        }
      }
    }
    // Assert that the ssrc_switch_context and the downstream_ssrc must be
    // found since they should have been linked.
    assert(ssrc_switch_context.get() != NULL);
    assert(downstream_ssrc != 0);
    if (ssrc_switch_context.get() == NULL) {
      LOG(INFO) << "ssrc_switch_context point is null.";
      return;
    }

    // The switch_context contains the last_seq/last_ts, as well as the ts
    // and seq(current). To calculate the new packet's current seq and ts,
    // use the algorithm as follows:
    //  if the stream is new one (i.e. the base_seq/base_ts is -1), then
    //  reset the base_seq and base_ts to this packet's seq and ts.
    //  And the upcoming packet will be calcuated as:
    //   seq = last_seq + (rtp->seq - base_seq)
    //   ts = last_ts + (rtp->ts - base_ts)
    // NOTE: The reason that we should rewrite the seq and ts - if the new
    // stream come up to take one old slot of the ssrc map - the stream should
    // have been sent to the client(browser), and the stream needs keeping
    // the seq/ts as the sequential next ones, otherwise a big jump seq number
    // error may be occured (in SRTP, srtp_channel.cc).
    if (ssrc_switch_context->base_seq == -1) {
      ssrc_switch_context->base_seq = rtp->getSeqNumber();
    }
    ssrc_switch_context->seq = ssrc_switch_context->last_seq + (rtp->getSeqNumber() - ssrc_switch_context->base_seq);

    if (ssrc_switch_context->base_ts == -1) {
      ssrc_switch_context->base_ts = rtp->getTimestamp();
    }
    ssrc_switch_context->ts = ssrc_switch_context->last_ts + (rtp->getTimestamp() - ssrc_switch_context->base_ts);

    // Rewrite the SSRC and SeqNumber and Timestamp of the packet.
    rtp->setSSRC(downstream_ssrc);
    rtp->setTimestamp(ssrc_switch_context->ts);
    rtp->setSeqNumber(ssrc_switch_context->seq);
  }
  
  void ClassRoomPlugin::RelayRtpPacket(const dataPacket& packet) {
    if (!active()) {
      return;
    }

    if (packet.type == AUDIO_PACKET) {
      TransportPlugin::RelayRtpPacket(packet);
    }
    if (packet.type == VIDEO_PACKET) {
      dataPacket p;
      p.comp = 0;
      p.type = VIDEO_PACKET;
      p.length = packet.length;
      memcpy(&(p.data[0]), &(packet.data[0]), packet.length);
      // Rewrite the RTP header of the data packet p. 
      RewriteRtpHeader(&p);
      // Relay the packet to the browser.
      TransportPlugin::RelayRtpPacket(p);
    }
  }

  void ClassRoomPlugin::IncomingRtcpPacket(const dataPacket& packet) {
    TransportPlugin::IncomingRtcpPacket(packet);
    if (packet.type == AUDIO_PACKET) {
      // Skip for now
    } else if (packet.type == VIDEO_PACKET) {
      const unsigned char* buf1 = reinterpret_cast<const unsigned char*>(packet.data);
      unsigned char* buf2 = const_cast<unsigned char*>(buf1);
      char* buf = reinterpret_cast<char*>(buf2);
      int len = packet.length;
      if(janus_rtcp_has_fir(buf, len) || janus_rtcp_has_pli(buf, len)) {
        /* We got a FIR, forward it to all of the publisher */
        auto audio_room = room_.lock();
        if (audio_room.get() != NULL) {
          ((ClassRoom*)audio_room.get())->RequestKeyFrameFromAllParticipants();
        }
      }
    }
  }

  void ClassRoomPlugin::OnTransportStateChange(TransportState state) {
    LOG(INFO) << "TransportState change to " << state;
    auto class_room = room_.lock();
    if (class_room && class_room.get() != NULL) {
      ((ClassRoom*)class_room.get())->OnTransportStateChange(stream_id(), state);
    }
  }

  void ClassRoomPlugin::GetStreamVideoMap(olive::StreamVideoMapResponse* stream_video_map_response) {
    std::lock_guard<std::mutex> p_lock(video_ssrc_mutex_);
    int video_id = 1;
    for (auto extended_ssrc_it = extended_ssrc_vec_.begin();
         extended_ssrc_it != extended_ssrc_vec_.end(); extended_ssrc_it++) {
      if (extended_ssrc_it->second != 0) {
        unsigned int downstream_ssrc = extended_ssrc_it->first;
        auto switch_contexts_iter = video_switch_contexts_.find(downstream_ssrc);
        assert(switch_contexts_iter != video_switch_contexts_.end());
        std::shared_ptr<ClassRoomSsrcSwitchContext> switch_context = switch_contexts_iter->second;
        olive::StreamVideoMap* stream_video_map = stream_video_map_response->add_stream_video_maps();
        stream_video_map->set_stream_id(switch_context->source_stream_id);
        stream_video_map->set_video_id(video_id);
        stream_video_map->set_mslabel(StringPrintf("v%d", video_id));
      }
      video_id++;
    }
  }

  // --------------------------------Class ClassRoom------------------------
  ClassRoom::ClassRoom(int32_t session_id)
    : session_id_(session_id) {
    running_ = false;
    published_streams_.clear();
    master_streams_.clear();
  }

  ClassRoom::~ClassRoom() {
    running_ = false;
    Destroy();
  }

  void ClassRoom::Create() {
    if (!Init()) {
      LOG(ERROR) << "Create the ClassRoom, init failed.";
    }
  }

  void ClassRoom::AddParticipant(TransportPlugin *plugin) {    
    std::lock_guard<std::mutex> p_lock(classroom_plugins_mutex_);
    ClassRoomPlugin* classroom_plugin = (ClassRoomPlugin*)plugin;
    Room::AddParticipant(plugin);

    int stream_id = classroom_plugin->stream_id();
    audio_mixer_element_->AddAudioBuffer(stream_id);
    audio_mixer_element_->RegisterMixResultListener(stream_id,
        dynamic_cast<IAudioMixerRtpPacketListener*>(classroom_plugin));

    if (classroom_plugin->GetPluginRole() == ClassRoomPlugin::TEACHER) {
      std::lock_guard<std::mutex> p_lock(published_streams_mutex_);
      master_streams_.insert(stream_id);
      if(time_recorder_ != NULL) {
        time_recorder_->AddStream(stream_id, TEACHER);
      }
    } else {
      // By default, we publish the video stream of STUDENT.
      std::lock_guard<std::mutex> p_lock(published_streams_mutex_);
      published_streams_.insert(stream_id);
      if(time_recorder_ != NULL) {
        time_recorder_->AddStream(stream_id, STUDENT);
      }
    }

    if (rtp_capture_ != NULL && !rtp_capture_->IsReady()) {
      orbit::SessionInfoManager* session_info =
             Singleton<orbit::SessionInfoManager>::GetInstance();
      orbit::SessionInfoPtr session = session_info->GetSessionInfoById(session_id_);
      string room_id = "";
      if(session != NULL) {
        room_id = session->GetRoomID();
      }
      File::RecursivelyCreateDir(FLAGS_packet_capture_directory);

      string folder = StringPrintf("%s/%s_%d",
          FLAGS_packet_capture_directory.c_str(),
              room_id.c_str(), session_id_);
       File::RecursivelyCreateDir(folder);

       string file_path = StringPrintf("%s/%d.pb",
           folder.c_str(), session_id_);

      rtp_capture_->SetExportFile(file_path);
      rtp_capture_->SetFolderPath(folder);
      LOG(INFO)<<"----------================----Add participant room id is "<<file_path;
    }
  }

  bool ClassRoom::RemoveParticipant(TransportPlugin* plugin) {
    std::lock_guard<std::mutex> p_lock(classroom_plugins_mutex_);
    unsigned int upstream_ssrc = plugin->GetUpstreamSsrc();
    Room::RemoveParticipant(plugin);

    // Remove all the plugins's reference to this plugin's SSRC
    std::vector<TransportPlugin*> participants = GetParticipants();
    for (vector<TransportPlugin*>::iterator it = participants.begin(); it != participants.end(); ++it) {
      TransportPlugin* transport_plugin = *(it);
      ClassRoomPlugin* classroom_plugin = (ClassRoomPlugin*)transport_plugin;
      classroom_plugin->UnlinkToSsrc(upstream_ssrc);
    }

    ClassRoomPlugin* classroom_plugin = (ClassRoomPlugin*)plugin;
    int stream_id = classroom_plugin->stream_id();
    audio_mixer_element_->RemoveAudioBuffer(classroom_plugin->stream_id());
    audio_mixer_element_->UnregisterMixResultListener(classroom_plugin->stream_id());

    {
      std::lock_guard<std::mutex> p_lock(published_streams_mutex_);
      master_streams_.erase(stream_id);
      published_streams_.erase(stream_id);
    }
    if(time_recorder_!= NULL) {
      time_recorder_->RemoveStream(stream_id);
    }
    return true;
  }

  void ClassRoom::Destroy() {
    LOG(INFO)<<"ClassRoom destroy";
    running_ = false;
    if (rtp_capture_ != NULL) {
      if(FLAGS_auto_replay_pb_file) {
        ReplayExector* exector = Singleton<ReplayExector>::GetInstance();
        exector->AddTask(rtp_capture_->GetFilePath(), rtp_capture_->GetFolderPath());
      }
      delete rtp_capture_;
      rtp_capture_ = NULL;
    }
    if(time_recorder_ != NULL) {
      delete time_recorder_;
      time_recorder_ = NULL;
    }

  }

  bool ClassRoom::Init() {
    audio_mixer_element_.reset(
        AudioMixerFactory::Make(session_id_,
                                new AudioOption(),
                                this, // IAudioMixerRawListener
                                this  // ISpeakerChangeListener
        ));
    return true;
  }

  void ClassRoom::EnableCapturePackets(bool enable) {
    enable_capture_packets_ = enable;

    // Capture module initialization.
    if (enable) {
      rtp_capture_ = new RtpCapture();
      time_recorder_ = new TimeRecorder(FLAGS_packet_capture_directory, this->session_id_);
    }
  }

  void ClassRoom::Start() {
    running_ = true;
  }

  void ClassRoom::RequestKeyFrameFromAllParticipants() {
    std::lock_guard<std::mutex> p_lock(classroom_plugins_mutex_);
    std::vector<TransportPlugin*> participants = GetParticipants();
    for (vector<TransportPlugin*>::iterator it = participants.begin(); it != participants.end(); ++it) {
      ClassRoomPlugin* class_room_plugin = (ClassRoomPlugin*)(*it);
      class_room_plugin->SendFirPacket();
    }
  }

  void ClassRoom::LinkSsrcsToParticipants(int stream_id) {
    std::lock_guard<std::mutex> p_lock(classroom_plugins_mutex_);
    std::vector<TransportPlugin*> participants = GetParticipants();
    ClassRoomPlugin* new_plugin = NULL;
    for (vector<TransportPlugin*>::iterator it = participants.begin(); it != participants.end(); ++it) {
      TransportPlugin* transport_plugin = *(it);
      ClassRoomPlugin* classroom_plugin = (ClassRoomPlugin*)transport_plugin;
      if (classroom_plugin->stream_id() == stream_id) {
        new_plugin = classroom_plugin;
        break;
      }
    }

    assert(new_plugin != NULL);
    if (new_plugin == NULL) {
      LOG(INFO) << "new_plugin is null.";
      return;
    }
    
    unsigned int upstream_ssrc = new_plugin->GetUpstreamSsrc();
    for (vector<TransportPlugin*>::iterator it = participants.begin(); it != participants.end(); ++it) {
      TransportPlugin* transport_plugin = *(it);
      ClassRoomPlugin* old_plugin = (ClassRoomPlugin*)transport_plugin;
      if (old_plugin->stream_id() == stream_id) {
        continue;
      }
      old_plugin->LinkToSsrc(upstream_ssrc, new_plugin->stream_id());
      new_plugin->LinkToSsrc(old_plugin->GetUpstreamSsrc(), old_plugin->stream_id());
    }
  }

  void ClassRoom::OnTransportStateChange(int stream_id, TransportState state) {
    if(state == TRANSPORT_READY) {
      /*
       * Start audio mixer loop only when at least one user joined.
       */
      if(audio_mixer_element_ && !audio_mixer_element_->IsStarted()){
        audio_mixer_element_->Start();
      }
      RequestKeyFrameFromAllParticipants();
      LinkSsrcsToParticipants(stream_id);

      // Debug the GetStreamVideoMap, Enable this if you want to debug with the GstStreamVideoMap
      // TODO(chengxu): remove this code after getting the code running correctly.
      /*
      std::vector<TransportPlugin*> participants = GetParticipants();
      for (vector<TransportPlugin*>::iterator it = participants.begin(); it != participants.end(); ++it) {
        TransportPlugin* transport_plugin = *(it);
        ClassRoomPlugin* classroom_plugin = (ClassRoomPlugin*)transport_plugin;
        int stream_id = classroom_plugin->stream_id();
        olive::StreamVideoMapResponse stream_video_map_response;
        GetStreamVideoMap(classroom_plugin->stream_id(), &stream_video_map_response);
        LOG(INFO) << "stream_id=" << stream_id;
        LOG(INFO) << stream_video_map_response.DebugString();
      }
      */
    }
  }

  void ClassRoom::MuxRtpPacket(const int stream_id, const dataPacket& packet) {
    std::lock_guard<std::mutex> p_lock(classroom_plugins_mutex_);
    std::vector<TransportPlugin*> participants = GetParticipants();
    for (vector<TransportPlugin*>::iterator it = participants.begin(); it != participants.end(); ++it) {
      TransportPlugin* transport_plugin = *(it);
      ClassRoomPlugin* classroom_plugin = (ClassRoomPlugin*)transport_plugin;
      bool should_forward = false;
      std::lock_guard<std::mutex> p_lock(published_streams_mutex_);
      if(classroom_plugin->stream_id() == stream_id) {
        // Do not forward the stream to itself. Unless you are STUDENT and you are 
        // published...
        //if (published_streams_.find(stream_id) != published_streams_.end()) {
        //  should_forward = true;
        //} else {
        should_forward = false;
        //}
      } else if (master_streams_.find(stream_id) != master_streams_.end() ||
                 master_streams_.find(classroom_plugin->stream_id()) != master_streams_.end() ||
                 published_streams_.find(stream_id) != published_streams_.end()) {
        // The stream_id is in master stream_ids, indicate that it is a TEACHER
        should_forward = true;
      }
      if (should_forward) {
        classroom_plugin->RelayRtpPacket(packet);
      }
    }
  }

  void ClassRoom::IncomingRtpPacket(const int stream_id, const dataPacket& packet) {
    if (packet.type == AUDIO_PACKET) {
      audio_mixer_element_->PushPacket(stream_id, packet);
    } else if (packet.type == VIDEO_PACKET) {
      MuxRtpPacket(stream_id, packet);
    }
    // If the capture module is enabled, record/store the packet into the file.
    if (rtp_capture_ != NULL) {
      rtp_capture_->CapturePacket(stream_id, packet);
    }
    if(time_recorder_ != NULL) {
      time_recorder_->UpdateStream(stream_id);
    }
  }

  void ClassRoom::GetStreamVideoMap(const int stream_id,
                                    olive::StreamVideoMapResponse* stream_video_map) {
    std::lock_guard<std::mutex> p_lock(classroom_plugins_mutex_);
    std::vector<TransportPlugin*> participants = GetParticipants();
    for (vector<TransportPlugin*>::iterator it = participants.begin(); it != participants.end(); ++it) {
      TransportPlugin* transport_plugin = *(it);
      ClassRoomPlugin* classroom_plugin = (ClassRoomPlugin*)transport_plugin;
      if (classroom_plugin->stream_id() == stream_id) {
        classroom_plugin->GetStreamVideoMap(stream_video_map);
        break;
      }
    }
  }

}  // namespace orbit
