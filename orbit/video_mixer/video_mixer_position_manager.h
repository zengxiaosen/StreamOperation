/*
 * Copyright 2016 All Rights Reserved.
 * Author: qingyong@orangelab.cn (Qingyong Zhang)
 *
 * video_mixer_position_manager.h
 * --------------------------------------------------
 * Arrange video plugin's position when mixing stream
 * --------------------------------------------------
 */

#ifndef VIDEO_MIXER_INCLUDE_VIDEO_MIXER_POSITINO_MANAGER_H_
#define VIDEO_MIXER_INCLUDE_VIDEO_MIXER_POSITINO_MANAGER_H_

//#pragma once

#include "stream_service/orbit/video_mixer/position_update_listener.h"
#include "stream_service/orbit/rtp/rtp_packet_queue.h"
#include "stream_service/orbit/rtp/rtp_headers.h"
#include "stream_service/orbit/modules/media_packet.h"
#include "video_mixer_position.h"
namespace orbit{
using namespace std;
class VideoMixerPluginImpl;

struct Rect{
  int width;
  int height;
};
class VideoMixerPositionManager{
private:
  PositionUpdateListener* listener_;
  vector<MixerPosition> positions_;
  boost::mutex positions_mutex_;
public:
  VideoMixerPositionManager(PositionUpdateListener*  listener);
  ~VideoMixerPositionManager();
  void UpdateVideoPosition();
  void AddStream(VideoMixerPluginImpl* session);
  void RemoveStream(VideoMixerPluginImpl* plugin);
  Rect GetOutVideoSize();
};


}


#endif
