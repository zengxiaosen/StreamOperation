/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * orbit_media_pipelines.cc
 * ---------------------------------------------------------------------------
 * Implements the media processors.
 * ---------------------------------------------------------------------------
 */
#include "stream_service/proto/stream_service.grpc.pb.h"
#include "orbit_media_pipelines.h"

#include <vector>
#include <string>
#include <map>
#include <queue>
#include <thread>
#include <boost/thread/mutex.hpp>
#include <boost/make_shared.hpp>

#include "glog/logging.h"
#include "gflags/gflags.h"

#include <google/protobuf/text_format.h>
#include "stream_service/orbit/webrtc_endpoint.h"
#include "stream_service/orbit/nice_connection.h"
#include "stream_service/orbit/server/orbit_server_util.h"

// Includes the related plugin for the processor.
#include "stream_service/orbit/video_mixer/video_mixer_plugin.h"
#include "stream_service/orbit/video_dispatcher/video_dispatcher_plugin.h"
#include "stream_service/orbit/video_dispatcher/video_dispatcher_room.h"
#include "stream_service/orbit/transport_plugin.h"
#include "stream_service/orbit/echo_plugin.h"
#include "stream_service/orbit/classroom_plugin.h"
#include "stream_service/orbit/video_bridge_plugin.h"
#include "stream_service/orbit/audio_conference_plugin.h"
#include "stream_service/orbit/modules/audio_event_listener.h"
#include "stream_service/orbit/http_server/rpc_call_stats.h"

DECLARE_string(stun_server_ip);
DECLARE_int32(stun_server_port);
DECLARE_string(turn_server_ip);
DECLARE_int32(turn_server_port);
DECLARE_string(turn_server_username);
DECLARE_string(turn_server_pass);
DECLARE_int32(nice_min_port);
DECLARE_int32(nice_max_port);

// Only for debug purpose. In the future, the flag should not be used since each
// processor is supposed to use only one plugin.
// The value could be echo/bridge/audio_conference
DEFINE_string(use_plugin_name, "audio_conference",
              "Specify the plugin to use in this processor");
DEFINE_bool(use_webcast, true, 
            "The flags specifies whether we are using webcast.");

namespace olive {
  using namespace google::protobuf;
  using std::map;
  using std::queue;
  using std::string;
  using std::vector;
  using std::thread;

  namespace {
  }  // Annoymouse namespace

  namespace orbit_videocall {
    class Internal {
    public:
      struct StreamData {
        boost::mutex stream_data_mutex;
        int32 stream_id;
        orbit::WebRtcEndpoint* webrtc_endpoint;
        queue<IceCandidate> response_ice_candidates;
        bool ice_gathering_done;
      };

      Internal(int32_t session_id, string plugin_name);

      void SetCaptureData(bool capture) {
        if (capture) {
          orbit::ClassRoom* class_room = 
            dynamic_cast<orbit::ClassRoom*>(audio_video_room_.get());
          if (class_room != NULL) {
            class_room->EnableCapturePackets(true);
          }
        }
      }

      int CreateStream(const CreateStreamOption& option);
      bool CloseStream(int stream_id);
      bool CloseAll();
      bool AddIceCandidate(int stream_id, const IceCandidate& ice_candidate);
      bool ProcessSdpOffer(int stream_id, const string& sdp_offer, string* sdp_answer);
      void ReceiveIceCandidates(int stream_id, vector<IceCandidate>* ice_candidates);
      bool ProcessMessage(int stream_id, int message_type, void *data);
    private:
      const int32_t session_id_;
      string plugin_name_;
      orbit::WebRtcEndpoint* CreateMediaPipeline(StreamData* stream_data,
                                                 const CreateStreamOption& option);
      StreamData* GetStreamData(int32 stream_id) {
        auto stream_it = stream_data_.find(stream_id);
        if (stream_it == stream_data_.end()) {
          return NULL;
        }
        StreamData* stream_data_item = (*stream_it).second;
        return stream_data_item;
      }
      map<int32, StreamData*> stream_data_;
      std::shared_ptr<orbit::Room> audio_video_room_;
      boost::mutex room_mutex_;
    };  // class Internal
  } // namespace orbit_videocall


