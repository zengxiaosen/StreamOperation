/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * orbit_stream_service_impl.cc
 * ---------------------------------------------------------------------------
 * Implements the stream service interface defined in the stream_service.proto.
 * ---------------------------------------------------------------------------
 */
#include <boost/thread/mutex.hpp>
#include <boost/make_shared.hpp>

#include "orbit_stream_service_impl.h"
#include "orbit_media_pipelines.h"

#include "stream_service/orbit/base/timeutil.h"
#include "stream_service/orbit/base/session_info.h"
#include "stream_service/orbit/network_status_common.h"
#include "stream_service/orbit/http_server/rpc_call_stats.h"
#include "stream_service/orbit/network_status.h"

#include <google/protobuf/text_format.h>
#include "glog/logging.h"
#include "gflags/gflags.h"

#include <map>
#include <queue>
#include <string>
#include <vector>
#include <sys/prctl.h>

namespace olive {

  using std::map;
  using std::queue;
  using std::string;
  using std::vector;
  using orbit::RpcCallStats;

  using namespace google::protobuf;
  namespace {  // annoymous namespace
    
    enum SessionState {
      SESSION_ACTIVE = 0,
      SESSION_CLOSED = 1,
      SESSION_DESTROYED = 2
    };
    class SessionData {
      public:
        int32 session_id;
        MediaPipeline *pipeline;
        boost::mutex mutex;

        SessionData() {
          pipeline = NULL;
          state = SESSION_ACTIVE;
          destroy_time = 0;
        }
        
        bool IsActive() {
          return SESSION_ACTIVE == state;
        }

        void Close() {
          state = SESSION_CLOSED; 
        }

        // check if the session data can be recycled
        bool CanRecycle() {
          if (SESSION_DESTROYED == state) {
            long long now = orbit::getTimeMS();  
            int timeout = 60000; // 60,000 ms for 60 seconds
            if (now - destroy_time > timeout) {
              return true; 
            }
          }
          return false;
        }

        // close the stream
        void CloseInvalidStream(int32_t session_id, int32_t stream_id) {
          pipeline->CloseInvalidStream(stream_id);

          //update statusz
          orbit::SessionInfoManager* session_info = 
            Singleton<orbit::SessionInfoManager>::GetInstance();
          orbit::SessionInfoPtr session = session_info->GetSessionInfoById(session_id);
          if(session != NULL) {
            session->RemoveStream(stream_id);
          }
        }

        // If all the streams in this session are closed, then close the session.
        void CheckAndRecycleSession() {
          orbit::SessionInfoManager* session_info = 
            Singleton<orbit::SessionInfoManager>::GetInstance();
          orbit::SessionInfoPtr session = session_info->GetSessionInfoById(session_id);
          // Check the live stream
          bool has_stream = session->HasLiveStreams();
          if (has_stream) {
            return;
          }
          
          // Recycle the session
          if (IsActive()) {
            Close();
          } else {
            return;
          }

          boost::thread([this] () {
            prctl(PR_SET_NAME, (unsigned long)"RecycleSessionThread");
            if (pipeline != NULL) {
              LOG(INFO) << "Recycle the session : session_id = " << session_id;
              orbit::SessionInfoManager* session_info = 
                Singleton<orbit::SessionInfoManager>::GetInstance();
              session_info->RemoveSession(session_id);

              pipeline->CloseAll();
            }
            Destroy();
          }).detach();
        }
      
        void Destroy() {
          if (pipeline != NULL) {
            delete pipeline;
            pipeline = NULL;
          }
          destroy_time = orbit::getTimeMS();
          state = SESSION_DESTROYED;
        }
      private:
        // control information
        SessionState state;
        long long destroy_time; 
    };

    // Map <session_id, SessionData>
    map<int32, SessionData*> session_data;
    std::mutex session_data_mutex_;
    boost::thread watch_dog_thread;
    bool running = false;

    SessionData* GetSessionData(int32 session_id) {
      std::lock_guard<std::mutex> guard(session_data_mutex_);
      auto it = session_data.find(session_id);
      if (it == session_data.end()) {
        return NULL;
      }
      SessionData* session_data_item = (*it).second;
      return session_data_item;
    }
  
