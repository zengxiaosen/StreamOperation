/*
 * Copyright 2016 All Rights Reserved.
 * Author: qingyong@orangelab.cn (Qingyong Zhang)
 *
 * mixer_position_manager.h
 * --------------------------------------------------
 * Arrange video plugin's position when mixing stream
 * --------------------------------------------------
 */

#include "stream_service/orbit/video_mixer/video_mixer_position_manager.h"
#include "stream_service/orbit/rtp/rtp_packet_queue.h"
#include "stream_service/orbit/rtp/rtp_headers.h"
#include "stream_service/orbit/modules/media_packet.h"
#include <vector>

#include "video_mixer_plugin.h"

#include <boost/thread/mutex.hpp>
#include <boost/scoped_ptr.hpp>

#include <opus/opus.h>
#include <math.h>
#define DEFAULT_VIDEO_WIDTH  180
#define DEFAULT_VIDEO_HEIGHT 180

#define VIDEO_HORIZONTIAL_SPACE 10
#define VIDEO_VERTICAL_SPACE 10

DEFINE_int32(video_width, DEFAULT_VIDEO_WIDTH, "If set, it will change mixer video frame width.");
DEFINE_int32(video_height, DEFAULT_VIDEO_HEIGHT, "If set, it will change mixer video frame height.");

namespace orbit{
  static int GetColumn(int size){
    int column = 1;
    switch(size){
      case 1:
      case 2:
        column = size;
        break;
      case 3:
      case 4:
        column = 2;
        break;
      case 5:
      case 6:
      case 7:
      case 8:
      case 9:
        column = 3;
        break;
      default:
        column = 4;
//        column = sqrt(size);
    }
    return column                                                                                                                                                             ;
}
  class VideoMixerPluginImpl;
  VideoMixerPositionManager::VideoMixerPositionManager(PositionUpdateListener *listener){
    listener_ = listener;
  }
  VideoMixerPositionManager::~VideoMixerPositionManager(){

  }
  void VideoMixerPositionManager::AddStream(VideoMixerPluginImpl* plugin){
    if(plugin == NULL) {
      return;
    }

    boost::mutex::scoped_lock lock(positions_mutex_);
    int stream_id = plugin->stream_id();
    size_t len = positions_.size();
    for (size_t i =0; i < len; i ++) {
      MixerPosition position = positions_[i];
      if(position.stream_id() == stream_id){
          return;
      }
    }

    MixerPosition position;
    position.set_plugin(plugin);
    position.set_stream_id(plugin->stream_id());
    positions_.push_back(position);
  }
  void VideoMixerPositionManager::UpdateVideoPosition(){
    bool updated = false;
    {
      boost::mutex::scoped_lock lock(positions_mutex_);
      size_t len = positions_.size();
      int column = GetColumn(len);
      int max_row = len/column;
      int last_row_count = len%column;
      if(last_row_count > 0){
        max_row++;
      }
      VLOG(2)<<"Last is "<<last_row_count;
      VLOG(2)<<"Count is "<<len;
      for (size_t i =0; i < len; i ++) {
        MixerPosition position = positions_[i];
        int x,y;
        int row = i/column;
        int col_pos = i%column;
        VLOG(2)<<"col_pos is "<<col_pos;
        VLOG(2)<<"max_row is "<<max_row;
        VLOG(2)<<"row is "<<row;
        if(row>0  && row == (max_row-1) && last_row_count>0){
          int total_width = (FLAGS_video_width + VIDEO_HORIZONTIAL_SPACE)*column - VIDEO_HORIZONTIAL_SPACE;
          VLOG(2)<<"total_width is "<<total_width;
          int last_width = (FLAGS_video_width + VIDEO_HORIZONTIAL_SPACE)*last_row_count - VIDEO_HORIZONTIAL_SPACE;
          VLOG(2)<<"last_width is "<<last_width;
          x =(total_width-last_width)/2 + (FLAGS_video_width + VIDEO_HORIZONTIAL_SPACE)*col_pos;
        } else {
          x = (FLAGS_video_width + VIDEO_HORIZONTIAL_SPACE)*col_pos;
        }
        if(row ==0){
          y = 0;
        } else {
          y = (FLAGS_video_height + VIDEO_VERTICAL_SPACE)*row;
        }
        VLOG(2)<<"____________________________________________________"<<i<<"x ="<<x<<"  y = "<<y;
        if(position.UpdatePosition(x,y)){
          position.plugin()->UpdateVideoPosition(x,y,false);
          updated = true;
        }
      }
    }
    if(updated && listener_){
      listener_->OnPositionUpdated();
    }
  }
  void VideoMixerPositionManager::RemoveStream(VideoMixerPluginImpl* plugin) {
    int stream_id = plugin->stream_id();
    VLOG(2)<<"------------------------------RemoveStream---------------------------------"<<stream_id;
    boost::mutex::scoped_lock lock(positions_mutex_);

    for(vector<MixerPosition>::iterator iter= positions_.begin();iter!=positions_.end(); ++iter){
      int cur_stream_id = (*iter).stream_id();
      VLOG(2)<<"Stream id ："<<cur_stream_id<<"will be removed";
      if(stream_id == cur_stream_id){
        VLOG(2)<<"Stream id ："<<stream_id<<"will be removed";
        positions_.erase(iter);
        break;
      };
    }

  }
  Rect VideoMixerPositionManager::GetOutVideoSize(){
    Rect rect;
    boost::mutex::scoped_lock lock(positions_mutex_);
    size_t size = positions_.size();
    int column = GetColumn(size);
    int row = size/column;
    if(size%column > 0){
      row ++;
    }
    rect.width = (FLAGS_video_width + VIDEO_HORIZONTIAL_SPACE)*column - VIDEO_HORIZONTIAL_SPACE;
    rect.height = (FLAGS_video_height + VIDEO_VERTICAL_SPACE)*row - VIDEO_VERTICAL_SPACE;
    return rect;
  }
}