  class OrbitWebrtcEndpointListener : public orbit::WebRtcEndpointEventListener {
  public:
    OrbitWebrtcEndpointListener(orbit_videocall::Internal::StreamData* stream_data) {
      stream_data_ = stream_data;
    }
    ~OrbitWebrtcEndpointListener() {
    }
    void NotifyEvent(orbit::WebRTCEvent newEvent, const std::string& message) override {
      LOG(INFO) << "Event:" << WebRTCEventToString(newEvent) << " message:" << message;
      if (newEvent == orbit::CONN_CANDIDATE) {
        // New candidate get....
        IceCandidate tmp_candidate;
        if (!orbit::ParseJSONString(message, &tmp_candidate)) {
          LOG(ERROR) << "Parse the ice_candidate error.";
          return;
        }
        stream_data_->response_ice_candidates.push(tmp_candidate);
      }
    }
  private:
    orbit_videocall::Internal::StreamData* stream_data_;
  };

MediaPipeline::MediaPipeline(int32_t session_id) {
  internal_ = new orbit_videocall::Internal(session_id, FLAGS_use_plugin_name);
}
MediaPipeline::MediaPipeline(int32_t session_id, string plugin_name) {
  internal_ = new orbit_videocall::Internal(session_id, plugin_name);
}
MediaPipeline::~MediaPipeline() {
  // TODO(chengxu): release all the memory and resource.
  delete internal_;
}

void MediaPipeline::SetMediaPipelineOption(const MediaPipelineOption& option) {
  if (option.capture_data) {
    internal_->SetCaptureData(true);
  }
}

int MediaPipeline::CreateStream(const CreateStreamOption& option) {
  return internal_->CreateStream(option);
}
bool MediaPipeline::CloseStream(int stream_id) {
  return internal_->CloseStream(stream_id);
}

bool MediaPipeline::CloseAll() {
  return internal_->CloseAll();
}

bool MediaPipeline::AddIceCandidate(int stream_id, const IceCandidate& ice_candidate) {
  return internal_->AddIceCandidate(stream_id, ice_candidate);
}

bool MediaPipeline::ProcessSdpOffer(int stream_id, const string& sdp_offer, string* sdp_answer) {
  return internal_->ProcessSdpOffer(stream_id, sdp_offer, sdp_answer);
}
void MediaPipeline::ReceiveIceCandidates(int stream_id, vector<IceCandidate>* ice_candidates) {
  internal_->ReceiveIceCandidates(stream_id, ice_candidates);
}

bool MediaPipeline::ProcessMessage(int stream_id, int message_type, void *data) {
  return internal_->ProcessMessage(stream_id, message_type, data);
}

bool MediaPipeline::CloseInvalidStream(int stream_id) {
  return internal_->CloseStream(stream_id);;
}

  namespace orbit_videocall {
    Internal::Internal(int32_t session_id, string plugin_name)
      : session_id_(session_id) {
      plugin_name_ = plugin_name;
      if(plugin_name_ == "video_mixer"){
        audio_video_room_ = std::make_shared<orbit::VideoMixerRoom>(session_id_, FLAGS_use_webcast);
      } else if(plugin_name_ == "video_dispatcher") {
        audio_video_room_ = std::make_shared<orbit::VideoDispatcherRoom>();
      } else if(plugin_name_ == "audio_mixer") {
        audio_video_room_ = std::make_shared<orbit::AudioConferenceRoom>(session_id_, false, false);
      } else if(plugin_name_ == "bridge") {
        audio_video_room_ = std::make_shared<orbit::Room>();
      } else if(plugin_name_ == "classroom") {
        audio_video_room_ = std::make_shared<orbit::ClassRoom>(session_id_);
      } else {
        audio_video_room_ = std::make_shared<orbit::AudioConferenceRoom>(session_id_, FLAGS_use_webcast, true);
      }

      if (plugin_name_ != "simple_echo") {
        audio_video_room_->Create();
        audio_video_room_->Start();
      }
    }