    void WatchDogThread() {
      prctl(PR_SET_NAME, (unsigned long)"WatchDogThread");
      long waiting_time = 10000000; // 10,000,000 us for 10 seconds
      while (running) {
        usleep(waiting_time); 
        VLOG(2) << "WatchDogThread...sessions size is " << session_data.size();

        // traversal all stream
        std::vector<orbit::NetworkStatusManager::Key> remove_stream;
        orbit::NetworkStatusManager::TraverseStreamMap(&remove_stream);
        for (auto ite = remove_stream.begin(); ite != remove_stream.end(); ite ++) {
          orbit::NetworkStatusManager::Key key = *ite;

          {
            SessionData* session_data_temp = GetSessionData(key.session_id());
            if (session_data_temp != NULL) {
              boost::mutex::scoped_lock lock(session_data_temp->mutex);
              session_data_temp->CloseInvalidStream(key.session_id(), key.stream_id());

              // The session has no live streams, then recycle the session
              session_data_temp->CheckAndRecycleSession();
            }
          }
        }
        {
          std::lock_guard<std::mutex> guard(session_data_mutex_);

          std::vector<std::pair<int32, SessionData *>> should_remove;

          for (auto &pair : session_data) {
            int32 session_id = pair.first;
            SessionData *session_data_item = pair.second;
            if (session_data_item) {
              if (session_data_item->CanRecycle()) {
                should_remove.push_back(pair);
                continue;
              }
            } else {
              LOG(ERROR) << "An invalid session in sessions pool. session id = " << session_id;
              continue;
            }
          }

          for (auto &pair : should_remove) {
            int32 session_id = pair.first;
            SessionData *session_data_item = pair.second;
            delete session_data_item;
            session_data.erase(session_id);
          }
        }
      } // end while
    }

    void StartWatchDogThread() {
      running = true;
      watch_dog_thread = boost::thread(&WatchDogThread);
    }

    void StopWatchDogThread() {
      running = false;
      watch_dog_thread.join();
    }

    void InsertSessionData(int32 session_id, MediaPipeline *pipeline) {
      SessionData* new_session_data_item = new SessionData;
      new_session_data_item->session_id = session_id;
      new_session_data_item->pipeline = pipeline;
      {
        std::lock_guard<std::mutex> guard(session_data_mutex_);
        session_data.insert(std::pair<int32, SessionData*>(session_id, new_session_data_item));
      }
    }

    MediaPipeline* GetPipeline(int32 session_id) {
      SessionData* session = GetSessionData(session_id);
      if (session == NULL)
        return NULL;
      return session->pipeline;
    }
  }  // annoymous namespace

  void OrbitStreamServiceImpl::Init() {
    srand (time(NULL));
    StartWatchDogThread();
  }

