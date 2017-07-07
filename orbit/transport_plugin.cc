/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * transport_plugin.cc
 * ---------------------------------------------------------------------------
 * Implements the TransportPlugin:
 *  - The inheritted class should always implement all the interface except
 *    the relay* methods.
 *  - relay* methods are used to relay the packets to the original sender in
 *    the session
 *  - incoming* methods are callback functions that are called everytime the
 *    the transport layer receives the rtp/rtcp packets.
 * 
 * ---------------------------------------------------------------------------
 */

#include "transport_plugin.h"
#include "rtp/rtp_headers.h"
namespace orbit {
  void TransportPlugin::IncomingRtpPacket(const dataPacket& packet) {
  }
  void TransportPlugin::IncomingRtcpPacket(const dataPacket& packet) {
  }
  void TransportPlugin::RelayRtpPacket(const dataPacket& packet) {
     const RtpHeader* header = reinterpret_cast<const RtpHeader*>(packet.data);
     VLOG(3) << " VideoPacket payload=" << (int)(header->getPayloadType())
             << " ts=" <<  header->getTimestamp()
             << " seq=" << header->getSeqNumber()
             << " h->getMarker()=" << (int)(header->getMarker())
             << " headerLength=" << header->getHeaderLength()
             << " pkt.length=" << packet.length;
    boost::mutex::scoped_lock lock_gateway(gateway_mutex_);
    if(gateway_ != NULL)
        gateway_->RelayPacket(packet);
  }
  void TransportPlugin::RelayRtcpPacket(const dataPacket& packet) {
    boost::mutex::scoped_lock lock_gateway(gateway_mutex_);
    if(gateway_ != NULL)
        gateway_->RelayPacket(packet);
  }
  void TransportPlugin::Stop() {
	  boost::mutex::scoped_lock lock_gateway(gateway_mutex_);
	  gateway_ = NULL;
  }

  void SimpleEchoPlugin::IncomingRtpPacket(const dataPacket& packet) {
    TransportPlugin::IncomingRtpPacket(packet);
    RelayRtpPacket(packet);
  }
  void SimpleEchoPlugin::IncomingRtcpPacket(const dataPacket& packet) {
    TransportPlugin::IncomingRtcpPacket(packet);
    if (packet.type == AUDIO_PACKET) {
      // Skip for now
    } else if (packet.type == VIDEO_PACKET) {
      RelayRtcpPacket(packet);
    }
  }

}  // namespace orbit
