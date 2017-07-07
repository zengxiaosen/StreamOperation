/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: qingyong@orangelab.cn (Qingyong Zhang)
 *
 * webcast.h
 * ---------------------------------------------------------------------------
 * Defines the interface to implement a gstreamer module
 * ---------------------------------------------------------------------------
 */
#ifndef LIVE_STREAM_PROCESSOR_H__
#define LIVE_STREAM_PROCESSOR_H__

#include "common_defines.h"
#include "live_stream_processor_impl.h"

#include <gstreamer-1.5/gst/gst.h>
#include <gst/app/gstappsrc.h>
#include "glog/logging.h"

namespace orbit {
  class MediaOutputPacket;
  class dataPacket;
  class LiveStreamProcessorImpl;
  class IKeyFrameNeedListener {
  public:
    virtual void OnKeyFrameNeeded() = 0;
  };
  class LiveStreamProcessor {
  public :
    LiveStreamProcessor(bool has_audio, bool has_video);
    LiveStreamProcessor(GstElement* pipeline, GstElement* video_tee);
    /**
     * @rtmp_location : Rtmp location.
     * @return
     *       true: Start succeed;
     *       false: Start failed;
     */
    bool Start(const char* rtmp_location);
    void RelayRtpVideoPacket(const std::shared_ptr<MediaOutputPacket> packet);
    void RelayRtpAudioPacket(const std::shared_ptr<MediaOutputPacket> packet);
    void RelayRawAudioPacket(const char* outBuffer, int size);
    void RelayRtpAudioPacket(const dataPacket& packet);
    void RelayRtpVideoPacket(const dataPacket& packet);

    void SetKeyFrameNeedListener(std::shared_ptr<IKeyFrameNeedListener> listener);
    bool IsStarted();
    ~LiveStreamProcessor();
  private:
    LiveStreamProcessorImpl* internal_;
  };
}  // namespace orbit

#endif  // LIVE_STREAM_PROCESSOR_H__
