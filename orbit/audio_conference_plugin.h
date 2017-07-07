/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * audio_conference_plugin.h
 * ---------------------------------------------------------------------------
 * Defines the interface to implement a audio conference meeting plugin
 * ---------------------------------------------------------------------------
 */

#ifndef AUDIO_CONFERENCE_PLUGIN_H__
#define AUDIO_CONFERENCE_PLUGIN_H__

#include "transport_plugin.h"
#include "rtp/rtp_packet_queue.h"
#include "rtp/rtp_headers.h"
#include "stream_service/orbit/live_stream/live_stream_processor.h"
#include "modules/media_packet.h"
#include "modules/video_forward_element.h"
#include "modules/audio_mixer_element.h"
#include "modules/audio_event_listener.h"
#include "modules/stream_recorder_element.h"

#include <vector>

#include <boost/thread/mutex.hpp>
#include <boost/scoped_ptr.hpp>

#include <opus/opus.h>

namespace orbit {
  class AudioConferencePlugin : public TransportPlugin , public IAudioMixerRtpPacketListener{
  public:
    AudioConferencePlugin(std::weak_ptr<Room> room, int stream_id);
    ~AudioConferencePlugin() {
    	LOG(INFO) <<"Enter into ~AudioConferencePlugin";
    }

    // Overrides the interface in TransportPlugin
    void IncomingRtpPacket(const dataPacket& packet) override;
    void IncomingRtcpPacket(const dataPacket& packet) override;

    /**
     * Event for ice connection state change.
     */
    void OnTransportStateChange(TransportState state) override;
    // Overrides the functions in IAudioMixerRtpPacketListener
    void OnAudioMixed(const std::shared_ptr<MediaOutputPacket> packet) override;

    void RelayMediaOutputPacket(const std::shared_ptr<MediaOutputPacket> packet,
                                packetType packet_type);
    int stream_id() {
      return stream_id_;
    }
  protected:
    // Support two kinds of Listener interfaces:
    //  - IVideoPacketListener
    //  - AudioPacketListener
    // Holds the weak reference to the Room
    std::weak_ptr<Room> room_;
    // The stream_id of the plugin in the delegate.
    int stream_id_;
    // Records the recent video_payload_ (could be VP8, VP9 or H264).
    int video_payload_ = 0;
  };

  class IKeyFrameNeedListener;
  class AudioConferenceRoom : public Room,
                              public IAudioMixerRawListener,
                              public IAudioMixerRtpPacketListener,
                              public IAudioEventListener,
                              public IKeyFrameNeedListener,
                              public IVideoForwardEventListener,
                              public ISpeakerChangeListener {
  public:
    AudioConferenceRoom(int32_t session_id, bool use_webcast, bool support_video);
    ~AudioConferenceRoom();
    // The lifetime management of the Room.
    virtual void Create() override;
    virtual void Destroy() override;
    virtual void Start() override;

    void SetupLiveStream(bool support, bool need_return_video, const char* rtmp_location) override;

    void SetupRecordStream();
    void AddParticipant(TransportPlugin* plugin) override;
    void OnPacketLoss(int stream_id, int percent);
    void OnAudioMixed(const char* outBuffer, int size) override;
    void OnAudioMixed(const std::shared_ptr<MediaOutputPacket> packet) override;
    bool RemoveParticipant(TransportPlugin* plugin) override;
    void OnKeyFrameNeeded() override;

    void IncomingRtpPacket(const int stream_id, const dataPacket& packet) override;

    bool MuteStream(const int stream_id,const bool mute) override;
    void SendFirPacketToViewer();

    // ISpeakerChangeListener - when the speaker has been changed.
    void OnSpeakerChanged(int stream_id) override {
      if(video_forward_element_) {
        video_forward_element_->ChangeToSpeaker(stream_id);
      }
    }

    bool SwitchPublisher(int stream_id);
    /**
     * Video forward element's event callback.
     */
    void OnRelayRtcpPacket(int stream_id, const dataPacket& packet) override;
    void OnRelayRtpPacket(int stream_id, const std::shared_ptr<MediaOutputPacket> packet) override;

    void OnTransportStateChange(const int stream_id, const TransportState state) override;
  private:
    bool Init();
    void Release();

    const int32_t session_id_;

    bool has_audio_;
    bool has_video_;

    bool running_;
    bool need_return_video_;

    int live_stream_id_ = -1;
    bool use_webcast_ = false;
    // The audio and video element for the room. Owns by the class.
    std::unique_ptr<AbstractAudioMixerElement> audio_mixer_element_;
    std::unique_ptr<AbstractVideoForwardElement> video_forward_element_;

    // THe other pipelines for outputing the media data to recorder or live_stream.
    StreamRecorderElement *stream_recorder_element_ = NULL;
    LiveStreamProcessor *live_stream_processor_ = NULL;
    // --------------------------------------------------------------------------

    // The frame count used for requesting Key frame.
    int all_frame_count_ = 0;
  };

}  // namespace orbit

#endif  // AUDIO_BRIDGE_PLUGIN_H__
