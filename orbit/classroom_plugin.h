/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * classroom_plugin.h
 * ---------------------------------------------------------------------------
 * Defines the interface to implement a class room plugin.
 * ---------------------------------------------------------------------------
 */

#ifndef CLASSROOM_PLUGIN_H__
#define CLASSROOM_PLUGIN_H__

#include "audio_conference_plugin.h"
#include "stream_service/proto/stream_service.pb.h"
#include "stream_service/orbit/replay_pipeline/time_recorder.h"

#include <vector>

#define CLASSROOM_STUDENT_NUMBER 5

namespace orbit {
  // The struct maintains all the data for a stream switch context.
  struct ClassRoomSsrcSwitchContext {
    // Ssrc of the context.
    unsigned int ssrc = 0;
    unsigned int source_ssrc = 0;

    /* RTP seq and ts */
    uint16_t seq = 0;
    uint32_t ts = 0;
    
    /* The context data of source stream */
    uint32_t last_ts = 0;
    uint32_t base_ts = -1;
    uint16_t last_seq = 0;
    uint16_t base_seq = -1;

    /* The source stream's stream id */
    int source_stream_id = 0;

    void RenewContext(unsigned int ss, unsigned int source_ss, int _source_stream_id) {
      base_ts = -1;
      base_seq = -1;
      ssrc = ss;
      source_ssrc = source_ss;
      source_stream_id = _source_stream_id;
    }
    
    std::string DebugString() {
      std::ostringstream oss;
      oss << "ssrc=" << ssrc << " seq=" << seq << " ts=" << ts
          << " last_ts=" << last_ts << " base_ts=" << base_ts
          << " last_seq=" << last_seq << " base_seq=" << base_seq
          << std::endl;
      return oss.str();
    }
  };

  // The ClassRoomPlugin forwards the video stream from other plugins
  // to the current plugin. The RelayRtpPacket is responsible for rewriting
  // the RTP header (SSRC, seq and ts). Since there may be more than one
  // incoming stream to be forwarded. The following functions are provided
  // to link the SSRC of the incoming video streams.
  // The function LinkToSsrc(ssrc) will notify the plugin that the incoming
  // stream will have the parameter 'ssrc' in their packet. And the plugin
  // will try to allocate one slot from extended_ssrc_vec_ and
  //  video_switch_contexts_ to provide a seat and map from the ssrc to
  // the local downstream ssrcs.
  class ClassRoomPlugin : public AudioConferencePlugin {
  public:
    enum PluginRole {
      TEACHER = 0,
      STUDENT = 1
    };

    ClassRoomPlugin(std::weak_ptr<Room> room, int stream_id);
    ~ClassRoomPlugin() {
    }
    void OnTransportStateChange(TransportState state) override;
    virtual void IncomingRtcpPacket(const dataPacket& packet) override;
    virtual void RelayRtpPacket(const dataPacket& packet) override;
    // IncomingRtpPacket() derived from AudioConferencePlugin
    // RelayRtcpPacket() derived from AudioConference/TransportPlugin

    // Must override this function, it specifies how many video SSRC
    // should be put in the SDP. The Endpoint will call this function
    // to decide the number of extended video ssrcs.
    virtual int GetExtendedVideoSsrcNumber() override{
      return CLASSROOM_STUDENT_NUMBER;
    }
    virtual void SetExtendedVideoSsrcs(
        std::vector<unsigned int> extended_video_ssrcs) override;

    // Link the plugin to the incoming stream's ssrc and stream_id
    void LinkToSsrc(unsigned int other_ssrc, int other_stream_id);
    // Unlink the plugin to ssrc, usually when the SSRC's has exited the room.
    void UnlinkToSsrc(unsigned int other_ssrc);

    // Send a FIR/PLI RTCP packet to request key frame of this plugin.
    void SendFirPacket();

    // Set the role of this plugin.
    void SetPluginRole(PluginRole role) {
      role_ = role;
    }
    PluginRole GetPluginRole() {
      return role_;
    }
    
