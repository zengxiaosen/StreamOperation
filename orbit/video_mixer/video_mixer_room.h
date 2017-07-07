/*
 * video_mixer_room.h
 *
 *  ***************************************************************************************
 * PeerConnection-->AppSrc-->RtpDepay-->Vp8Enc-->VideoConvert \
 *                                                             -->VideoMixer-->Tee-->webcast
 * PeerConnection-->AppSrc-->RtpDepay-->Vp8Enc-->VideoConvert /
 *|----------------------------------------------------------|------------------------------|
 *                         VideoMixerPlugin                          VideoMixerRoom
 ***************************************************************************************
 *  Created on: Mar 4, 2016
 *      Author: vellee
 */

#ifndef VIDEO_MIXER_INCLUDE_VIDEO_MIXER_ROOM_H_
#define VIDEO_MIXER_INCLUDE_VIDEO_MIXER_ROOM_H_

#include "stream_service/orbit/transport_plugin.h"
#include "stream_service/orbit/rtp/rtp_packet_queue.h"
#include "stream_service/orbit/rtp/rtp_headers.h"
#include "stream_service/orbit/modules/media_packet.h"
#include <vector>

// Live stream and recorder element
#include "stream_service/orbit/live_stream/live_stream_processor.h"
#include "stream_service/orbit/modules/stream_recorder_element.h"

#include "video_mixer_plugin.h"
#include "video_mixer_room.h"
#include "video_mixer_position.h"
#include "video_mixer_position_manager.h"
#include "stream_service/orbit/video_mixer/position_update_listener.h"

#include "stream_service/orbit/modules/audio_mixer_element.h"
#include "stream_service/orbit/modules/audio_event_listener.h"
// GStreamer and related.
#include <gstreamer-1.5/gst/gst.h>

namespace orbit {
class VideoMixerPositionManager;
class MixerPosition;
class TransportPlugin;
class VideoMixerRoom : public Room,
                       public PositionUpdateListener,
                       public IAudioEventListener,
                       public IAudioMixerRawListener,
                       public IAudioMixerRtpPacketListener {
  public:
    VideoMixerRoom(int32_t session_id, bool support_live_stream);
    ~VideoMixerRoom();

    void Create() override { };
    void SyncElements();
    void PullAppSinkData();
    int GetVideoCount(){ return plugins_.size(); };
    void Destroy() override;
    void Start() override;
    void OnPositionUpdated();
    void OnPluginRemoved(int stream_id);
    void SetupRecordStream();
    void AddParticipant(TransportPlugin* plugin) override;
    bool RemoveParticipant(TransportPlugin* plugin) override;
    void IncomingRtpPacket(const int stream_id, const dataPacket& packet) override;
    GstElement* GetTee() { return tee_;};
    GstElement* GetAppSink(){ return app_sink_; }
    GstElement* GetMixerPipeline(){ return pipeline_; }
    GstElement* GetVideoMixer(){ return video_mixer_; }
    void OnPacketLoss(int stream_id, int percent);
    bool MuteStream(const int stream_id,const bool mute);
    void OnAudioMixed(const char* outBuffer, int size);
    void OnAudioMixed(const std::shared_ptr<MediaOutputPacket> packet);

    virtual void OnTransportStateChange(const int stream_id, const TransportState state) override;
    virtual void SetupLiveStream(bool support, bool need_return_video,
                                 const char* rtmp_location) override;
  private:

    const int32_t session_id_;
    bool use_live_stream_ = false;
    GstElement* pipeline_ = NULL;
    GstElement* app_sink_ = NULL;
    GstElement* video_mixer_ = NULL;
    GstElement* caps_filter_ = NULL;

    GstElement* voaacenc = NULL;
    GstElement* vp8enc_ = NULL;
    GstElement* rtp_pay_ = NULL;
    boost::scoped_ptr<boost::thread> pull_sink_thread_;
    GstElement* tee_ = NULL;

    //Live stream element
    LiveStreamProcessor *live_stream_processor_ = NULL;
    StreamRecorderElement *stream_recorder_element_ = NULL;

    VideoMixerPositionManager *position_manager_ = NULL;
    std::unique_ptr<AbstractAudioMixerElement> audio_mixer_element_ = NULL;

    bool running_;
    int sampling_rate_;

  };

}  // namespace orbit

#endif /* VIDEO_MIXER_INCLUDE_VIDEO_MIXER_ROOM_H_ */
