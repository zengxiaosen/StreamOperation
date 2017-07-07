/*
 * Copyright 2016 (C) Orange Labs Inc. All Rights Reserved.
 * Author: Xinyu Cai
 *
 * video_forward_element_new.cc
 * ---------------------------------------------------------------------------
 * Defines and implements a new video forwrading algorithm.
 * ---------------------------------------------------------------------------
 */
#include "video_forward_element.h"

#include "stream_service/orbit/rtp/janus_rtcp_processor.h"
#include "stream_service/orbit/rtp/rtp_format_util.h"
#include "stream_service/orbit/base/timeutil.h"

#include "glog/logging.h"
#include "gflags/gflags.h"

using namespace std;
namespace orbit {
void FirSender::SendFir(int32_t stream_id) {
  LOG(INFO) << "-----" << "FirSender::SendFir";
  std::lock_guard<std::mutex> lock(mutex_);
  char buf[20];
  memset(buf, 0, 20);
  int fir_tmp = 0;
  auto it = firseq_map_.find(stream_id);
  if(it != firseq_map_.end()) {
    fir_tmp = it->second;
  }

  int len = janus_rtcp_fir((char *)&buf, 20, &fir_tmp);
  firseq_map_[stream_id] = fir_tmp;

  dataPacket p;
  p.comp = 0;
  p.type = VIDEO_PACKET;
  p.length = len;
  memcpy(&(p.data[0]), buf, len);

  if(video_forward_event_listener_ != NULL){
    video_forward_event_listener_->OnRelayRtcpPacket(stream_id, p);
  }
  VLOG(4) << "Send RTCP...FIR....";

  /* Send a PLI too, just in case... */
  memset(buf, 0, 12);
  len = janus_rtcp_pli((char *)&buf, 12);

  p.length = len;
  memcpy(&(p.data[0]), buf, len);
  if(video_forward_event_listener_ != NULL){
    video_forward_event_listener_->OnRelayRtcpPacket(stream_id, p);
  }
}

VideoForwardElementNew::VideoForwardElementNew(IVideoForwardEventListener* video_forward_event_listener)
: forward_loop_([this]{ForwardLoop();}) {
  video_forward_event_listener_ = video_forward_event_listener;
  fir_sender_ = new FirSender(video_forward_event_listener);
  video_mapper_ = new VideoMap;
  video_mapper_->SetPrepareCallback([this](int stream_id) {
    LOG(INFO) << "prepare for stream : " << stream_id;
    if (fir_sender_) {
      fir_sender_->SendFir(stream_id);
    }
  });
  video_mapper_->AddEnabledListener([this](int stream) {
    LOG(INFO) << "stream enabled : " << stream;
    auto queue = queue_map_[stream];
    queue->Enable();
  });
  video_mapper_->AddDisabledListener([this](int stream) {
    LOG(INFO) << "stream disabled : " << stream;
    auto queue = queue_map_[stream];
    queue->Disable();
  });
  video_mapper_->AddHasLookerListener([this](int stream) {
    LOG(INFO) << "stream has looker : " << stream;
    auto queue = queue_map_[stream];
    queue->SetHasLooker(true);
  });
  video_mapper_->AddNoLookerListener([this](int stream) {
    LOG(INFO) << "stream has no looker : " << stream;
    auto queue = queue_map_[stream];
    queue->SetHasLooker(false);
  });
  video_mapper_->AddSrcChangedListener([this](int stream) {
    LOG(INFO) << "stream's video source changed, now "
        << stream << " see " << video_mapper_->GetSrc(stream);
    int src = video_mapper_->GetSrc(stream);
    auto queue = queue_map_[src];
    auto frame = queue->TopPacket();
    auto switch_context = video_switch_contexts_[stream];
    switch_context->base_ts  = frame->ts();
    switch_context->base_seq = frame->seq();
    switch_context->last_ts  = switch_context->ts + 2880;
    switch_context->last_seq = switch_context->seq + 1;
  });
}

VideoForwardElementNew::~VideoForwardElementNew() {
  try {
    running_ = false;
    forward_loop_.join();
  } catch (...) {
    // just catch any exception.
  }
  if (video_mapper_) {
    delete video_mapper_;
    video_mapper_ = nullptr;
  }
  if (fir_sender_) {
    delete fir_sender_;
    fir_sender_ = nullptr;
  }
}

void VideoForwardElementNew::OnIncomingPacket(const int stream_id, const dataPacket &packet) {
  Lock lock(mutex_);
  if (!Contains(stream_id)) {
    return;
  }
  auto queue = queue_map_[stream_id];
  queue->PushPacket((uint8_t *)(packet.data), packet.length);
}

void VideoForwardElementNew::LinkVideoStream(int stream_id) {
  Lock lock(mutex_);
  if (Contains(stream_id)) {
    return;
  }

  LOG(INFO) << "LinkVideoStream stream_id = " << stream_id;

  auto queue = std::make_shared<VideoFrameQueue>(stream_id, [this](VideoFrameQueue::id_t id) {
    LOG(INFO) << "after queue prepared : " << id;
    video_mapper_->SetPrepared(id);
  });
  queue_map_[stream_id] = queue;

  auto switch_context = std::make_shared<VideoSwitchContext>();
  video_switch_contexts_[stream_id] = switch_context;

  video_mapper_->Add(stream_id);
}

void VideoForwardElementNew::UnlinkVideoStream(int stream_id) {
  Lock lock(mutex_);

  LOG(INFO) << "UnlinkVideoStream stream_id = " << stream_id;

  video_mapper_->Remove(stream_id);
  queue_map_.erase(stream_id);
  video_switch_contexts_.erase(stream_id);
}

void VideoForwardElementNew::ChangeToSpeaker(int stream_id) {
  Lock lock(mutex_);

  VLOG(3) << "ChangeToSpeaker stream_id = " << stream_id;

  video_mapper_->SetMaster(stream_id);
}

void VideoForwardElementNew::RequestSpeakerKeyFrame() {
  Lock lock(mutex_);

  LOG(INFO) << "RequestSpeakerKeyFrame";

  int32_t master = video_mapper_->GetMaster();
  if (master != VideoMap::NOID) {
    fir_sender_->SendFir(master);
  }
}

bool VideoForwardElementNew::Contains(int stream_id) const {
  return queue_map_.find(stream_id) != queue_map_.end();
}

void VideoForwardElementNew::ForwardLoop() {
  long start_time = GetCurrentTime_MS();
  while (running_) {
    long end_time = GetCurrentTime_MS();
    if (end_time - start_time < VIDEO_FORWARD_THREAD_SLEEP_MS) {
      usleep(1000);
      continue;
    }
    start_time = end_time;
    Lock lock(mutex_);
    ForwardData();
  }
}

void VideoForwardElementNew::ForwardData() {
  std::vector<VideoMap::id_t> stream_ids;
  video_mapper_->GetAllId(&stream_ids);

  bool first = true;
  for (int32_t id : stream_ids) {
    int32_t src = video_mapper_->GetSrc(id);
    if (src == VideoMap::NOID) {
      continue;
    }
    auto queue = queue_map_[src];
    auto frame = queue->TopPacket();
    if (!frame) {
      continue;
    }
    auto sc = video_switch_contexts_[id];

    sc->seq = frame->seq() - sc->base_seq + sc->last_seq;
    sc->ts  = frame->ts()  - sc->base_ts  + sc->last_ts;

    auto packet = std::make_shared<MediaOutputPacket>();
    packet->timestamp  = sc->ts;
    packet->seq_number = sc->seq;
    packet->end_frame  = frame->marker();
    packet->ssrc       = -1;
    unsigned char *tmp_buf = (unsigned char*)malloc(frame->len() - 12);
    memcpy(tmp_buf, frame->data() + 12, frame->len() - 12);
    packet->encoded_buf = tmp_buf;
    packet->length     = frame->len() - 12;

    video_forward_event_listener_->OnRelayRtpPacket(id, packet);

    /**
     * Here we relay packet one more times for event listener used for live stream and so on.
     * stream_id is 0;
     */
    if (first) {
      video_forward_event_listener_->OnRelayRtpPacket(0, packet);
      first = false;
    }
  }
  for (auto &pair : queue_map_) {
    auto queue = pair.second;
    if (queue) {
      queue->PopPacket();
    }
  }
}

}  // namespace orbit