  OrbitStreamServiceImpl::~OrbitStreamServiceImpl() {
    StopWatchDogThread();
  }

// virtual
grpc::Status OrbitStreamServiceImpl::CreateSession(grpc::ServerContext* context,
                                              const CreateSessionRequest* request,
                                              CreateSessionResponse* response) {
  RpcCallStats rpc_stat("OrbitStreamServiceImpl_CreateSession");

  std::string str;
  google::protobuf::TextFormat::PrintToString(*request, &str);

  int session_id = rand();
  SessionData *data = GetSessionData(session_id);
  while(data) { // Note: There are conflicts in concurrent situations. 
    session_id = rand();
    data = GetSessionData(session_id); 
  }

  LOG(INFO) << "session_id=" << session_id;
  MediaPipeline* pipeline = GetPipeline(session_id);
  if (pipeline == NULL) {
    // start session info for statuz
    orbit::SessionInfoManager* session_info = 
      Singleton<orbit::SessionInfoManager>::GetInstance();
    session_info->AddSession(session_id);
    // end session info for statuz
    LOG(INFO)<<"CreatePipelineType = "<<request->type();

    switch (request->type()) {
    case CreateSessionRequest::VIDEO_BRIDGE:{
      LOG(INFO) << "request_type=VIDEO_BRIDGE";
      pipeline = new MediaPipeline(session_id);
      break;
    }
    case CreateSessionRequest::AUDIO_MIXER_BRIDGE:{
      LOG(INFO) << "request_type=AUDIO_MIXER_BRIDGE";
      pipeline = new MediaPipeline(session_id, "audio_mixer");
      break;
    }
    case CreateSessionRequest::VIDEO_MIXER_BRIDGE:{
      pipeline = new MediaPipeline(session_id, "video_mixer");
      string rtmp_location = request->rtmp_location();
      pipeline->ProcessMessage(DEFAULT_STREAM_ID, SET_RTMP_LOCATION, &rtmp_location);
      break;
    }
    case CreateSessionRequest::VIDEO_LIVE_BRIDGE:{
      LOG(INFO) << "request_type=VIDEO_LIVE_BRIDGE";
      pipeline = new MediaPipeline(session_id, "audio_conference");
      string rtmp_location = request->rtmp_location();
      pipeline->ProcessMessage(DEFAULT_STREAM_ID, SET_RTMP_LOCATION, &rtmp_location);
      break;
    }
    case CreateSessionRequest::VIDEO_DISPATCHER:
    {
      pipeline = new MediaPipeline(session_id, "video_dispatcher");
      string rtmp_location = request->rtmp_location();
      pipeline->ProcessMessage(DEFAULT_STREAM_ID, SET_RTMP_LOCATION, &rtmp_location);
      break;
    }
    case CreateSessionRequest::ORBIT_CLASSROOM:
    {
      pipeline = new MediaPipeline(session_id, "classroom");
      break;      
    } 
    default:
      LOG(INFO) << "request_type=" << request->type() <<" , not implemented yet.";
      rpc_stat.Fail();
      return grpc::Status::CANCELLED;
    }

    if (request->capture_data()) {
      MediaPipelineOption option;
      option.capture_data = true;
      pipeline->SetMediaPipelineOption(option);
    }
  }
  
  InsertSessionData(session_id, pipeline);
  response->set_status(CreateSessionResponse::OK);
  response->set_session_id(session_id);
  return grpc::Status::OK;
}

//  virtual
grpc::Status OrbitStreamServiceImpl::CloseSession(grpc::ServerContext* context,
                                             const CloseSessionRequest* request,
                                             CloseSessionResponse* response) {
  RpcCallStats rpc_stat("OrbitStreamServiceImpl_CloseSession");

  int session_id = request->session_id(); 
  LOG(INFO) <<"OrbitStreamServiceImpl CloseSession." 
            <<"session_id = " << session_id;

  SessionData *session_data_item = GetSessionData(session_id); 
  if (session_data_item != NULL) { 
    LOG(INFO)<<"session_data_item != NULL";
    boost::mutex::scoped_lock lock(session_data_item->mutex);
    if (!session_data_item->IsActive()) {
      response->set_session_id(session_id);
      rpc_stat.Fail();
      return grpc::Status::OK;
    } else {
      session_data_item->Close();
    }
    MediaPipeline *pipeline = session_data_item->pipeline;
    if (pipeline != NULL) {
      LOG(INFO)<<"pipeline != NULL";

      // start session info for statusz
      orbit::SessionInfoManager* session_info = 
        Singleton<orbit::SessionInfoManager>::GetInstance();
      session_info->RemoveSession(session_id);
      // end session info for statuz

      pipeline->CloseAll();
    }
    session_data_item->Destroy();
  }
  response->set_session_id(session_id);
  return grpc::Status::OK;
}

// virtual
grpc::Status OrbitStreamServiceImpl::CreateStream(grpc::ServerContext* context,
                                             const CreateStreamRequest* request,
                                             CreateStreamResponse* response) {
  RpcCallStats rpc_stat("OrbitStreamServiceImpl_CreateStream");

  std::string str;
  google::protobuf::TextFormat::PrintToString(*request, &str);
  LOG(INFO) << "request=" << str;

  int session_id = request->session_id(); 
  SessionData *session_data_item = GetSessionData(session_id); 
  if (session_data_item) {
    boost::mutex::scoped_lock lock(session_data_item->mutex);
    if (!session_data_item->IsActive()) {
      response->set_status(CreateStreamResponse::ERROR);
      response->set_error_message("session was closed.");
      rpc_stat.Fail();
      return grpc::Status::OK;
    }
    MediaPipeline* pipeline = GetPipeline(session_id);
    if (pipeline == NULL) {
      response->set_status(CreateStreamResponse::ERROR);
      response->set_error_message("pipeline not found.");
      rpc_stat.Fail();
      return grpc::Status::OK;
    }

    LOG(INFO) << "pipeline_=" << pipeline;
    int stream_id = pipeline->CreateStream(request->option());

    // start session info for statuz
    orbit::SessionInfoManager* session_info = 
          Singleton<orbit::SessionInfoManager>::GetInstance();
    orbit::SessionInfoPtr session = session_info->GetSessionInfoById(session_id);
    if(session != NULL) {
      session->AddStream(stream_id);
    }
    // end session info for statuz
    
    response->set_status(CreateStreamResponse::OK);
    response->set_stream_id(stream_id);
    return grpc::Status::OK;
  } else {
    response->set_status(CreateStreamResponse::ERROR);
    response->set_error_message("session not found.");
    rpc_stat.Fail();
    return grpc::Status::OK;
  }
}

//  virtual
grpc::Status OrbitStreamServiceImpl::CloseStream(grpc::ServerContext* context,
                                            const CloseStreamRequest* request,
                                            CloseStreamResponse* response) {
  RpcCallStats rpc_stat("OrbitStreamServiceImpl_CloseStream");

  int session_id = request->session_id(); 
  int stream_id  = request->stream_id();
  SessionData *session_data_item = GetSessionData(session_id);
  if (session_data_item) {
    boost::mutex::scoped_lock lock(session_data_item->mutex);
    if (!session_data_item->IsActive()) {
      rpc_stat.Fail();
      return grpc::Status::OK;
    }
    MediaPipeline *pipeline = session_data_item->pipeline; 
    if (pipeline) {
      pipeline->CloseStream(stream_id);
    } else {
      LOG(ERROR) << "Invalid pipeline for close stream.";
    }
  }
  // start session info for statuz  
  orbit::SessionInfoManager* session_info = 
  Singleton<orbit::SessionInfoManager>::GetInstance();
  orbit::SessionInfoPtr session = session_info->GetSessionInfoById(session_id);
  if(session != NULL) {
    session->RemoveStream(stream_id);
  }
  // end session info for statuz 
  return grpc::Status::OK;
}

grpc::Status OrbitStreamServiceImpl::SendSessionMessage(grpc::ServerContext* context,
                                                        const SendSessionMessageRequest* request,
                                                        SendSessionMessageResponse* response) {
  RpcCallStats rpc_stat("OrbitStreamServiceImpl_SendSessionMessage", std::to_string(request->type()));

  // Fill in the common fields in the response.
  response->set_session_id(request->session_id());
  response->set_stream_id(request->stream_id());

  // Look up from the request->session_id and stream_id, to find the webrtc element.
  int session_id = request->session_id();
  int stream_id = request->stream_id();

  SessionData *session_data_item = GetSessionData(session_id);
  if (session_data_item) {
    boost::mutex::scoped_lock lock(session_data_item->mutex);
    if (!session_data_item->IsActive()) {
      response->set_status(SendSessionMessageResponse::ERROR);
      response->set_error_message("session was closed.");
      rpc_stat.Fail();
      return grpc::Status::OK;
    }
    MediaPipeline* pipeline = GetPipeline(session_id);
    if(pipeline == NULL) {
      response->set_status(SendSessionMessageResponse::ERROR);
      response->set_error_message("pipeline not found.");
      rpc_stat.Fail();
      return grpc::Status::OK;
    }

    // process messages
    switch (request->type()) {
    case ICE_CANDIDATE: {
      IceCandidate candidate = request->candidate();
      if (!pipeline->AddIceCandidate(stream_id, candidate)) {
        response->set_status(SendSessionMessageResponse::ERROR);
        response->set_error_message("AddIceCandidate has error.");
        rpc_stat.Fail();
        return grpc::Status::OK;
      }
      response->set_type(MessageResponseType::ICE_CANDIDATE_ANSWER);
      break;
    };
    case SDP_OFFER: {
      string sdp_offer = request->sdp_offer();
      string sdp_answer;
      if (!pipeline->ProcessSdpOffer(stream_id, sdp_offer, &sdp_answer)) {
        response->set_status(SendSessionMessageResponse::ERROR);
        response->set_error_message("ProcessSdpOffer has error.");
        rpc_stat.Fail();
        return grpc::Status::OK;
      }
      response->set_type(MessageResponseType::SDP_ANSWER);
      response->set_sdp_answer(sdp_answer);
      break;
    };
    case MESSAGE: {
      string message = request->message();
      MuteStreams streams = request->mute_streams();
      int size = streams.stream_ids_size();
      LOG(INFO) << "receive a message from the stream:(" << message << ")";
      if (message.compare("raise_hand")==0) {      // RAISE_HAND type
        pipeline->ProcessMessage(stream_id, RAISE_HAND, NULL);
        response->set_message_answer("OK");
        response->set_type(MessageResponseType::MESSAGE_ANSWER);
      } else if (size > 0) { //MUTE
        // If mute streams size is not 0, we mute the streams.
        pipeline->ProcessMessage(DEFAULT_STREAM_ID, MUTE_STREAM, &streams);
        response->set_type(MessageResponseType::MUTE_ANSWER);
      } else {                                    // UNKNOWN type
        LOG(INFO) <<"unknown message received.";
        pipeline->ProcessMessage(stream_id, 1, NULL);
        response->set_message_answer("OK");
        response->set_type(MessageResponseType::MESSAGE_ANSWER);
      }
      break;
    };
    case MUTE: { //mute is useless type ?
      MuteStreams streams = request->mute_streams();
      pipeline->ProcessMessage(DEFAULT_STREAM_ID, MUTE_STREAM, &streams);
      response->set_type(MessageResponseType::MUTE_ANSWER);
      break;
    };
    case STREAM_INFO: {
      response->set_message_answer("OK");
      response->set_type(MessageResponseType::STREAM_INFO_ANSWER);
  
      orbit::SessionInfoManager* session_info = 
          Singleton<orbit::SessionInfoManager>::GetInstance();
      orbit::SessionInfoPtr session = session_info->GetSessionInfoById(session_id);
      if(session != NULL) {
        string message = request->message();
        session->SetMoreInfo(stream_id, message);
      }
      break;
    };
    case PUBLISH_STREAMS: {
      PublishStreams publish_streams = request->publish_streams();
      pipeline->ProcessMessage(DEFAULT_STREAM_ID, PUBLISH_STREAMS, &publish_streams);
      response->set_type(MessageResponseType::PUBLISH_STREAMS_ANSWER);
      response->set_message_answer("OK");
      break;
    };
    case GET_STREAM_VIDEO_MAP: {
      olive::StreamVideoMapResponse stream_video_map_response;
      pipeline->ProcessMessage(stream_id, GET_STREAM_VIDEO_MAP, &stream_video_map_response);
      response->set_type(MessageResponseType::GET_STREAM_VIDEO_MAP_ANSWER);
      response->mutable_stream_video_map()->CopyFrom(stream_video_map_response);
      response->set_message_answer("OK");
      break;
    };
    case PROBE_NET: {
      olive::ProbeNetTestResult test_result;
      orbit::NetworkStatusManager::GetProbeNetResponse(session_id, stream_id, &test_result);
      LOG(INFO) << "Probe_net called." << test_result.DebugString();
      response->mutable_probe_net_result()->CopyFrom(test_result);
      response->set_type(MessageResponseType::PROBE_NET_ANSWER);
      response->set_message_answer("OK");
      LOG(INFO) << "response=" << response->DebugString();
      break;
    }
    case SET_PUBLISHER: {
      bool ret = pipeline->ProcessMessage(stream_id, SET_DISPATCHER_PUBLISHER, NULL);
      response->set_type(MessageResponseType::SET_PUBLISHER_ANSWER);
      if (ret) {
        response->set_message_answer("OK");
      } else {
        response->set_message_answer("FAILED");
        rpc_stat.Fail();
      }
      break;
    };
    default: {
      LOG(ERROR) << "Process Message: Type Unknown..." << request->type();
      response->set_error_message("unknown request type.");
      response->set_message_answer("FAILED");
      rpc_stat.Fail();
      break;
    }
    }
    return grpc::Status::OK;
  } else {
    response->set_status(SendSessionMessageResponse::ERROR);
    response->set_error_message("session not found.");
    rpc_stat.Fail();
    return grpc::Status::OK;
  }
}

