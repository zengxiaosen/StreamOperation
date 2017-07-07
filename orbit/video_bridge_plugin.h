/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * video_bridge_plugin.h
 * ---------------------------------------------------------------------------
 * Defines the interface to implement a 2 party video bridge meeting plugin
 * ---------------------------------------------------------------------------
 */

#ifndef VIDEO_BRIDGE_PLUGIN_H__
#define VIDEO_BRIDGE_PLUGIN_H__

#include "transport_plugin.h"

#include <vector>

namespace orbit {
  class VideoBridgePlugin : public TransportPlugin {
  public:
    VideoBridgePlugin(std::shared_ptr<Room> room, int stream_id) {
      room_ = room;
      stream_id_ = stream_id;
    }
    // Overrides the interface in TransportPlugin
    void IncomingRtpPacket(const dataPacket& packet);
    void IncomingRtcpPacket(const dataPacket& packet);

    // Other stuff.
  private:
    std::shared_ptr<Room> room_;
    int stream_id_;
  };
}  // namespace orbit


#endif  // VIDEO_BRIDGE_PLUGIN_H__
