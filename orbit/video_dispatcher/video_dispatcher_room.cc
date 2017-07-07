/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: qingyong@orangelab.cn (Qingyong Zhang)
 *
 * video_dispatch_room.cc
 * ---------------------------------------------------------------------------
 */

#include "video_dispatcher_room.h"
#include "video_dispatcher_plugin.h"
#include <sys/prctl.h>
#include <boost/thread/mutex.hpp>

#include "stream_service/orbit/audio_processing/audio_energy.h"
#include "stream_service/orbit/rtp/janus_rtcp_processor.h"
#include "stream_service/orbit/live_stream/common_defines.h"
#include "stream_service/orbit/base/timeutil.h"

#include "gflags/gflags.h"

#define VIDEO_PREBUFFERING_SIZE 0

namespace orbit {
using namespace std;
  VideoDispatcherRoom::VideoDispatcherRoom() {
    running_ = false;
    for (int i = 0; i < MAX_DISPATCHER_PARTICIPANT_COUNT; ++i) {
      fir_seq_[i] = 0;
    }
    // publisher
    current_viewer_ = -1;
    publisher_stream_id_ = -1;
  }
  void VideoDispatcherRoom::Create() {
  }
  void VideoDispatcherRoom::Destroy() {
      running_ = false;
      if (selective_forward_thread_.get() != NULL) {
       selective_forward_thread_->join();
       VLOG(3) << "Thread terminated on destructor in video dispatcher room";
      }
  }

  void VideoDispatcherRoom::Start() {
    running_ = true;
    selective_forward_thread_.reset(new boost::thread([this] {
        this->SelectiveVideoForward();
      }));
  }

  void VideoDispatcherRoom::AddParticipant(TransportPlugin* plugin) {
    VLOG(2) << "AddParticipant===========================================================";
    Room::AddParticipant(plugin);
  }

  bool VideoDispatcherRoom::RemoveParticipant(TransportPlugin* plugin) {
    {
      boost::mutex::scoped_lock lock(room_plugin_mutex_);
      VideoDispatcherPlugin *vd_plugin = (VideoDispatcherPlugin *)plugin;
      if (vd_plugin->stream_id() == publisher_stream_id_) {
        current_viewer_ = -1; // reset publiser  
      }
      Room::RemoveParticipant(plugin);
      return true;
    }
  }
 
  void VideoDispatcherRoom::SendFirPacketToViewer() {
    std::vector<TransportPlugin*> participants = GetParticipants();
    SwitchPublisher(current_viewer_);
  }

  void VideoDispatcherRoom::SetupLiveStream(bool support, bool need_return_video,
                                            const char* live_location) {
    VLOG(2)<<"=========================================";
    VLOG(2)<<"VideoDispatcherRoom :: StartLiveStream";
    use_webcast_ = support;
    if(support) {
      VLOG(google::INFO) << "Init webcast.";
      live_stream_processor_ = new LiveStreamProcessor(false, true);
      live_stream_processor_->Start(live_location);
    }
  }
  void VideoDispatcherRoom::SendFirPacket(int viewer, const std::vector<TransportPlugin*>& participants ) {

    if (viewer < 0 || viewer > (MAX_DISPATCHER_PARTICIPANT_COUNT - 1)) {
      LOG(ERROR) << "Invalid viewer index for SendFirPacket. index = " << viewer;
      return;
    }
    // Send another FIR/PLI packet to the sender.
    /* Send a FIR to the new RTP forward publisher */
    char buf[20];
    memset(buf, 0, 20);
    int fir_tmp = fir_seq_[viewer];
    int len = janus_rtcp_fir((char *)&buf, 20, &fir_tmp);
    fir_seq_[viewer] = fir_tmp;

    dataPacket p;
    p.comp = 0;
    p.type = VIDEO_PACKET;
    p.length = len;
    memcpy(&(p.data[0]), buf, len);

    TransportPlugin* tp = participants[viewer];
    VideoDispatcherPlugin* acp = reinterpret_cast<VideoDispatcherPlugin*>(tp);
    acp->RelayRtcpPacket(p);
    VLOG(2) << "Send RTCP...FIR....";

    /* Send a PLI too, just in case... */
    memset(buf, 0, 12);
    len = janus_rtcp_pli((char *)&buf, 12);

    p.length = len;
    memcpy(&(p.data[0]), buf, len);
    acp->RelayRtcpPacket(p);
    VLOG(2) << "Send RTCP...PLI....===========================================================";
  }