  //virtual
grpc::Status OrbitStreamServiceImpl::RecvSessionMessage(grpc::ServerContext* context,
                                                        const RecvSessionMessageRequest* request,
                                                        RecvSessionMessageResponse* response) {
  RpcCallStats rpc_stat("OrbitStreamServiceImpl_RecvSessionMessage", std::to_string(request->type()));

  // Look up from the request->session_id and stream_id, to find the webrtc element.
  int session_id = request->session_id();
  int stream_id = request->stream_id();

  // Fill in the common fields in the response.
  response->set_session_id(session_id);
  response->set_stream_id(stream_id);

  SessionData *session_data_item = GetSessionData(session_id); 
 
  if (session_data_item) {
    boost::mutex::scoped_lock lock(session_data_item->mutex);
    if (!session_data_item->IsActive()) {
      response->set_status(RecvSessionMessageResponse::ERROR);
      response->set_error_message("session was closed.");
      rpc_stat.Fail();
      return grpc::Status::OK;
    }
    MediaPipeline* pipeline = GetPipeline(session_id);
    if(pipeline == NULL) {
      response->set_status(RecvSessionMessageResponse::ERROR);
      response->set_error_message("Pipeline not found.");
      rpc_stat.Fail();
      return grpc::Status::OK;
    }
    // process messages
    switch (request->type()) {
    case ICE_CANDIDATE: {
      response->set_type(MessageResponseType::ICE_CANDIDATE_ANSWER);
      vector<IceCandidate> ice_candidates;
      pipeline->ReceiveIceCandidates(stream_id, &ice_candidates);
      for (auto it = ice_candidates.begin(); it != ice_candidates.end(); ++it) {
          IceCandidate candidate = *it;
          IceCandidate* can = response->add_ice_candidate();
          can->CopyFrom(*it);
      }
      break;
    }
    case CONNECT_STATUS: {
      response->set_type(MessageResponseType::CONNECT_STATUS_ANSWER);
      // get network status
      orbit::SessionInfoManager* session_info =
            Singleton<orbit::SessionInfoManager>::GetInstance();
      orbit::SessionInfoPtr session = session_info->GetSessionInfoById(session_id);
      if(session != NULL) {
        std::vector<int> vec_streams_id;
        vec_streams_id = session->GetLiveStreamsId();
        for (std::vector<int>::iterator it=vec_streams_id.begin(); 
             it!=vec_streams_id.end(); it++) {
          olive::ConnectStatus* status = response->add_connect_status();
          int stream_id = *it;
          pipeline->ProcessMessage(stream_id, GET_NET_STATUS, status);
          status->set_stream_id(stream_id);
        }
      }
      break;
    }
    default: {
      LOG(ERROR) << "Process Message: Type Unknown..." << request->type();
      response->set_status(RecvSessionMessageResponse::ERROR);
      response->set_error_message("unknown request type.");
      rpc_stat.Fail();
      break;
    }
    };
    return grpc::Status::OK;
  } else {
    response->set_status(RecvSessionMessageResponse::ERROR);
    response->set_error_message("session not found.");
    rpc_stat.Fail();
    return grpc::Status::OK;
  }
}

} // namespace olive