    void GetStreamVideoMap(olive::StreamVideoMapResponse* stream_video_map);

  private:
    //void RewriteSsrc(dataPacket* p);
    void RewriteRtpHeader(dataPacket* p);
    PluginRole role_;

    std::mutex video_ssrc_mutex_;
    std::vector<std::pair<unsigned int, unsigned int> > extended_ssrc_vec_;
    map<int, std::shared_ptr<ClassRoomSsrcSwitchContext>> video_switch_contexts_;
    int key_frame_seq_ = 0;
  };

  // The ClassRoom is the room controller to mux the packets from all
  // participants to all each others. The room is responsible for controlling
  // the visibility of the participants (i.e. some people in the meeting may
  // not visible to others). It supports mux multiple video streams into one
  // transport channel (via different ssrcs).
  class ClassRoom : public Room,
                    public IAudioMixerRawListener,
                    public IAudioMixerRtpPacketListener,
                    public IAudioEventListener,
                    public ISpeakerChangeListener  {
  public:
    ClassRoom(int32_t session_id);
    ~ClassRoom();
    // The lifetime management of the Room.
    virtual void Create() override;
    virtual void Destroy() override;
    virtual void Start() override;
    // The participants management of the Room
    void AddParticipant(TransportPlugin* plugin) override;
    bool RemoveParticipant(TransportPlugin* plugin) override;

    // Audio mixer listener's callback
    void OnAudioMixed(const char* outBuffer, int size) override {
    }
    void OnAudioMixed(const std::shared_ptr<MediaOutputPacket> packet) override {
    }

    void IncomingRtpPacket(const int stream_id, const dataPacket& packet) override;

    bool MuteStream(const int stream_id,const bool mute) override {
      audio_mixer_element_->Mute(stream_id, mute);
      return true;
    }

    void OnTransportStateChange(const int stream_id, const TransportState state) override;
    // ISpeakerChangeListener - when the speaker has been changed.
    void OnSpeakerChanged(int stream_id) override {
    }
    
    // Handle the PUBLISH_STREAMS message in stream_service.proto
    // Make some of the streams publish.
    bool PublishStreams(const vector<int>& stream_ids) {
      std::lock_guard<std::mutex> p_lock(published_streams_mutex_);
      published_streams_.clear();
      RequestKeyFrameFromAllParticipants();
      for (auto stream_id : stream_ids) {
        published_streams_.insert(stream_id);
      }
    }
    // Handle the GET_STREAM_VIDEO_MAP message in stream_service.proto
    // Return the stream video's map to the client, the data structure
    // contains the mapping from the video stream id to the mslable and
    // sequence id in the SSRC section of the SDP.
    void GetStreamVideoMap(const int stream_id, 
                           olive::StreamVideoMapResponse* stream_video_map);

    // Request all the participants plugins to send keyframe in the upcoming
    // packets.
    void RequestKeyFrameFromAllParticipants();

    // Enable the capabilities of the plugin/classroom to store the RTP packets.
    void EnableCapturePackets(bool enable);
  private:
    bool Init();
    void Release();

    void LinkSsrcsToParticipants(int stream_id);
    // MuxRtpPacket - forward the RTP packet to all the other participants.
    void MuxRtpPacket(const int stream_id, const dataPacket& packet);

    std::mutex classroom_plugins_mutex_;
    std::mutex published_streams_mutex_;
    std::set<int> published_streams_;
    std::set<int> master_streams_;
    const int32_t session_id_;
    bool running_;

    // Indicates if the room should capture the packets into a file.
    bool enable_capture_packets_ = false;
    // RtpCapture to capture the media RTP packets.
    RtpCapture* rtp_capture_ = NULL; // The RTP capture module.
    TimeRecorder* time_recorder_ = NULL;

    // The audio mixer element for the room. Owns by the class.
    std::unique_ptr<AbstractAudioMixerElement> audio_mixer_element_;
    // The video mux logic is implemented in the calss. In the future, it
    // should be refactored to a standalone class.
  };

}  // namespace orbit

#endif  // CLASSROOM_PLUGIN_H__
