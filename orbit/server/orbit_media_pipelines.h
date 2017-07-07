/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * orbit_media_pipelines.h
 * ---------------------------------------------------------------------------
 * Defines a set of media processor based on MediaPipeline base class.
 * The media processors are essentially the pipelines to manage the streams and
 * communicate with the clients.
 * ---------------------------------------------------------------------------
 */
#pragma once

#define DEFAULT_STREAM_ID -1
#define SET_RTMP_LOCATION 1000
#define MUTE_STREAM 1001
#define RAISE_HAND 1002
#define GET_NET_STATUS 1003
// set the publisher, using for video dispatcher 
#define SET_DISPATCHER_PUBLISHER 2004

namespace olive {
using std::vector;
using std::string;

namespace orbit_videocall {
  class Internal;
}

struct MediaPipelineOption {
  bool capture_data = false;
};

class MediaPipeline {
public:
  MediaPipeline(int32_t session_id);
  MediaPipeline(int32_t session_id, string puglin_name);
  ~MediaPipeline();

  void SetMediaPipelineOption(const MediaPipelineOption& option);

  int CreateStream(const CreateStreamOption& option) ;
  bool CloseStream(int stream_id) ;
  bool CloseAll() ;

  bool AddIceCandidate(int stream_id, const IceCandidate& ice_candidate) ;
  bool ProcessSdpOffer(int stream_id, const string& sdp_offer,
                       string* sdp_answer) ;
  void ReceiveIceCandidates(int stream_id,
                            vector<IceCandidate>* ice_candidates) ;
  bool ProcessMessage(int stream_id,
                      int message_type,
                      void *data) ;

  bool CloseInvalidStream(int stream_id);

private:
  orbit_videocall::Internal* internal_;
};

}  // namespace olive
