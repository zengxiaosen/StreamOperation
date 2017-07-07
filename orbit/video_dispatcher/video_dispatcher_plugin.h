/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: qingyong@orangelab.cn (Qingyong Zhang)
 *
 * video_dispatcher_plugin.h
 * ---------------------------------------------------------------------------
 * ---------------------------------------------------------------------------
 */
#ifndef VIDEO_DISPATCHER_VIDEO_DISPATCHER_PLUGIN_H_
#define VIDEO_DISPATCHER_VIDEO_DISPATCHER_PLUGIN_H_

#include "stream_service/orbit/transport_plugin.h"
#include "stream_service/orbit/modules/media_packet.h"
#include "stream_service/orbit/rtp/rtp_packet_queue.h"
#include "stream_service/orbit/rtp/rtp_headers.h"

namespace orbit{
class VideoDispatcherPlugin : public TransportPlugin {
  public:
  VideoDispatcherPlugin(std::weak_ptr<Room> room, int stream_id) {
      room_ = room;
      stream_id_ = stream_id;

      muted_ = false;
      prebuffering_ = true;
      active_ = true;

      if (!Init()) {
        LOG(ERROR) << "Create the audioRoom, init failed.";
      }
    }
    ~VideoDispatcherPlugin() {
      Release();
    }

    // Overrides the interface in TransportPlugin
    void OnTransportStateChange(TransportState state) override;
    // Overrides the interface in TransportPluginInterface
    void IncomingRtpPacket(const dataPacket& packet) override;
    void IncomingRtcpPacket(const dataPacket& packet) override;

    void RelayMediaOutputPacket(const std::shared_ptr<MediaOutputPacket>  packet,
                                packetType packet_type);

    // Getter/Setter for the local variables.
    bool muted() {
      return muted_;
    }
    bool prebuffering() {
      return prebuffering_;
    }
    void set_muted(bool muted) {
      muted_ = muted;
    }
    void set_prebuffering(bool prebuffering) {
      prebuffering_ = prebuffering;
    }
    int stream_id() {
      return stream_id_;
    }

  private:
    bool Init();
    void Release();
    RtpPacketQueue* GetVideoQueue() {
      return &video_queue_;
    }

    RtpPacketQueue video_queue_;  // inbuf

    bool muted_;
    bool prebuffering_;

    std::weak_ptr<Room> room_;
    int stream_id_;
    friend class VideoDispatcherRoom;
  };
}

#endif /* VIDEO_DISPATCHER_VIDEO_DISPATCHER_PLUGIN_H_ */
