/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * audio_conference_plugin.cc
 * ---------------------------------------------------------------------------
 * Implements a Audio conference plugin system;
 * ---------------------------------------------------------------------------
 */

#include "audio_conference_plugin.h"

#include <sys/prctl.h>
#include <boost/thread/mutex.hpp>

#include "stream_service/orbit/audio_processing/audio_energy.h"
#include "stream_service/orbit/webrtc/webrtc_format_util.h"
#include "stream_service/orbit/rtp/janus_rtcp_processor.h"

#include "stream_service/orbit/base/timeutil.h"

#include "gflags/gflags.h"

DEFINE_bool(audio_conference_use_record_stream, false,
            "If set, it will have the recording stream function to enable.");

DEFINE_string(debug_setup_live_stream, "",
              "In debug mode, setup the live stream URL for debugging mode.");

DEFINE_bool(video_forward_element_new, true,
            "If set, VideoForwardElementNew will be used."
            "If not set, VideoForwardElement will be used.");

namespace orbit {
using namespace std;

const int kFirstKeyFrameCount = 6;

namespace {

  void PrintParsedPayload(const webrtc::RtpDepacketizer::ParsedPayload& payload) {
    // VideoHeader
    LOG(INFO) << " width=" << payload.type.Video.width
              << " height=" << payload.type.Video.height
              << " isFirstPacket=" << payload.type.Video.isFirstPacket;

    LOG(INFO) << " partId=" << payload.type.Video.codecHeader.VP8.partitionId
              << " pictureId=" << payload.type.Video.codecHeader.VP8.pictureId
              << " beginningOfPartition=" << payload.type.Video.codecHeader.VP8.beginningOfPartition;
    LOG(INFO) << "payload=" << payload.type.Video.codec;
    LOG(INFO) << "frametype=" << payload.frame_type;
    string frame_type = "Unknown";
    switch (payload.frame_type) {
    case webrtc::kEmptyFrame: {
      frame_type = "kEmptyFrame";
      break;
    }
    case webrtc::kAudioFrameSpeech: {
      frame_type = "kAudioFrameSpeech";
      break;
    }
    case webrtc::kAudioFrameCN: {
      frame_type = "kAudioFrameCN";
      break;
    }
    case webrtc::kVideoFrameKey: {
      frame_type = "kVideoFrameKey";
      break;
    }
    case webrtc::kVideoFrameDelta: {
      frame_type = "kVideoFrameDelta";
      break;
    }
    }
    LOG(INFO) << "frame_type=" << frame_type;
  }
  }  // annoymous namespace

  AudioConferencePlugin::AudioConferencePlugin(std::weak_ptr<Room> room,
                                               int stream_id) {
      LOG(INFO) <<"Init audio conference plugin";
      room_ = room;
      stream_id_ = stream_id;
  }

  void AudioConferencePlugin::RelayMediaOutputPacket(const std::shared_ptr<MediaOutputPacket> packet,
                                                     packetType packet_type) {
    if (packet->encoded_buf == NULL) {
      LOG(ERROR) << "OutputPacket is invalid.";
      return;
    }

    if (!active()) {
      return;
    }

    dataPacket p;
    p.comp = 0;
    p.type = packet_type;

    RtpHeader rtp_header;
    rtp_header.setTimestamp(packet->timestamp);
    rtp_header.setSeqNumber(packet->seq_number);
    rtp_header.setSSRC(packet->ssrc);
    rtp_header.setMarker(packet->end_frame);
    if (packet_type == VIDEO_PACKET) {
      if (video_payload_ == 0) {
        video_payload_ = VP8_90000_PT;
      }
      rtp_header.setPayloadType(video_payload_);
    } else {
      rtp_header.setPayloadType(OPUS_48000_PT);
    }

    assert(packet->length < (1500 - RTP_HEADER_BASE_SIZE));

    memcpy(&(p.data[0]), &rtp_header, RTP_HEADER_BASE_SIZE);
    memcpy(&(p.data[RTP_HEADER_BASE_SIZE]), packet->encoded_buf, packet->length);
    p.length = packet->length + RTP_HEADER_BASE_SIZE;

    VLOG(4) << "stream " << stream_id() << "'s energy:" << packet->audio_energy;

    // Call the gateway to relay the RTP packet to the participant.
    RelayRtpPacket(p);
    VLOG(4) << "relay to gateway succeed...";
  }