  void VideoDispatcherRoom::SelectiveVideoForward() {
    VLOG(2) << "SelectiveVideoForward";
    /* Set thread name */
    prctl(PR_SET_NAME, (unsigned long)"VDR::SelectiveVideoForward");

    /* RTP */
    int16_t seq = 0;
    int32_t ts = 0;

    int32_t last_ts = 0;
    int32_t base_ts = -1;
    int16_t last_seq = 0;
    int16_t base_seq = -1;

    long long framekey_timeout = getTimeMS();
    
    bool need_sleep = false;

    while(running_) {

      if (need_sleep) {
       usleep(1000);
       need_sleep = false;
      }

      // TODO Need to refactor the code for using lock 
      boost::mutex::scoped_lock lock(room_plugin_mutex_);

      std::vector<TransportPlugin*> participants = GetParticipants();
      int count = participants.size();
      if (count == 0) {
        VLOG(2) << "No participant...";
        need_sleep = true;
        continue;
      }

      // check publiser
      if (current_viewer_ == -1) {
        VLOG(2) << "publisher not ready...";
        need_sleep = true;
        continue;
      }
      
      // force to ask for fir packet each 10 second 
      long long current_time = getTimeMS();
      if (current_time - framekey_timeout > 10000) {
        framekey_timeout = current_time;
        SendFirPacket(current_viewer_, participants);
      }

      orbit::dataPacket pkt;
      {
        // get packet from publisher 
        TransportPlugin* p = participants[current_viewer_];
        VideoDispatcherPlugin* acp = reinterpret_cast<VideoDispatcherPlugin*>(p);
        RtpPacketQueue* inbuf = acp->GetVideoQueue();
        if (inbuf->getSize() > VIDEO_PREBUFFERING_SIZE) {
          std::shared_ptr<orbit::dataPacket> rtp_packet = inbuf->popPacket(true);
          pkt = *(rtp_packet.get());
        }

        // publish the packet to subscribers
        if (pkt.length != 0) {
          /* Update RTP header information */
          const unsigned char* data_buf = reinterpret_cast<const unsigned char*>(&(pkt.data[0]));
          const RtpHeader* h = reinterpret_cast<const RtpHeader*>(pkt.data);
          if (base_seq == -1) {
            base_seq = h->getSeqNumber();
          }
          seq = last_seq + (h->getSeqNumber() - base_seq);
          if (base_ts == -1) {
            base_ts = h->getTimestamp();
          }
          ts = last_ts + (h->getTimestamp() - base_ts);
  
          //ts = h->getTimestamp();
  
          VLOG(3) << " VideoPacket payload=" << (int)(h->getPayloadType())
                  << " ts=" <<  h->getTimestamp()
                  << " our_ts=" << ts
                  << " seq=" << h->getSeqNumber()
                  << " our_seq=" << seq
                  << " h->getMarker()=" << (int)(h->getMarker())
                  << " headerLength=" << h->getHeaderLength()
                  << " pkt.length=" << pkt.length;

          std::shared_ptr<MediaOutputPacket> mixed_pkt = std::make_shared<MediaOutputPacket>();
          mixed_pkt->timestamp = ts;
          mixed_pkt->seq_number = seq;
          mixed_pkt->end_frame = h->getMarker();
          mixed_pkt->ssrc = -1;

          unsigned char* tmp_buf = (unsigned char*)malloc(pkt.length-12);

          memcpy(tmp_buf, data_buf+12, pkt.length-12);
          mixed_pkt->encoded_buf = tmp_buf;
          mixed_pkt->length = pkt.length-12;
          for (int i = 0; i < count; ++i) {
            VideoDispatcherPlugin* p = (VideoDispatcherPlugin*)participants[i];
            if(i == current_viewer_){
                VLOG(3) << "Publisher id :"<<p->stream_id();
                continue;
            }
            VLOG(3) << "Send video to :"<<p->stream_id();
            VideoDispatcherPlugin* acp = reinterpret_cast<VideoDispatcherPlugin*>(p);
            acp->RelayMediaOutputPacket(mixed_pkt, VIDEO_PACKET);
          }
          if (use_webcast_  && live_stream_processor_->IsStarted()){
            live_stream_processor_->RelayRtpVideoPacket(mixed_pkt);
          }
        } else {
          usleep(1000);
          need_sleep = true;
          continue;
        }
      }
    } // while(running_) 
  }
  
  void VideoDispatcherRoom::SwitchPublisher(int publisher_id) {
    VLOG(3) << "Change the Viewer....";
    std::vector<TransportPlugin*> participants = GetParticipants();
    SendFirPacket(publisher_id, participants);
  }

  bool VideoDispatcherRoom::ProcessMessage(int stream_id, int message_type, void *data) {
    // TODO how to process different message
    if (message_type == 2) { // TODO hard code here for set publisher message
      bool success = false;
      {
        boost::mutex::scoped_lock lock(room_plugin_mutex_);          
        std::vector<TransportPlugin*> participants = GetParticipants();
        int count = participants.size();
        for (int i = 0; i < count; ++i) {
          VideoDispatcherPlugin* p = (VideoDispatcherPlugin*)participants[i];
          if (stream_id == p->stream_id()) {
            publisher_stream_id_ = stream_id;
            current_viewer_ = i;
            SendFirPacket(current_viewer_, participants);
            success = true;
          }
        }
      }
      if (!success) {
        LOG(ERROR) << "Fail to set publisher since stream_id not found .. ";
      }
      return success; 
    } else {
      // other message 
      LOG(ERROR) << "Unknown message to VideoDispatcherRoom";
    }
    return false; 
  }

  void VideoDispatcherRoom::RequestFirPacket(const int stream_id) {
    {
      boost::mutex::scoped_lock lock(room_plugin_mutex_);          

      if (current_viewer_ != -1 && stream_id != publisher_stream_id_) {
        // When requesting a key frame, set the interval limit of 2 seconds.
        long long current_time = getTimeMS();
        if (current_time < last_fir_time_ + 2000) {
          return;
        }
        last_fir_time_ = current_time;  

        std::vector<TransportPlugin*> participants = GetParticipants();
        SendFirPacket(current_viewer_, participants);
     }

    }
  }
}
