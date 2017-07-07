/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * erizo_stream_service_impl.cc
 * ---------------------------------------------------------------------------
 * Implements the stream service interface based on erizo lib.
 * ---------------------------------------------------------------------------
 */

#include "erizo_stream_service_impl.h"
#include "erizo_media_processor.h"

#include "stream_service/media_pipeline.h"

#include <google/protobuf/text_format.h>
#include "glog/logging.h"
#include "gflags/gflags.h"

#include <map>
#include <queue>
#include <string>
#include <vector>

namespace olive {
  using std::map;
  using std::queue;
  using std::string;
  using std::vector;

  using namespace google::protobuf;
  namespace {  // annoymous namespace
    struct SessionData {
      // TODO(chengxu): add a lock to use this class
      int32 session_id;
      MediaPipeline * pipeline;
    };
    // Map <session_id, SessionData>
    map<int32, SessionData*> session_data;


    SessionData* GetSessionData(int32 session_id) {
      auto it = session_data.find(session_id);
      if (it == session_data.end()) {
        return NULL;
      }
      SessionData* session_data_item = (*it).second;
      return session_data_item;
    }

    MediaPipeline* GetPipeline(int32 session_id) {
      SessionData* session = GetSessionData(session_id);
      if (session == NULL)
        return NULL;
      return session->pipeline;
    }
  }  // annoymous namespace

// virtual
grpc::Status ErizoStreamServiceImpl::CreateSession(grpc::ServerContext* context,
                                              const CreateSessionRequest* request,
                                              CreateSessionResponse* response) {
  std::string str;
  google::protobuf::TextFormat::PrintToString(*request, &str);

  int session_id;
  session_id = 123;

  MediaPipeline* pipeline = GetPipeline(session_id);
  if (pipeline == NULL) {
    //pipeline = new VideoMixerPipeline;
    switch (request->type()) {
    case CreateSessionRequest::VIDEO_BRIDGE:
      LOG(INFO) << "request_type=VIDEO_BRIDGE";
      pipeline = new ErizoVideoCallPipeline;
      break;
    case CreateSessionRequest::AUDIO_MIXER_BRIDGE:
      LOG(INFO) << "request_type=AUDIO_MIXER_BRIDGE";
      LOG(FATAL) << "Not implemented yet.";
      break;
    case CreateSessionRequest::VIDEO_MIXER_BRIDGE:
      LOG(INFO) << "request_type=VIDEO_MIXER_BRIDGE";
      LOG(FATAL) << "Not implemented yet.";
      //pipeline = new VideoMixerPipeline;
      break;
    case CreateSessionRequest::VIDEO_LIVE_BRIDGE:
      LOG(INFO) << "request_type=VIDEO_LIVE_BRIDGE";
      LOG(FATAL) << "Not implemented yet.";
      /*
      pipeline = new VideoLivePipeline;
      string rtmp_location = request->rtmp_location();
      pipeline->ProcessMessage(1, &rtmp_location);
      */
      break;
    default:
      LOG(INFO) << "request_type=" << request->type();
      LOG(FATAL) << "Not implemented yet.";
      break;
    }
  }
  SessionData* new_session_data_item = new SessionData;
  new_session_data_item->session_id = session_id;
  new_session_data_item->pipeline = pipeline;
  session_data.insert(std::pair<int32, SessionData*>(session_id, new_session_data_item));
  response->set_status(CreateSessionResponse::OK);
  response->set_session_id(session_id);
  response->set_message("ok");
  return grpc::Status::OK;
}

//  virtual
grpc::Status ErizoStreamServiceImpl::CloseSession(grpc::ServerContext* context,
                                             const CloseSessionRequest* request,
                                             CloseSessionResponse* response) {
  // TODO(chengxu): Add the session management code here.
  LOG(INFO)<<"<<<<<<<< ErizoStreamServiceImpl CloseSession";
  int session_id = request->session_id(); 
  LOG(INFO)<<"session_id = "<<session_id;
  SessionData *session_data = GetSessionData(session_id); 
  if (session_data != NULL) {
    LOG(INFO)<<"session_data != NULL";
    MediaPipeline *pipeline = session_data->pipeline;
    if (pipeline != NULL) {
      LOG(INFO)<<"pipeline != NULL";
      pipeline->CloseAll();
      //session_data.erase(session_id);
    }
  }
  response->set_session_id(session_id);
  return grpc::Status::OK;
}


// virtual
grpc::Status ErizoStreamServiceImpl::CreateStream(grpc::ServerContext* context,
                                             const CreateStreamRequest* request,
                                             CreateStreamResponse* response) {

  std::string str;
  google::protobuf::TextFormat::PrintToString(*request, &str);
  LOG(INFO) << "request=" << str;

  int session_id = request->session_id();
  response->set_session_id(session_id);

  MediaPipeline* pipeline = GetPipeline(session_id);
  if (pipeline == NULL) {
    response->set_status(CreateStreamResponse::ERROR);
    response->set_message("pipeline not found.");
    return grpc::Status::CANCELLED;
  }

  LOG(INFO) << "pipeline_=" << pipeline;

  int stream_id = pipeline->CreateStream();
  response->set_status(CreateStreamResponse::OK);
  response->set_stream_id(stream_id);
  response->set_message("ok");

  //CreateMediaPipeline(response->session_id(),
  //                    response->stream_id());
  return grpc::Status::OK;
}

//  virtual
grpc::Status ErizoStreamServiceImpl::CloseStream(grpc::ServerContext* context,
                                            const CloseStreamRequest* request,
                                            CloseStreamResponse* response) {
  int session_id = request->session_id(); 
  int stream_id  = request->stream_id();
  SessionData *session_data = GetSessionData(session_id);
  if (NULL != session_data) {
    MediaPipeline *pipeline = session_data->pipeline; 
    pipeline->CloseStream(stream_id);
  }
  return grpc::Status::OK;
}