    orbit::WebRtcEndpoint* Internal::CreateMediaPipeline(StreamData* stream_data,
                                                         const CreateStreamOption& option) {
      bool audio_enabled = true;
      bool video_enabled = true;
      bool trickle_enabled = true;
      orbit::IceConfig ice_config;

      if (!FLAGS_stun_server_ip.empty()) {
        ice_config.stunServer = FLAGS_stun_server_ip;
        ice_config.stunPort = FLAGS_stun_server_port;
      }

      int stream_id = stream_data->stream_id;

      if (option.use_relay()) {
        ice_config.use_relay_mode = true;
        if (!FLAGS_turn_server_ip.empty()) {
          ice_config.turnServer = FLAGS_turn_server_ip;
          ice_config.turnPort = FLAGS_turn_server_port;
          ice_config.turnUsername = FLAGS_turn_server_username;
          ice_config.turnPass = FLAGS_turn_server_pass;
        }
        if (!option.relay_server_info().turn_server().empty()) {
          ice_config.turnServer = option.relay_server_info().turn_server();
          ice_config.turnPort = option.relay_server_info().port();
          ice_config.turnUsername = option.relay_server_info().username();
          ice_config.turnPass = option.relay_server_info().credential();    
        }
      }

      if (FLAGS_nice_min_port && FLAGS_nice_max_port) {
        ice_config.minPort = (uint16_t)FLAGS_nice_min_port;
        ice_config.maxPort = (uint16_t)FLAGS_nice_max_port;
        LOG(INFO) <<" ice_config.minPort = " << ice_config.minPort
                  <<" ice_config.maxPort = " << ice_config.maxPort;
      }

      OrbitWebrtcEndpointListener* listener =
        new OrbitWebrtcEndpointListener(stream_data);
      LOG(INFO) << "Create Connection plugin_name is "<<plugin_name_;

      orbit::WebRtcEndpoint* endpoint =
        new orbit::WebRtcEndpoint(audio_enabled, video_enabled,
                                  trickle_enabled, ice_config, session_id_, stream_id);
      if (option.target_bandwidth() != 0) {
        endpoint->set_target_bandwidth(option.target_bandwidth());
      }
      endpoint->setWebRtcEndpointEventListener(listener);
      // Try to test the video_encoding and min_encoding_bitrate parameter,
      // Use the following statements to replace the below two lines.
      //  endpoint->set_video_encoding(olive::VideoEncoding::H264);
      //  endpoint->set_min_encoding_bitrate(55);
      endpoint->set_video_encoding(option.video_encoding());
      endpoint->set_min_encoding_bitrate(option.min_encoding_bitrate());

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
        plugin = new orbit::VideoBridgePlugin(audio_video_room_, stream_id);
      } else if (plugin_name_ == "video_mixer") {
        //TODO change pulgin_impl to plugin.
        plugin = new orbit::VideoMixerPluginImpl(audio_video_room_, stream_id, 0, 0);
      } else if (plugin_name_ == "video_dispatcher") {
        plugin = new orbit::VideoDispatcherPlugin(audio_video_room_, stream_id);
      } else if (plugin_name_ == "classroom") {
        orbit::ClassRoomPlugin* classroom_plugin = new orbit::ClassRoomPlugin(audio_video_room_, stream_id);
        plugin = dynamic_cast<orbit::TransportPlugin*>(classroom_plugin);
        if (option.role_type() == olive::CreateStreamOption::ORBIT_TEACHER) {
          classroom_plugin->SetPluginRole(orbit::ClassRoomPlugin::TEACHER);
        } else {
          classroom_plugin->SetPluginRole(orbit::ClassRoomPlugin::STUDENT);
        }
        endpoint->set_should_rewrite_ssrc(false);
      } else {
        plugin = new orbit::AudioConferencePlugin(audio_video_room_, stream_id);
      }

      endpoint->set_plugin(plugin);

      audio_video_room_->AddParticipant(plugin);
      return endpoint;
    }

    int Internal::CreateStream(const CreateStreamOption& option) {
      int stream_id = rand();
      StreamData *data = GetStreamData(stream_id);
      while(data) {
        stream_id = rand();
        data = GetStreamData(stream_id);
      }
      {
        boost::mutex::scoped_lock lock(room_mutex_);
        StreamData* new_stream_data_item = new StreamData;
        {
          boost::mutex::scoped_lock lock(new_stream_data_item->stream_data_mutex);
          new_stream_data_item->stream_id = stream_id;
          stream_data_.insert(std::pair<int32, StreamData*>(stream_id, new_stream_data_item));
          new_stream_data_item->webrtc_endpoint = CreateMediaPipeline(new_stream_data_item, option);
        }
      }
      return stream_id;
    }

