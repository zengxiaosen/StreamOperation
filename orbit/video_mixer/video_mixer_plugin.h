/*/*
 * Copyright 2016 All Rights Reserved.
 * Author: qingyong@orangelab.cn (Qingyong Zhang)
 *
 *
 ***************************************************************************************
 * PeerConnection-->AppSrc-->RtpDepay-->Vp8Enc-->VideoConvert
                                                               -->VideoMixer-->Tee-->webcast
 * PeerConnection-->AppSrc-->RtpDepay-->Vp8Enc-->VideoConvert
 *|----------------------------------------------------------|------------------------------|
 *                         VideoMixerPlugin                          VideoMixerRoom
 ***************************************************************************************
 *
 *
 *  Created on: Mar 4, 2016
 *      Author: vellee
 */

#ifndef VIDEO_MIXER_PLUGIN_H_
#define VIDEO_MIXER_PLUGIN_H_

#include "stream_service/orbit/transport_plugin.h"
#include "stream_service/orbit/rtp/rtp_packet_queue.h"
#include "stream_service/orbit/rtp/rtp_headers.h"
#include "stream_service/orbit/modules/media_packet.h"
#include "stream_service/orbit/modules/audio_buffer_manager.h"
#include "stream_service/orbit/modules/audio_mixer_element.h"

#include <vector>

#include <boost/thread/mutex.hpp>
#include <boost/scoped_ptr.hpp>

#include "gflags/gflags.h"

#include "stream_service/orbit/video_mixer/video_mixer_position.h"
#include "video_mixer_room.h"

#include <gstreamer-1.5/gst/gst.h>

DECLARE_int32(video_width);
DECLARE_int32(video_height);
namespace orbit {

class VideoMixerPlugin : public TransportPlugin, public IAudioMixerRtpPacketListener{
  public:
    VideoMixerPlugin(int stream_id,
                     int video_width = FLAGS_video_width,
                     int video_height = FLAGS_video_height);

    ~VideoMixerPlugin() {
      Release();
    }

    // Overrides the interface in TransportPlugin
    void IncomingRtpPacket(const dataPacket& packet) override;
    void IncomingRtcpPacket(const dataPacket& packet) override;
    void OnAudioMixed(const std::shared_ptr<MediaOutputPacket> packet);
    void RelayMediaOutputPacket(const std::shared_ptr<MediaOutputPacket> packet,
                                packetType packet_type);
    void Disconnect();
    int stream_id() {
      return stream_id_;
    }

    void SendFirPacket();
    // Other stuff.
    void Stop();
    bool is_stoped(){
      return stoped_;
    }
    bool limit_video_size() {
      return limit_frame_size_;
    }
  private:
    bool Init();
    void Release();

    bool stoped_ = false;
    int stream_id_;

    bool limit_frame_size_ = true;

    int video_width_;
    int video_height_;
    int fir_index_;
};

class VideoMixerRoom;
class VideoMixerPluginImpl :public VideoMixerPlugin {
 public:
  VideoMixerPluginImpl(std::weak_ptr<Room> room,
                       int stream_id,
                       int video_width = FLAGS_video_width,
                       int video_height = FLAGS_video_height);
  ~VideoMixerPluginImpl();
  // Overrides the interface in TransportPlugin
  void IncomingRtpPacket(const dataPacket& packet) override;

  void UpdateVideoPosition(int x, int y, bool imediately);
  void UnlinkElements();
  void RemoveElements();
  void OnTransportStateChange(TransportState state) override;
  GstPad* GetMixerSinkPad() {
    return sink_pad_;
  }
  GstElement* GetVideoSrc() {
    return app_src_;
  }
  std::shared_ptr<dataPacket> PopVideoPacket();
 private:
  GstElement *app_src_ = NULL;
  GstElement *rtp_vp8_depay_ = NULL;
  GstElement *vp8_dec_ = NULL;
  GstElement *jitter_buffer_ = NULL;
  GstElement *queue_ = NULL;
  GstElement *video_rate_ = NULL;

  GstPad *sink_pad_ = NULL;

  RtpPacketQueue video_queue_;  // inbuf
  std::weak_ptr<Room> room_;
  int x_;
  int y_;
};

}  // namespace orbit

#endif /* VIDEO_MIXER_PLUGIN_H_ */