  // virtual
grpc::Status ErizoStreamServiceImpl::SendSessionMessage(grpc::ServerContext* context,
                                                   const SendSessionMessageRequest* request,
                                                   SendSessionMessageResponse* response) {
  // Fill in the common fields in the response.
  response->set_session_id(request->session_id());
  response->set_stream_id(request->stream_id());

  // Look up from the request->session_id and stream_id, to find the webrtc element.
  int session_id = request->session_id();
  int stream_id = request->stream_id();

  MediaPipeline* pipeline = GetPipeline(session_id);


  if (request->type() == ICE_CANDIDATE) {
    IceCandidate candidate = request->candidate();
    if (!pipeline->AddIceCandidate(stream_id, candidate)) {
      return grpc::Status::CANCELLED;
    }
    response->set_type(MessageResponseType::ICE_CANDIDATE_ANSWER);
  } else if (request->type() == SDP_OFFER) {
    string sdp_offer = request->sdp_offer();
    string sdp_answer;
    if (!pipeline->ProcessSdpOffer(stream_id, sdp_offer, &sdp_answer)) {
      return grpc::Status::CANCELLED;
    }
    response->set_type(MessageResponseType::SDP_ANSWER);
    response->set_sdp_answer(sdp_answer);
  }

  return grpc::Status::OK;
}

  //virtual
grpc::Status ErizoStreamServiceImpl::RecvSessionMessage(grpc::ServerContext* context,
                                                   const RecvSessionMessageRequest* request,
                                                   RecvSessionMessageResponse* response) {
  // Look up from the request->session_id and stream_id, to find the webrtc element.
  int session_id = request->session_id();
  int stream_id = request->stream_id();

  // Fill in the common fields in the response.
  response->set_session_id(session_id);
  response->set_stream_id(stream_id);

  MediaPipeline* pipeline = GetPipeline(session_id);

  if (request->type() == ICE_CANDIDATE) {
      
    response->set_type(MessageResponseType::ICE_CANDIDATE_ANSWER);
    vector<IceCandidate> ice_candidates;
    pipeline->ReceiveIceCandidates(stream_id, &ice_candidates);
    for (auto it = ice_candidates.begin(); it != ice_candidates.end(); ++it) {
        IceCandidate candidate = *it;
        //LOG(INFO) << "!!!!!!!send to node:" << candidate.ice_candidate();
        IceCandidate* can = response->add_ice_candidate();
        can->CopyFrom(*it);
    }
  }
  return grpc::Status::OK;
}

}