    bool Internal::CloseStream(int stream_id) {
      LOG(INFO) << "CloseStream()";
      StreamData *stream_data = nullptr;
      orbit::WebRtcEndpoint  *endpoint = nullptr;
      orbit::TransportPlugin *plugin = nullptr;

      {
        boost::mutex::scoped_lock lock(room_mutex_);
        stream_data = GetStreamData(stream_id);
        if (!stream_data) {
          LOG(ERROR) << "CloseStream: stream_id is invalid.";
          return false;
        }
        stream_data_.erase(stream_id);

        endpoint = stream_data->webrtc_endpoint;
        if (endpoint) {
          plugin = endpoint->get_plugin();
          if (plugin && audio_video_room_) {
            audio_video_room_->RemoveParticipant(plugin);
          }
        }
      }

      thread([stream_data, endpoint, plugin]{
        orbit::RpcCallStats rpc_stat("OrbitStreamServiceImpl_CloseStreamThread");
        if (endpoint) {
          endpoint->Stop();
        }
        if (plugin) {
          plugin->Stop();
        }
        delete endpoint;
        delete plugin;
        delete stream_data;
      }).detach();

      LOG(INFO) << "End of Close stream";
      return true;
    }

    bool Internal::CloseAll() {
      std::map<int32, StreamData*> map;
      {
        boost::mutex::scoped_lock lock(room_mutex_);

        if (audio_video_room_) {
          for (auto &pair : stream_data_) {
            StreamData *stream_data = pair.second;
            if (stream_data) {
              orbit::WebRtcEndpoint *endpoint = stream_data->webrtc_endpoint;
              if (endpoint) {
                orbit::TransportPlugin *plugin = endpoint->get_plugin();
                if (plugin && audio_video_room_) {
                  audio_video_room_->RemoveParticipant(plugin);
                }
              }
            }
          }
        }

        map.swap(stream_data_);
      }

      thread([map]{
        orbit::RpcCallStats rpc_stat("OrbitStreamServiceImpl_CloseSessionThread");
        for (auto &pair : map) {
          orbit::WebRtcEndpoint *endpoint = nullptr;
          orbit::TransportPlugin *plugin = nullptr;
          StreamData *stream_data = pair.second;
          if (stream_data) {
            endpoint = stream_data->webrtc_endpoint;
            if (endpoint) {
              endpoint->Stop();
              plugin = endpoint->get_plugin();
              if (plugin) {
                plugin->Stop();
              }
            }
          }
          delete endpoint;
          delete plugin;
          delete stream_data;
        }
      }).detach();

      return true;
    }

    bool Internal::ProcessSdpOffer(int stream_id,
                                   const string& sdp_offer,
                                   string* sdp_answer) {
      VLOG(3) << "sdp_offer=" << sdp_offer;
      StreamData* stream_data_item = GetStreamData(stream_id);
      if (stream_data_item == NULL) {
        LOG(ERROR) << "ProcessSdpOffer: stream_id is invalid.";
        return false;
      }

      {
        boost::mutex::scoped_lock lock(stream_data_item->stream_data_mutex);
        orbit::WebRtcEndpoint* conn = stream_data_item->webrtc_endpoint;

        if (conn->ProcessOffer(sdp_offer, sdp_answer)) {
          VLOG(3) << "sdp_answer=" << *sdp_answer;
          return true;
        } else {
          LOG(ERROR) << "process offer failed.";
        }
      }
      return false;
    }

    bool Internal::AddIceCandidate(int stream_id,
                                   const olive::IceCandidate& ice_candidate) {
      VLOG(3) << "AddIceCandidate:stream_id=" << stream_id;
      string str;
      google::protobuf::TextFormat::PrintToString(ice_candidate, &str);
      VLOG(3) << str;

      StreamData* stream_data_item = GetStreamData(stream_id);
      if (stream_data_item == NULL) {
        LOG(ERROR) << "AddIceCandidate: stream_id is invalid.";
        return false;
      }
      {
        boost::mutex::scoped_lock lock(stream_data_item->stream_data_mutex);
        orbit::WebRtcEndpoint* conn = stream_data_item->webrtc_endpoint;
        string ice_str = ice_candidate.ice_candidate();
        string sdp_mid = ice_candidate.sdp_mid();
        int32 sdp_m_line_index = ice_candidate.sdp_m_line_index();
        ice_str = "a=" + ice_str;
        conn->AddRemoteCandidate(sdp_mid, sdp_m_line_index, ice_str);
      }
      return true;
    }

