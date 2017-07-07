/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * video_bridge_plugin.cc
 * ---------------------------------------------------------------------------
 * Implements a Video Bridge plugin system:
 * ---------------------------------------------------------------------------
 */

#include "video_bridge_plugin.h"

namespace orbit {
using namespace std;

  void VideoBridgePlugin::IncomingRtpPacket(const dataPacket& packet) {
    TransportPlugin::IncomingRtpPacket(packet);
    vector<TransportPlugin*> plugins = room_->GetParticipants();
    if (plugins.size() == 2) {
      for (auto p : plugins) {
        VideoBridgePlugin* plugin = reinterpret_cast<VideoBridgePlugin*>(p);
        if (plugin->stream_id_ == stream_id_) continue;
LOG(INFO) << "This(" << stream_id_ << ") Relay a rtp packet..to" << plugin->stream_id_ << ".." << " size=" << packet.length;
        plugin->RelayRtpPacket(packet);
      }
    }
  }
  void VideoBridgePlugin::IncomingRtcpPacket(const dataPacket& packet) {
    TransportPlugin::IncomingRtcpPacket(packet);
    vector<TransportPlugin*> plugins = room_->GetParticipants();
    if (plugins.size() == 2) {
      for (auto p : plugins) {
        VideoBridgePlugin* plugin = reinterpret_cast<VideoBridgePlugin*>(p);
        if (plugin->stream_id_ == stream_id_) continue;
LOG(INFO) << "This(" << stream_id_ << ") Relay a rtcp packet..to" << plugin->stream_id_ << ".." << " size=" << packet.length;
        plugin->RelayRtcpPacket(packet);
      }
    }
  }

}  // namespace orbit