  void AudioConferencePlugin::IncomingRtpPacket(const dataPacket& packet) {
    if (!active()) {
      return;
    }
    TransportPlugin::IncomingRtpPacket(packet);
    VLOG(3) <<"Incoming RTP packet : Type = "<<packet.type;
    auto audio_room = room_.lock();
    if (audio_room.get() != NULL) {
      audio_room->IncomingRtpPacket(stream_id(), packet);
    }
    if (packet.type == VIDEO_PACKET) {
      // Get the payload of the packet. Update the video payload.
      const RtpHeader* h = reinterpret_cast<const RtpHeader*>(packet.data);
      //video_payload_ = VP8_90000_PT;
      video_payload_ = (int)(h->getPayloadType());
    }
  }

  void AudioConferencePlugin::OnAudioMixed(const std::shared_ptr<MediaOutputPacket> packet){
    if (!active()) {
      return;
    }
    RelayMediaOutputPacket(packet, AUDIO_PACKET);
  }

  void AudioConferencePlugin::IncomingRtcpPacket(const dataPacket& packet) {
    TransportPlugin::IncomingRtcpPacket(packet);
    //RelayRtcpPacket(packet);
    if (packet.type == AUDIO_PACKET) {
      // Skip for now
    } else if (packet.type == VIDEO_PACKET) {
      const unsigned char* buf1 = reinterpret_cast<const unsigned char*>(packet.data);
      unsigned char* buf2 = const_cast<unsigned char*>(buf1);
      char* buf = reinterpret_cast<char*>(buf2);
      int len = packet.length;
      if(janus_rtcp_has_fir(buf, len) || janus_rtcp_has_pli(buf, len)) {
       /* We got a FIR, forward it to the publisher */
        auto audio_room = room_.lock();
        if (audio_room.get() != NULL) {
          ((AudioConferenceRoom*)audio_room.get())->SendFirPacketToViewer();
        }
      }
      uint64_t bitrate = janus_rtcp_get_remb(buf, len);
      if(bitrate > 0) {
        /* FIXME We got a REMB from this listener, should we do something about it? */
        VLOG(3) << "Got REMB packet... bitrate=" << bitrate;
        //((AudioConferenceRoom*)room_)->SendRembPacketToViewer();
      }
    }
  }

  void AudioConferencePlugin::OnTransportStateChange(TransportState state) {
    auto audio_room = room_.lock();
    if (audio_room && audio_room.get() != NULL) {
      ((AudioConferenceRoom*)audio_room.get())->OnTransportStateChange(stream_id(), state);
    }
  }

  // --------------------------------------AudioConferenceRoom--------------------
  AudioConferenceRoom::AudioConferenceRoom(int32_t session_id,
                                           bool use_webcast = false,
                                           bool support_video = true)
    : session_id_(session_id) {
    running_ = false;
    need_return_video_ = true;
    use_webcast_ = use_webcast;
    VLOG(google::INFO) << "Init audio conference room";
    
    // It will always have audio input, but need to specify if there is video
    // stream as input.
    has_audio_ = true;
    has_video_ = support_video;
  }

  AudioConferenceRoom::~AudioConferenceRoom() {
    running_ = false;
    Destroy();
  }

  void AudioConferenceRoom::SendFirPacketToViewer() {
    if(video_forward_element_) {
      video_forward_element_->RequestSpeakerKeyFrame();
    }
  }

  void AudioConferenceRoom::Create() {
    if (!Init()) {
      LOG(ERROR) << "Create the audioRoom, init failed.";
    }
  }

  void AudioConferenceRoom::SetupLiveStream(bool support,
                                            bool need_return_video,
                                            const char* live_location) {
    VLOG(2)<<"=========================================";
    VLOG(2)<<"AudioConferenceRoom :: StartLiveStream";
    use_webcast_ = support;
    need_return_video_ = need_return_video;
    if(support) {
      if (live_stream_processor_ == NULL) {
        live_stream_processor_ = new LiveStreamProcessor(true, has_video_);
       }
      if(!live_stream_processor_->IsStarted()){
        live_stream_processor_->Start(live_location);
        audio_mixer_element_->SetMixAllRtpPacketListener(this);
      }
    }
  }

  void AudioConferenceRoom::SetupRecordStream() {
    if(stream_recorder_element_ == NULL) {
      stream_recorder_element_ = new StreamRecorderElement("");
      audio_mixer_element_->SetMixAllRtpPacketListener(this);
    }
  }

