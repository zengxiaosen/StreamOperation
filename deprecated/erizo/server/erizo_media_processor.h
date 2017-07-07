/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * erizo_media_processor.h
 * ---------------------------------------------------------------------------
 * Defines the media processor using erizo lib.
 * ---------------------------------------------------------------------------
 */

#ifndef ERIZO_MEDIA_PROCESSOR_H__
#define ERIZO_MEDIA_PROCESSOR_H__

#include "stream_service/media_pipeline.h"

namespace olive {
  using std::vector;
  using std::string;

  namespace erizo_videocall {
    class Internal;
  }

class ErizoVideoCallPipeline : public olive::MediaPipeline {
public:
  ErizoVideoCallPipeline();
  ~ErizoVideoCallPipeline();

  int CreateStream();
  bool CloseStream(int stream_id);

  bool AddIceCandidate(int stream_id, const IceCandidate& ice_candidate);
  bool ProcessSdpOffer(int stream_id, const string& sdp_offer, string* sdp_answer);
  void ReceiveIceCandidates(int stream_id, vector<IceCandidate>* ice_candidates);

private:
  erizo_videocall::Internal* internal_;
};

}  // namespace olive

#endif  // ERIZO_MEDIA_PROCESSOR_H__
