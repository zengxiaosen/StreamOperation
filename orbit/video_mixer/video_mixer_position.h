/*
 * mixer_position.h
 *
 *  Created on: Mar 18, 2016
 *      Author: vellee
 */

#ifndef VIDEO_MIXER_INCLUDE_VIDEO_MIXER_POSITION_H_
#define VIDEO_MIXER_INCLUDE_VIDEO_MIXER_POSITION_H_

#define PRIORITY_HIGH 3
#define PRIORITY_NORMAL 4
#define PRIORITY_LOW 5
namespace orbit{
  class VideoMixerPluginImpl;
  class MixerPosition{
  private:
    int stream_id_;
    int priority = 4;

    int x_;
    int y_;
    int width_;
    int height_;
    VideoMixerPluginImpl* plugin_;
  public:
    /**
     * @return position changed;
     */
    bool UpdatePosition(int x, int y){
      bool changed = false;
      if(x_ != x ||y_ != y){
        changed = true;
        x_ = x;
        y_ = y;
      }
      return changed;
    }
    VideoMixerPluginImpl* plugin(){
      return plugin_;
    }
    void set_plugin(VideoMixerPluginImpl* plugin){
      plugin_ = plugin;
    }
    void set_stream_id(int stream_id){
      stream_id_ = stream_id;
    }
    int stream_id(){
      return stream_id_;
    }
    void set_x(int x) {
      x_ = x;
    }
    void set_y(int y) {
      y_ = y;
    }
    int x(){
      return x_;
    }
    int y(){
      return y_;
    }
  };
}
#endif /* VIDEO_MIXER_INCLUDE_VIDEO_MIXER_POSITION_H_ */
