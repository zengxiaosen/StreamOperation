/*
 * remb_processor.cc
 *
 *  Created on: May 30, 2016
 *      Author: chenteng
 */

#include "remb_processor.h"
#include "janus_rtcp_processor.h"
#include "stream_service/orbit/transport_delegate.h"

#define USEC_PER_SEC 1000000
#define SLOW_LINK_NACKS_PER_SEC 8

namespace orbit {

RembProcessor::RembProcessor(TransportDelegate *trans_delegate) {
  // TODO Auto-generated constructor stub
  bitrate_ = 0;
  trans_delegate_ = trans_delegate;
}

RembProcessor::~RembProcessor() {
  // TODO Auto-generated destructor stub
}

void RembProcessor::SetBitrate(uint64_t bitrate) {
  bitrate_ =  bitrate;
}

/* Call SlowLink function if enough NACKs within a second */
void RembProcessor::SlowLinkUpdate(
  unsigned int nacks, int video, int uplink, uint64_t now) {
  if(now - nack_period_ts_ > 2 * USEC_PER_SEC) {
    /* old nacks too old, don't count them */
    nack_period_ts_ = now;
    nack_recent_cnt_ = 0;
  }
  nack_recent_cnt_ += nacks;
  if(nack_recent_cnt_ >= SLOW_LINK_NACKS_PER_SEC
     && now - last_slowlink_time_ > 1 * USEC_PER_SEC) {
    VideoCallSlowLink(uplink, video);
    last_slowlink_time_ = now;
    nack_period_ts_ = now;
    nack_recent_cnt_ = 0;
  }
}

void RembProcessor::VideoCallSlowLink(int uplink, int video) {
  if(uplink && !video) {
      /* We're not relaying audio and the peer is expecting it, so NACKs are normal */
//      JANUS_LOG(LOG_VERB, "Getting a lot of NACKs (slow uplink) for audio, but that's expected, a configure disabled the audio forwarding\n");
//    } else if(uplink && video ) {
      /* We're not relaying video and the peer is expecting it, so NACKs are normal */
//      JANUS_LOG(LOG_VERB, "Getting a lot of NACKs (slow uplink) for video, but that's expected, a configure disabled the video forwarding\n");
  } else if(!uplink && video ) {
    /* Slow uplink or downlink, maybe we set the bitrate cap too high? */
    if(video) {
      /* Halve the bitrate, but don't go too low... */
      if(!uplink) {
        /* Downlink issue, user has trouble sending, halve this user's bitrate cap */
        bitrate_ = bitrate_ > 0 ? bitrate_ : 512*1024;
        bitrate_ = bitrate_/2;
        if(bitrate_ < 64*1024)
          bitrate_ = 64*1024;
      } else {
        /* Uplink issue, user has trouble receiving, halve this user's peer's bitrate cap */
//          if(session->peer == NULL || session->peer->handle == NULL)
//            return; /* Nothing to do */
//          session->peer->bitrate = session->peer->bitrate > 0 ? session->peer->bitrate : 512*1024;
//          session->peer->bitrate = session->peer->bitrate/2;
//          if(session->peer->bitrate < 64*1024)
//            session->peer->bitrate = 64*1024;
      }
//        JANUS_LOG(LOG_WARN, "Getting a lot of NACKs (slow %s) for %s, forcing a lower REMB: %"SCNu64"\n",
//          uplink ? "uplink" : "downlink", video ? "video" : "audio", uplink ? session->peer->bitrate : bitrate_);
      LOG(INFO) << "Getting a lot of NACKs , forcing a lower REMB:" << bitrate_;
      /* ... and send a new REMB back */
      if(!uplink) {
        SendRembPacket(bitrate_);
      }
    }
  }
} /* VideoCallSlowLink */

void RembProcessor::SendRembPacket(uint64_t bitrate) {
  char rtcpbuf[24];
  VLOG(3) <<"%%%%%%%%%%%%%%%%%%%%%% Send remb = " << bitrate;
  janus_rtcp_remb((char *)(&rtcpbuf), 24, bitrate);
  /* Enqueue it, we'll send it later */
  dataPacket packet;
  packet.comp = 0;
  packet.length = 24;
  packet.type = REMB_PACKET;
  memcpy(&(packet.data[0]), (unsigned char*)rtcpbuf, 24);
 // trans_delegate_->RelayPacket(packet);
}


} /* namespace orbit */