    void Internal::ReceiveIceCandidates(int stream_id,
                                        vector<IceCandidate>* ice_candidates) {
      StreamData* stream_data_item = GetStreamData(stream_id);
      if (stream_data_item == NULL) {
        LOG(ERROR) << "AddIceCandidate: stream_id is invalid.";
        return;
      }
      
      {
        boost::mutex::scoped_lock lock(stream_data_item->stream_data_mutex);
        while(!stream_data_item->response_ice_candidates.empty()) {
          IceCandidate cand = stream_data_item->response_ice_candidates.front();
          ice_candidates->push_back(cand);
          VLOG(3) << "@@@@@@@@@@ice_candidate_str" << cand.ice_candidate();
          stream_data_item->response_ice_candidates.pop();
        }
      }
      return;
    }

    bool Internal::ProcessMessage(int stream_id, int message_type, void *data) {
      switch(message_type){
      case SET_RTMP_LOCATION:
        if(orbit::IsValidRtmpLocation(((string*)data)->c_str())){
          // TODO(chengxu): set the need_return_video = false because live_broadcast
          // does not need to return the video right now.
          bool support_webcast = true;
          bool need_return_video = false;
          const char* rtmp_location = ((string*)data)->c_str();
          audio_video_room_->SetupLiveStream(support_webcast,
                                             need_return_video,
                                             rtmp_location);
        }
        break;
      case MUTE_STREAM:{
        orbit::IAudioEventListener* audio_event_listener =
            dynamic_cast<orbit::IAudioEventListener*>(audio_video_room_.get());
        if(audio_event_listener != NULL) {
          const MuteStreams* streams = (MuteStreams*) data;
          const bool mute = streams->mute();
          int size = streams->stream_ids_size();
          for(int i=0; i<size; i++){
            const int stream_id = streams->stream_ids(i);
            audio_event_listener->MuteStream(stream_id, mute);
          }
        }
        }
        break;
      case RAISE_HAND:{
          // ProcessMessage is useful for passing the user-defined message to the
          // corresponding plugins and rooms. For now, the only user-defined message
          // for the audio_conference_room is "Raise_Hand" message. The raise_hand message
          // indicates that an participant in the meeting is raising the hand for speak out.
          // Thus it will force the gateway and the participant plugins to switch to the new
          // publisher.
          StreamData *stream_data_item = GetStreamData(stream_id);
          if(stream_data_item == NULL) {
            return false;
          }
          std::shared_ptr<orbit::AudioConferenceRoom> av_room = std::static_pointer_cast<orbit::AudioConferenceRoom>(audio_video_room_);
          if(av_room != NULL) {
            return av_room->SwitchPublisher(stream_id);
          }
        }
        break;
      case GET_NET_STATUS:{
          boost::mutex::scoped_lock lock(room_mutex_);
          StreamData *stream_data = GetStreamData(stream_id);
          if (stream_data == NULL) {
            return false;
          }
          orbit::WebRtcEndpoint* webrtc_endpoint = stream_data->webrtc_endpoint;
          if (webrtc_endpoint != NULL) {
            *(olive::ConnectStatus*)data = 
	            webrtc_endpoint->GetConnectStatusInfo();
          }
        }
        break;
      case SET_DISPATCHER_PUBLISHER: {
          orbit::VideoDispatcherRoom* vd_room = 
	          dynamic_cast<orbit::VideoDispatcherRoom*>(audio_video_room_.get());
          if (vd_room != NULL) {
            return vd_room->ProcessMessage(stream_id, 2, NULL);
          }
        }
        break;
      case GET_STREAM_VIDEO_MAP: {
          olive::StreamVideoMapResponse* stream_video_map_response =
            static_cast<olive::StreamVideoMapResponse*>(data);
          orbit::ClassRoom* class_room = 
	          dynamic_cast<orbit::ClassRoom*>(audio_video_room_.get());
          if (class_room != NULL) {
            class_room->GetStreamVideoMap(stream_id, stream_video_map_response);
          }
        }
        break;
      case PUBLISH_STREAMS: {
        orbit::ClassRoom* class_room = 
	          dynamic_cast<orbit::ClassRoom*>(audio_video_room_.get());
        if (class_room != NULL) {
          const PublishStreams* publish_streams = (PublishStreams*) data;
          assert(publish_streams->published() == true);
          int size = publish_streams->stream_ids_size();
          std::vector<int> to_publish_stream_ids;
          for(int i=0; i<size; i++){
            const int stream_id = publish_streams->stream_ids(i);
            to_publish_stream_ids.push_back(stream_id);
          }
          return class_room->PublishStreams(to_publish_stream_ids);
        }

      }
      break;
      default:
        break;
      }
      return false;
    }
  }  // orbit_videocall

}  // namespace olive
