// Copyright 2015 All Rights Reserved.
// Author: qingyong@orangelab.cn(QingyongZhang)

#ifndef MEDIA_PIPELINE_H__
#define MEDIA_PIPELINE_H__

#include "stream_service/stream_service.grpc.pb.h"
#include <vector>
#include <string>

namespace olive {
using std::vector;
using std::string;

class MediaPipeline {
public:
  //MediaPipeline();
  //virtual ~MediaPipeline();
  virtual bool CloseAll() { return false; };
  virtual int CreateStream() { return -1; };
  virtual bool CloseStream(int stream_id) { return false; };
  virtual bool AddIceCandidate(int stream_id, const IceCandidate& ice_candidate) { return false; };
  virtual bool ProcessSdpOffer(int stream_id, const string& sdp_offer, string* sdp_answer) { return false; };
  virtual void ReceiveIceCandidates(int stream_id, vector<IceCandidate>* ice_candidates) { return ; };
  virtual bool ProcessMessage(int stream_id, int message_type, void *data) {return false;};
  virtual void SetSessionId(int session_id) {return ;};
};
}
  // namespace olive

#endif  // MEDIA_PIPELINE_H__
