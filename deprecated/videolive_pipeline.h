// Copyright 2015 All Rights Reserved.
// Author: eric@orangelab.cn (Cheng Xu)

#ifndef VIDEOLIVE_PIPELINE_H__
#define VIDEOLIVE_PIPELINE_H__
#include "media_pipeline.h"
#include <vector>
#include <string>

namespace olive {
  using std::vector;
  using std::string;

  namespace videolive {
    class Internal;
  }

  class VideoLivePipeline : public olive::MediaPipeline {
  public:
    VideoLivePipeline();
    ~VideoLivePipeline();

    bool CloseAll();// TODO close pipeline 

    int CreateStream();
    bool CloseStream(int stream_id);

    bool AddIceCandidate(int stream_id, const IceCandidate& ice_candidate);
    bool ProcessSdpOffer(int stream_id, const string& sdp_offer, string* sdp_answer);
    void ReceiveIceCandidates(int stream_id, vector<IceCandidate>* ice_candidates);
    bool ProcessMessage(int stream_id, int message_type, void *data);

  private:
    videolive::Internal* internal_;
    //DISALLOW_COPY_AND_ASSIGN(VideoLivePipeline);
  };

}  // namespace olive

#endif  // VIDEOLIVE_PIPELINE_H__
