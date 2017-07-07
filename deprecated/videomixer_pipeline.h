// Copyright 2015 All Rights Reserved.
// Author: eric@orangelab.cn (Cheng Xu)

#ifndef VIDEOMIXER_PIPELINE_H__
#define VIDEOMIXER_PIPELINE_H__
#include "media_pipeline.h"
#include <vector>
#include <string>

namespace olive {
using std::vector;
using std::string;

namespace videomixer {
class Internal;
}

class VideoMixerPipeline: public olive::MediaPipeline {
public:
  VideoMixerPipeline();
  ~VideoMixerPipeline();

  int CreateStream();
  bool CloseStream(int stream_id);

  bool AddIceCandidate(int stream_id, const IceCandidate& ice_candidate);
  bool ProcessSdpOffer(int stream_id, const string& sdp_offer, string* sdp_answer);
  void ReceiveIceCandidates(int stream_id, vector<IceCandidate>* ice_candidates);

private:
  videomixer::Internal* internal_;
  //DISALLOW_COPY_AND_ASSIGN(VideoCallPipeline);
};

}  // namespace olive

#endif  // VIDEOMIXER_PIPELINE_H__