  void AudioConferenceRoom::AddParticipant(TransportPlugin *plugin){
    Room::AddParticipant(plugin);
    AudioConferencePlugin* audio_conference_plugin = (AudioConferencePlugin*)plugin;
    int stream_id = audio_conference_plugin->stream_id();
    audio_mixer_element_->AddAudioBuffer(stream_id);

    audio_mixer_element_->RegisterMixResultListener(stream_id,
        dynamic_cast<IAudioMixerRtpPacketListener*> (audio_conference_plugin));

  }

  void AudioConferenceRoom::Destroy() {
    LOG(INFO)<<"AudioConferenceRoom destroy";
    running_ = false;

   if(live_stream_processor_ != NULL) {
      delete live_stream_processor_;
      live_stream_processor_ = NULL;
    }
    if(stream_recorder_element_) {
      delete stream_recorder_element_;
      stream_recorder_element_ = NULL;
    }
  }

  bool AudioConferenceRoom::Init() {
    audio_mixer_element_.reset(
        AudioMixerFactory::Make(session_id_,
                                new AudioOption(),
                                this, // IAudioMixerRawListener
                                this  // ISpeakerChangeListener
        ));

    if(has_video_) {
      if (FLAGS_video_forward_element_new) {
        video_forward_element_.reset(new VideoForwardElementNew(this));
      } else {
        video_forward_element_.reset(new VideoForwardElement(this));
      }
    }
    if (FLAGS_audio_conference_use_record_stream) {
      SetupRecordStream();
    }

    if (!FLAGS_debug_setup_live_stream.empty()) {
      SetupLiveStream(true, true, FLAGS_debug_setup_live_stream.c_str());
    }

    return true;
  }

  void AudioConferenceRoom::Start() {
    running_ = true;
  }

  bool AudioConferenceRoom::MuteStream(int stream_id, bool mute){
    audio_mixer_element_->Mute(stream_id, mute);
    return true;
  }

  void AudioConferenceRoom::OnKeyFrameNeeded() {
    SendFirPacketToViewer();
  }

  void AudioConferenceRoom::OnTransportStateChange(int stream_id, TransportState state) {
    if(state == TRANSPORT_READY) {
      /*
       * Start audio mixer loop only when at least one user joined.
       */
      if(audio_mixer_element_ && !audio_mixer_element_->IsStarted()){
        audio_mixer_element_->Start();
      }
      /*
       * Link video stream when participant's ice is connected.
       */
      if(video_forward_element_) {
        video_forward_element_->LinkVideoStream(stream_id);
      }
    }
  }
  bool AudioConferenceRoom::RemoveParticipant(TransportPlugin* plugin) {
   {
      boost::mutex::scoped_lock lock(room_plugin_mutex_);
      Room::RemoveParticipant(plugin);
    }
    std::vector<TransportPlugin*> participants = GetParticipants();

    AudioConferencePlugin* audio_conference_plugin = (AudioConferencePlugin*)plugin;

    audio_mixer_element_->RemoveAudioBuffer(audio_conference_plugin->stream_id());
    audio_mixer_element_->UnregisterMixResultListener(audio_conference_plugin->stream_id());

    if(video_forward_element_) {
      video_forward_element_->UnlinkVideoStream(audio_conference_plugin->stream_id());
    }
    return true;
  }

  bool AudioConferenceRoom::SwitchPublisher(int stream_id){
    if(video_forward_element_) {
      video_forward_element_->ChangeToSpeaker(stream_id);
    }
    return true;
  }

  void AudioConferenceRoom::OnRelayRtcpPacket(int stream_id, const dataPacket& packet) {
    boost::mutex::scoped_lock lock(room_plugin_mutex_);
    std::vector<TransportPlugin*> participants = GetParticipants();
    for (vector<TransportPlugin*>::iterator it = participants.begin(); it != participants.end(); ++it) {
      TransportPlugin* transport_plugin = *(it);
      AudioConferencePlugin* audio_conference_plugin = (AudioConferencePlugin*)transport_plugin;
      if(stream_id == audio_conference_plugin->stream_id()){
        audio_conference_plugin->RelayRtcpPacket(packet);
        break;
      }
    }
  }

