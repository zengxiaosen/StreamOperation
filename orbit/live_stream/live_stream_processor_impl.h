/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: qingyong@orangelab.cn (Qingyong Zhang)
 *
 * webcast.h
 * ---------------------------------------------------------------------------
 * Defines the interface to implement a gstreamer module
 * ---------------------------------------------------------------------------
 */
#ifndef LIVE_STREAM_PROCESSER_IMPL_H__
#define LIVE_STREAM_PROCESSER_IMPL_H__

#include "stream_service/orbit/rtp/rtp_headers.h"
#include "stream_service/orbit/modules/rtp_packet_buffer.h"
#include "stream_service/orbit/live_stream/common_defines.h"
#include "stream_service/orbit/live_stream/live_stream_processor.h"

#include <gstreamer-1.5/gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <glib-2.0/glib.h>
#include "glog/logging.h"
#include <memory>
#include <list>

namespace orbit {
  class MediaOutputPacket;
  class dataPacket;
  class IKeyFrameNeedListener;

  struct audioDataPacket{
      char data[2000];
      int length = 0;
  };
/**
 *
 * Current audio :webrtc_endpoint_audio_ssrc -> audio_mixer -> appsrc -> rtpopusdepay -> opusedc -> autoaudiosink
 * Current video :webrtc_endpoint_video_ssrc -> appsrc -> rtpvp8depay -> vp8dec -> autovideosink
 * Can replace audio :webrtc_endpoint_audio_ssrc -> audio_mixer ->appsrc -> autoaudiosink
 *
 */
  class LiveStreamProcessorImpl {
    /** 
     * Define a enum for instance type of live here, since there are two constructors.
     * The difference between the two is that pipeline from a caller outside or itself inside.
     * @author stephen
     */
    enum LiveInstType {
      LIVE_INTERNAL_PIPELINE = 1,
      LIVE_EXTERNAL_PIPELINE = 2,
    };
  public :
    LiveStreamProcessorImpl(bool has_audio, bool has_video);
    LiveStreamProcessorImpl(GstElement* pipeline, GstElement* video_tee);
    ~LiveStreamProcessorImpl();
    /**
     * @rtmp_location : Rtmp location.
     * @return
     *       true: Start succeed;
     *       false: Start failed;
     */
    bool Start(const char* rtmp_location);
    bool IsStarted();
    bool IsStopped();
    void SyncElements();
    void RelayRtpAudioPacket(const std::shared_ptr<MediaOutputPacket> packet);
    void RelayRtpVideoPacket(const std::shared_ptr<MediaOutputPacket> packet);
    void RelayRawAudioPacket(const char* outBuffer, int size);
    void RelayRtpAudioPacket(const dataPacket& packet);
    void RelayRtpVideoPacket(const dataPacket& packet);
    void SetKeyFrameNeedListener(std::shared_ptr<IKeyFrameNeedListener> listener);

    GstElement* GetAudioSrc() {
      return audio_app_src_;
    }
    GstElement* GetVideoSrc() {
      return video_app_src_;
    }
    GstElement* GetPipeline() {
      return pipeline_;
    }

    std::shared_ptr<dataPacket> PopVideoPacket();
    std::shared_ptr<dataPacket> PopAudioPacket();
  private:
    void SetupVideoElements();
    void SetupAudioElements();
    void SetupRtmpElements();

    void Destroy();
    void DestroyElements();
    boost::mutex queue_mutex_;
    std::list<std::shared_ptr<audioDataPacket>> queue_;
    std::shared_ptr<RtpPacketBuffer> video_queue_;  // inbuf
    std::shared_ptr<RtpPacketBuffer> audio_queue_;  // inbuf
    int last_video_seq_ = -1;
    int last_audio_seq_ = -1;

    long audio_start_time_ = 0;
    long audio_delay_time_ = 0;
    long audio_ntp_start_time_ = 0;

    long video_start_time_ = 0;
    long video_delay_time_ = 0;
    long video_ntp_start_time_ = 0;

    GstElement *pipeline_ = NULL;
    GstElement *audio_app_src_ = NULL;
    GstElement *voaacenc_ = NULL;
    GstElement *opusdepay_ = NULL;
    GstElement *opusdec_ = NULL;
    GstElement *audioconvert_ = NULL;
    GstElement *audioresample_ = NULL;
    GstElement *audio_jitter_ = NULL;

    GstElement *video_app_src_ = NULL;
    GstElement *clock_overlay_ = NULL;
    GstElement *video_convert_ = NULL;
    GstElement *video_rate_ = NULL;
    GstElement *x264enc_ = NULL;
    GstElement *video_rtp_depay_ = NULL;
    GstElement *vp8dec_ = NULL;
    GstElement *video_jitter_ = NULL;
    GstElement *video_scale_ = NULL;
    GstElement *scale_caps_filter_ = NULL;

    GstElement *flv_mux_ = NULL; 
    GstElement *rtmp_sink_ = NULL;

    //Support connect video element from other tee
    GstElement *video_tee_ = NULL;
    GstElement *tee_queue_ = NULL;

    std::shared_ptr<IKeyFrameNeedListener> key_frame_listener_ ;
    bool started_ = false;
    bool stopped_ = false;
    
    int fir_index_;
    int last_fir_ = 0;
    int last_seq_ = -1;
    bool lose_packet_ = false;
    bool waiting_key_frame_ = false;

    bool video_queue_data_ = false;
    
    // instance type decided by which constructor caller using
    LiveInstType inst_type = LIVE_INTERNAL_PIPELINE;
  };
}  // namespace orbit

#endif  // LIVE_STREAM_PROCESSER_IMPL_H__
