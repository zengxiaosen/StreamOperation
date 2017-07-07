/*
 * Copyright 2016 (C) Orange Labs Inc. All Rights Reserved.
 * Author: qingyong@orangelab.cn (Qingyong Zhang)
 *         cheng@orangelab.cn (Cheng Xu)
 *
 * video_forward_element.cc
 * ---------------------------------------------------------------------------
 * Defines and implements a SFU algorithm for video forwarding unit in Orbit
 * ---------------------------------------------------------------------------
 */
#include "video_forward_element.h"

#include "stream_service/orbit/rtp/janus_rtcp_processor.h"
#include "stream_service/orbit/rtp/rtp_format_util.h"
#include "stream_service/orbit/base/timeutil.h"

#include "glog/logging.h"
#include "gflags/gflags.h"

#include <sys/prctl.h>

#define VIDEO_FORWARD_PREBUFFERING_SIZE 0
#define VIDEO_FORWARD_THREAD_SLEEP_MS 1

using namespace std;
namespace orbit {
// Constructor
VideoForwardElement::VideoForwardElement(IVideoForwardEventListener* video_forward_event_listener) {
  video_forward_event_listener_ = video_forward_event_listener;
  running_ = true;
  selective_forward_thread_.reset(new boost::thread([this] {
    this->SelectiveVideoForwardLoop();
  }));
}

// Destructor
VideoForwardElement::~VideoForwardElement() {
  running_ = false;
  LOG(INFO) << "Joining selective_forward thread";
  if (selective_forward_thread_) {
    selective_forward_thread_->join();
  }
  LOG(INFO) << "Thread terminated on destructor";
}

// Helpfer function to get the video buffer given by the viewer_id
std::shared_ptr<VideoAwareFrameBuffer> VideoForwardElement::GetVideoQueue(int viewer_id) {
  if (viewer_id == -1) {
    return NULL;
  }
  map<int ,std::shared_ptr<VideoAwareFrameBuffer>>::iterator it = video_queues_.find(viewer_id);
  if(it == video_queues_.end()) {
    LOG(ERROR) << "Find viewer_id=" << viewer_id << " failed....";
    return NULL;
  }
  
  std::shared_ptr<VideoAwareFrameBuffer> inbuf = video_queues_[viewer_id];
  return inbuf;
}

// Do a pop() function and get the rtp_packet as return from a given buffer.
// Returns NULL if no buffer or buffer is still pre-buffering phase.
std::shared_ptr<orbit::dataPacket> VideoForwardElement::PopFromBuffer(
    int viewer_id, bool pop_from_queue) {
  if (viewer_id == -1) {
    return NULL;
  }
  std::shared_ptr<orbit::dataPacket> rtp_packet;
  std::shared_ptr<VideoAwareFrameBuffer> viewer_buffer = GetVideoQueue(viewer_id);
  if (viewer_buffer == NULL) {
    LOG(ERROR) << "Viewer buffer is NULL? Why. viewer_id=" << viewer_id;
    return NULL;
  }
  if (viewer_buffer->PrebufferingDone()) {
    if (viewer_buffer->GetBufferType() != VideoAwareFrameBuffer::PLAY_BUFFER) {
      LOG(ERROR) << "Error... the current_viewer_buffer is not PLAY_BUFFER :" 
                 << (int)(viewer_buffer->GetBufferType());
      return NULL;
    }
    if (pop_from_queue) {
      rtp_packet = viewer_buffer->PopPacket();
    } else {
      rtp_packet = viewer_buffer->TopPacket();
    }
  } else {
    return NULL;
  }
  return rtp_packet;
}

// Callback function: for all the incoming packets, store them into the corresponding buffers.
void VideoForwardElement::OnIncomingPacket(const int stream_id, const orbit::dataPacket& packet) {
  std::unique_lock<std::mutex> lock(video_queue_mutex_);
  int size = -1;
  map<int ,std::shared_ptr<VideoAwareFrameBuffer>>::iterator it = video_queues_.find(stream_id);
  if(it != video_queues_.end()) {
    std::shared_ptr<VideoAwareFrameBuffer> queue = it->second;
    queue->PushPacket((const char *)&(packet.data[0]), packet.length);
  }
}

// Send a FIR RTCP packet to the given source stream (stream_id).
void VideoForwardElement::SendFirPacket(int stream_id) {
  // Send another FIR/PLI packet to the sender.
  /* Send a FIR to the new RTP forward publisher */
  char buf[20];
  memset(buf, 0, 20);
  int fir_tmp = 0;
  map<int ,int>::iterator it = key_frame_seq_.find(stream_id);
  if(it != key_frame_seq_.end()) {
    fir_tmp = it->second;
  }

  int len = janus_rtcp_fir((char *)&buf, 20, &fir_tmp);
  key_frame_seq_[stream_id] = fir_tmp;

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
  VLOG(4) << "Send RTCP...PLI...." << stream_id;
}

/**
 * Link stream to current element.
 */
void VideoForwardElement::LinkVideoStream(int stream_id) {
  std::unique_lock<std::mutex> lock(video_queue_mutex_);
  int n = 0;
  for (auto q : video_queues_) {
    if (q.second->GetBufferType() == VideoAwareFrameBuffer::PLAY_BUFFER ||
        q.second->GetBufferType() == VideoAwareFrameBuffer::STORAGE_BUFFER) {
      n++;
    }
  }
  if(current_viewer_ == -1) {
    current_viewer_ = stream_id;
  }
  map<int ,std::shared_ptr<VideoAwareFrameBuffer>>::iterator it =
      video_queues_.find(stream_id);
  if(it != video_queues_.end()) {
    LOG(ERROR)<<"Stream is already inserted into video forward element : "<<stream_id;
  } else {
    std::shared_ptr<VideoAwareFrameBuffer> queue =
        std::make_shared<VideoAwareFrameBuffer>(VIDEO_FORWARD_PREBUFFERING_SIZE);
    if (n < 2) {
      queue->SetBufferType(VideoAwareFrameBuffer::PLAY_BUFFER);
    } else {
      queue->SetBufferType(VideoAwareFrameBuffer::NORMAL_BUFFER);
    }
    video_queues_[stream_id] = queue;
    // Insert the video_switch_context
    std::shared_ptr<VideoSwitchContext> context = std::make_shared<VideoSwitchContext>();
    context->stream_id = -1;
    context->new_stream_id = current_viewer_;
    context->changing_ = true;
    changing_ = true;
    video_switch_contexts_[stream_id] = context;
    
    // If the current_viewer is the only one in the room, we will take the place as second_viewer_
    std::shared_ptr<VideoSwitchContext> current_viewer_context = video_switch_contexts_[current_viewer_];
    if (current_viewer_context &&
        current_viewer_context->stream_id == current_viewer_ &&
        current_viewer_context->new_stream_id == -1) {
      current_viewer_context->new_stream_id = stream_id;
      current_viewer_context->changing_ = true;
      request_key_frame_stream_ids_.push_back(stream_id);
    }
    request_key_frame_stream_ids_.push_back(current_viewer_);
  }
}

/**
 * Unlink stream to current element.
 */
void VideoForwardElement::UnlinkVideoStream(int stream_id) {
  std::unique_lock<std::mutex> lock(video_queue_mutex_);

  video_queues_.erase(stream_id);
  video_switch_contexts_.erase(stream_id);

  int first_stream_id = -1;
  auto begin = video_queues_.begin();
  if (begin != video_queues_.end()) {
    first_stream_id = begin->first;
  }

  if(changing_ && new_viewer_ == stream_id) {
    new_viewer_ = -1;
    changing_ = false;
  }

  int size = video_queues_.size();
  if (size == 0) {
    current_viewer_ = -1;
  } else if (size == 1) {
    if (first_stream_id != -1) {
      LOG(INFO) << "this_stream_id=" << stream_id;
      LOG(INFO) << "first_stream_id=" << first_stream_id;
      auto switch_contexts_iter = video_switch_contexts_.find(first_stream_id);
      if (switch_contexts_iter != video_switch_contexts_.end()) {
        std::shared_ptr<VideoSwitchContext> switch_context = switch_contexts_iter->second;
        LOG(INFO) << "-----context---";
        LOG(INFO) << switch_context->DebugString();

        SwitchPublisher(first_stream_id);
        current_viewer_ = first_stream_id;
        changing_       = true;
        new_viewer_     = first_stream_id;
        request_key_frame_stream_ids_.push_back(first_stream_id);

        assert(switch_context);
        switch_context->stream_id     = first_stream_id;
        switch_context->new_stream_id = first_stream_id;
        switch_context->changing_     = true;
      }
    }
  } else if (first_stream_id != -1) {
    SwitchPublisher(first_stream_id);
    current_viewer_ = first_stream_id;
    changing_ = true;
    // Find all the reference id to this stream_id 
    for (auto context_it : video_switch_contexts_) {
      std::shared_ptr<VideoSwitchContext> switch_context = context_it.second;
      if (switch_context->stream_id == stream_id) {
        switch_context->stream_id = -1;
        switch_context->new_stream_id = first_stream_id;
        switch_context->changing_ = true;
      }
    }
  } else {
    // do nothing
  }
}

void VideoForwardElement::RequestSpeakerKeyFrame() {
  std::unique_lock<std::mutex> lock(video_queue_mutex_);
  request_key_frame_stream_ids_.push_back(current_viewer_);
}

void VideoForwardElement::ChangeContext(std::shared_ptr<VideoSwitchContext> context, int source_id) {
  if (context) {
    int base_stream_id = context->stream_id;
    if (base_stream_id != source_id) {
      context->changing_ = true;
      context->new_stream_id = source_id;
    }
  }
}

void VideoForwardElement::SwitchPublisher(int publisher_id) {
  VLOG(3) << "Change the Viewer....to publisher_id=" << publisher_id << " current_viewer_=" << current_viewer_;
  if (publisher_id != current_viewer_ && publisher_id != new_viewer_) {
    // First step: request the key frame for ths new_viewer
    new_viewer_ = publisher_id;
    request_key_frame_stream_ids_.push_back(publisher_id);

    // Second step: Change the new_viewer's Buffer to STORAGE_BUFFER if needed.
    // Note that if the new_buffer is PLAY_BUFFER or something, no need to do that.
    std::shared_ptr<VideoAwareFrameBuffer> new_viewer_buffer = GetVideoQueue(new_viewer_);
    if (new_viewer_buffer) {
      switch (new_viewer_buffer->GetBufferType()) {
       case VideoAwareFrameBuffer::NORMAL_BUFFER: {
         new_viewer_buffer->SetBufferType(VideoAwareFrameBuffer::STORAGE_BUFFER);
         new_viewer_buffer->ResetBuffer();
         break;
       }
       case VideoAwareFrameBuffer::STORAGE_BUFFER: {
         LOG(INFO) << "Yikes, this is a STORAGE_BUFFER";
         break;
       }
       case VideoAwareFrameBuffer::PLAY_BUFFER: {
         LOG(INFO) << "Yikes, this is a PLAY_BUFFER";
         break;
       }
      }
    }

    for (auto context_it : video_switch_contexts_) {
      int this_stream_id = context_it.first;
      std::shared_ptr<VideoSwitchContext> switch_context = context_it.second;
      if (this_stream_id == new_viewer_) {
        // the publisher will still forward from current_viewer_
        // Do nothing.
        continue;
      } else {
        int source_id = new_viewer_;
        ChangeContext(switch_context, source_id);
      }
    }

    changing_ = true;
  }
}

void VideoForwardElement::ChangeToSpeaker(int stream_id) {
  if(stream_id < 0) {
    return;
  }
  std::unique_lock<std::mutex> lock(video_queue_mutex_);
  VLOG(3) << "Intended to change speaker to " << stream_id;
  if (stream_id != current_viewer_ && getTimeMS() - last_switch_time_ > 2000) {
    LOG(INFO)<<"===================Speaker changed "<< stream_id;
    if(video_queues_.find(stream_id) != video_queues_.end()) {
      // NOTE(chengxu): the last_switch_time_ should be put into the thread.
      // At this point, the switch has not happened.
      last_switch_time_ = getTimeMS();
      SwitchPublisher(stream_id);
    }
  }
}

// Main worker thread for select the streams and forward.
void VideoForwardElement::SelectiveVideoForwardLoop() {
  LOG(INFO) << "SelectiveVideoForward";

  /* Set thread name */
  prctl(PR_SET_NAME, (unsigned long)"SelectiveVideoForward");

  /* Timer */
  long start_time = GetCurrentTime_MS();

  while(running_) {
    // Sleep every time for SLEEP_MS (constant value)
    long end_time = GetCurrentTime_MS();
    if (end_time - start_time < VIDEO_FORWARD_THREAD_SLEEP_MS) {
      usleep(1000);
      continue;
    }
    start_time = end_time;    

    std::unique_lock<std::mutex> lock(video_queue_mutex_);

    // If the count is 0, indicates that the video stream has not been added
    // to the pipeline.
    int count = video_queues_.size();
    if(count == 0) {
      usleep(1000);
      continue;
    } 

    // If the following code explicitly ask for requesting key frame,
    // send it right away.
    while (!request_key_frame_stream_ids_.empty()) {
      int need_key_frame_stream_id = request_key_frame_stream_ids_.front();
      request_key_frame_stream_ids_.pop_front();
      SendFirPacket(need_key_frame_stream_id);
    }

    // Get all the streams' id into a vector.
    vector<int> stream_ids;
    for (auto context_it : video_switch_contexts_) {
      stream_ids.push_back(context_it.first);
    }

    /**
     * ---------------------------------------------------------------------------
     * CHANGING_PHASE:
     *   - in changing phase, there must be one or more streams switch involved in
     *     this change phase.
     *   - thus, we should check all the streams, and see if those streams have
     *     KeyFrame arrvied. If arrived, the STORAGE_BUFFER could be changed to
     *     PLAY_BUFFER, and then it will finish the changing
     *   - usually update the context_switch structure with the newly update
     *     stream_id, new_stream_id, base_ts, base_seq and etc.
     * ---------------------------------------------------------------------------
     */
    if (changing_) {
      bool all_change_done = true;
      for (int this_stream_id : stream_ids) {
        auto switch_contexts_iter = video_switch_contexts_.find(this_stream_id);
        if (switch_contexts_iter == video_switch_contexts_.end()) {
          continue;
        }
        std::shared_ptr<VideoSwitchContext> switch_context = switch_contexts_iter->second;
        std::shared_ptr<VideoAwareFrameBuffer> this_buffer = GetVideoQueue(this_stream_id);
        if (switch_context->changing_) {
          // assert(switch_context->new_stream_id != -1)
          assert(switch_context->new_stream_id != -1);
          std::shared_ptr<VideoAwareFrameBuffer> new_stream_buffer = GetVideoQueue(switch_context->new_stream_id);
          std::shared_ptr<dataPacket> rtp_packet;
          if (new_stream_buffer != NULL) {
            rtp_packet = new_stream_buffer->TopPacket();
          }
          if (new_stream_buffer != NULL && rtp_packet != NULL &&
              new_stream_buffer->IsKeyFramePacket(rtp_packet)) {
            // The new_stream_buffer finally has the KeyFrame, it will be PLAY_BUFFER
            if (new_stream_buffer->GetBufferType() != VideoAwareFrameBuffer::PLAY_BUFFER) {
              new_stream_buffer->SetBufferType(VideoAwareFrameBuffer::PLAY_BUFFER);
            }
            switch_context->changing_ = false;
            switch_context->stream_id = switch_context->new_stream_id;
            switch_context->new_stream_id = -1;
            const RtpHeader* h = reinterpret_cast<const RtpHeader*>(rtp_packet->data);
            switch_context->base_ts = h->getTimestamp();
            switch_context->base_seq = h->getSeqNumber();
            switch_context->last_ts = switch_context->ts + 2880;
            switch_context->last_seq = switch_context->seq + 1;
            if (this_stream_id != new_viewer_ && 
                this_stream_id != current_viewer_ &&
                this_buffer != NULL) {
              this_buffer->CleanBuffer();
              this_buffer->SetBufferType(VideoAwareFrameBuffer::NORMAL_BUFFER);
            }
          }
        }
        if (switch_context->changing_) {
          all_change_done = false;
        }
      }
      if (all_change_done) {
        changing_ = false;
        if (new_viewer_ != -1) {
          second_viewer_ = current_viewer_;
          current_viewer_ = new_viewer_;
          new_viewer_ = -1;
        }
      }
    }


    /**
     * ---------------------------------------------------------------------------
     * FORWARD_PHASE:
     *   - in forward phase, iterate on all the streams that are still valid in
     *   - the forward_element. For each streams, we will get the rtp packets
     *   - from its source (or called base) stream. The rtp packet will be
     *   - rewritten into a mixed_pkt (the ts, seq will be updated accordingy).
     *   - After rewrite the packet, the newly created mixed_pkt will be forwarded
     *   - to the right receiver.
     * ---------------------------------------------------------------------------
     */

    // Use a <int, packet> map to store the stream_id with the corresponding
    // poped rtp_packets.
    map<int, shared_ptr<orbit::dataPacket>> play_packets;
    int i = 0;
    for (int this_stream_id : stream_ids) {
      auto switch_contexts_iter = video_switch_contexts_.find(this_stream_id);
      if (switch_contexts_iter == video_switch_contexts_.end()) {
        continue;
      }
      std::shared_ptr<VideoSwitchContext> switch_context = switch_contexts_iter->second;

      int source_stream_id = switch_context->stream_id;
      if (source_stream_id == -1) {
        continue;
      }

      std::shared_ptr<orbit::dataPacket> rtp_packet;
      if (play_packets.find(source_stream_id) != play_packets.end()) {
        rtp_packet = play_packets[source_stream_id];
      } else {
        rtp_packet = PopFromBuffer(source_stream_id, true);
        play_packets[source_stream_id] = rtp_packet;
      }

      if (rtp_packet) {
        const unsigned char* data_buf = reinterpret_cast<const unsigned char*>(&(rtp_packet->data[0]));
        // Construct the media packet.
        // Output the mixed_pkt as the forwarding packet to all the participants.
        const RtpHeader* h = reinterpret_cast<const RtpHeader*>(rtp_packet->data);
        assert (switch_context->base_seq != -1);
        uint16_t next_seq = switch_context->last_seq + (h->getSeqNumber() - switch_context->base_seq);
        if ((next_seq > switch_context->seq) && abs(next_seq - switch_context->seq) > 500) {
          LOG(ERROR) << "Next_seq may have problem. next_seq=" << next_seq << " current_seq=" << switch_context->seq;
          //continue;
        }
        switch_context->seq = next_seq;
        assert (switch_context->base_ts != -1);
        switch_context->ts = switch_context->last_ts + (h->getTimestamp() - switch_context->base_ts);
        
        std::shared_ptr<MediaOutputPacket> mixed_pkt = std::make_shared<MediaOutputPacket>();
        mixed_pkt->timestamp = switch_context->ts;
        mixed_pkt->seq_number = switch_context->seq;
        mixed_pkt->end_frame = h->getMarker();
        mixed_pkt->ssrc = -1;
        unsigned char* tmp_buf = (unsigned char*)malloc(rtp_packet->length-12);
        memcpy(tmp_buf, data_buf+12, rtp_packet->length-12);
        mixed_pkt->encoded_buf = tmp_buf;
        mixed_pkt->length = rtp_packet->length-12;
        
        video_forward_event_listener_->OnRelayRtpPacket(this_stream_id, mixed_pkt);
        /**
         * Here we relay packet one more times for event listener used for live stream and so on.
         * stream_id is 0;
         */
        if (i == 0) {
          video_forward_event_listener_->OnRelayRtpPacket(0, mixed_pkt);
        }
      }
      ++i;
    }

    // Debug the warning if the processing time takes too long.
    int elapsed_time_ms = GetCurrentTime_MS() - end_time;
    if (elapsed_time_ms > 5) {
      LOG(WARNING) << " Warning!!!! video forward processing elapsed time="
                   << elapsed_time_ms << " ms"
                   << "....It may have caused problem yet.";
    }
  }  // End of while.  
}

} /* namespace orbit */
