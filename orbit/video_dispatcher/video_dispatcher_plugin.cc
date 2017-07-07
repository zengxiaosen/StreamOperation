/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: qingyong@orangelab.cn (Qingyong Zhang)
 *
 * video_dispatch_plugin.cc
 * ---------------------------------------------------------------------------
 */

#include "video_dispatcher_plugin.h"
#include "video_dispatcher_room.h"

#include <boost/thread/mutex.hpp>

#include "stream_service/orbit/audio_processing/audio_energy.h"
#include "stream_service/orbit/rtp/janus_rtcp_processor.h"
#include "stream_service/orbit/live_stream/live_stream_processor.h"

#include "gflags/gflags.h"

namespace orbit {
using namespace std;

namespace {
}  // annoymous namespace

  bool VideoDispatcherPlugin::Init() {
    return true;
  }

  void VideoDispatcherPlugin::Release() {
  }

  void VideoDispatcherPlugin::RelayMediaOutputPacket(const std::shared_ptr<MediaOutputPacket> packet,
                                                     packetType packet_type) {
    if (packet->encoded_buf == NULL) {
      LOG(ERROR) << "OutputPacket is invalid.";
      return;
    }

    dataPacket p;
    p.comp = 0;
    p.type = packet_type;

    RtpHeader rtp_header;
    rtp_header.setTimestamp(packet->timestamp);
    rtp_header.setSeqNumber(packet->seq_number);
    rtp_header.setSSRC(packet->ssrc);
    rtp_header.setMarker(packet->end_frame);
    if (packet_type == VIDEO_PACKET) {
      rtp_header.setPayloadType(VP8_90000_PT);
    } else {
      rtp_header.setPayloadType(OPUS_48000_PT);
    }

    memcpy(&(p.data[0]), &rtp_header, RTP_HEADER_BASE_SIZE);
    memcpy(&(p.data[RTP_HEADER_BASE_SIZE]), packet->encoded_buf, packet->length);
    p.length = packet->length + RTP_HEADER_BASE_SIZE;

    VLOG(4) << "stream " << stream_id() << "'s energy:" << packet->audio_energy;


    // Call the gateway to relay the RTP packet to the participant.
    RelayRtpPacket(p);
  }

  void VideoDispatcherPlugin::IncomingRtpPacket(const dataPacket& packet) {
    TransportPlugin::IncomingRtpPacket(packet);
    VLOG(2) <<"Incoming RTP packet : Type = "<<packet.type;
    if (packet.type == AUDIO_PACKET) {
      // do nothing
    } else if (packet.type == VIDEO_PACKET) {
      video_queue_.pushPacket((const char *)&(packet.data[0]), packet.length);
    }
  }

  void VideoDispatcherPlugin::IncomingRtcpPacket(const dataPacket& packet) {
    TransportPlugin::IncomingRtcpPacket(packet);
    if (packet.type == AUDIO_PACKET) {
      // do nothing
    } else if (packet.type == VIDEO_PACKET) {
      const unsigned char* buf1 = reinterpret_cast<const unsigned char*>(packet.data);
      unsigned char* buf2 = const_cast<unsigned char*>(buf1);
      char* buf = reinterpret_cast<char*>(buf2);
      int len = packet.length;
      if(janus_rtcp_has_fir(buf, len) || janus_rtcp_has_pli(buf, len)) {
        /* We got a FIR, forward it to the publisher */
        auto vd_room = room_.lock();
        if(vd_room) {
          ((VideoDispatcherRoom*)vd_room.get())->SendFirPacketToViewer();
        }
      }
    }
  }

  void VideoDispatcherPlugin::OnTransportStateChange(TransportState state) {
    int cur_stream_id = stream_id();
    LOG(INFO) << "stream_id(" << cur_stream_id 
              << "):OnTransportStateChangestate=" << state;
    if (state == TRANSPORT_READY) {
      auto vd_room = room_.lock();
      if (vd_room != NULL) {
        LOG(INFO) << "stream_id(" << cur_stream_id << "):Request FIR packet...";
        ((VideoDispatcherRoom*)vd_room.get())->RequestFirPacket(cur_stream_id);
      }
    }
  }
}  // namespace orbit