  void AudioConferenceRoom::OnRelayRtpPacket(int stream_id, const std::shared_ptr<MediaOutputPacket> packet) {
    //TODO(QingyongZhang) change packet to shared_ptr and remove free of it in every usage.
    boost::mutex::scoped_lock lock(room_plugin_mutex_);
    std::vector<TransportPlugin*> participants = GetParticipants();
    if(stream_id == 0) {
//      if(use_webcast_ && live_stream_processor_ && live_stream_processor_->IsStarted()){
//        live_stream_processor_->RelayRtpVideoPacket(packet);
//        if (packet->end_frame) {
//          all_frame_count_++;
          // HACK(chengxu): since the first key frame may be corrupted
          // in windows chrome browser (bug# ...). We have to request
          // keyframe after first few frame received.
//          if (video_forward_element_ && all_frame_count_ == kFirstKeyFrameCount) {
//            video_forward_element_->RequestSpeakerKeyFrame();
//          }
//        }
//      }
      if(stream_recorder_element_) {
        stream_recorder_element_->RelayMediaOutputPacket(packet, VIDEO_PACKET);
      }
    }
    for (vector<TransportPlugin*>::iterator it = participants.begin(); it != participants.end(); ++it) {
        TransportPlugin* transport_plugin =  *(it);
        AudioConferencePlugin* audio_conference_plugin = (AudioConferencePlugin*)transport_plugin;
        if(audio_conference_plugin->stream_id()== stream_id) {
          audio_conference_plugin->RelayMediaOutputPacket(packet,VIDEO_PACKET);
        }
    }
  }

  void AudioConferenceRoom::IncomingRtpPacket(const int stream_id, const dataPacket& packet) {
    if (packet.type == AUDIO_PACKET) {
      audio_mixer_element_->PushPacket(stream_id, packet);
    } else if (packet.type == VIDEO_PACKET) {
      if(video_forward_element_){
        video_forward_element_->OnIncomingPacket(stream_id, packet);
      }
    }

   // HACK(chengxu): We forward the incoming RTP packet directly to live_stream_processor
    // This is not the final solution - ideally we should use the forwarded result to
    // the live stream pipeline. But right now we have to do this for lip synchronization
    // problem. We should fix this later.
    if(use_webcast_ && live_stream_processor_ && live_stream_processor_->IsStarted()) {
      if (live_stream_id_ == -1) {
        live_stream_id_ = stream_id;
      } else if (stream_id != live_stream_id_) {
        // Only forward the RTP packets for the first participant. First stream_id.
        LOG(ERROR) << "stream_id is not match.";
        return;
      }

      const RtpHeader* h = reinterpret_cast<const RtpHeader*>(packet.data);
      if (h->getMarker()) {
          all_frame_count_++;
         // HACK(chengxu): since the first key frame may be corrupted
         // in windows chrome browser (bug# ...). We have to request
         // keyframe after first few frame received.
          if (video_forward_element_ && all_frame_count_ == kFirstKeyFrameCount) {
            video_forward_element_->RequestSpeakerKeyFrame();
          }
       }
      if (packet.type == AUDIO_PACKET) {
//        LOG(INFO) << "Relay AUDIO_PACKET";
        live_stream_processor_->RelayRtpAudioPacket(packet);
      }
      if (packet.type == VIDEO_PACKET) {
//        LOG(INFO) << "Relay VIDEO_PACKET";
        live_stream_processor_->RelayRtpVideoPacket(packet);
      }
    }
  }

  void AudioConferenceRoom::OnPacketLoss(int stream_id, int percent){
    if(audio_mixer_element_ != NULL) {
      audio_mixer_element_->OnPacketLoss(stream_id, percent);
    }
  }

  void AudioConferenceRoom::OnAudioMixed(const char* outBuffer, int size){
    if(use_webcast_ && live_stream_processor_ && live_stream_processor_->IsStarted()){
      //live_stream_processor_->RelayRawAudioPacket(outBuffer, size);
    }
  }
  void AudioConferenceRoom::OnAudioMixed(const std::shared_ptr<MediaOutputPacket> packet){
    if(stream_recorder_element_) {
      stream_recorder_element_->RelayMediaOutputPacket(packet, AUDIO_PACKET);
    }
//    if(use_webcast_ && live_stream_processor_ && live_stream_processor_->IsStarted()){
//      live_stream_processor_->RelayRtpAudioPacket(packet);
//    }
  }
}  // namespace orbit
