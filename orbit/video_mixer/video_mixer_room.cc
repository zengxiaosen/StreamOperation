/*
 * video_mixer_plugin.cc
 *
 *  Created on: Mar 4, 2016
 *      Author: vellee
 */
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <glib-2.0/glib.h>
#include <sys/prctl.h>

#include "stream_service/orbit/video_mixer/video_mixer_plugin.h"
#include "stream_service/orbit/video_mixer/video_mixer_room.h"
#include "stream_service/orbit/transport_plugin.h"

#include "stream_service/orbit/live_stream/live_stream_processor_impl.h"
#include "stream_service/orbit/live_stream/live_stream_processor.h"
#include "stream_service/orbit/video_mixer/video_mixer_position_manager.h"
#include "stream_service/orbit/audio_processing/audio_energy.h"
#include "stream_service/orbit/rtp/rtp_packet_queue.h"
#include "stream_service/orbit/rtp/rtp_headers.h"

#include "glog/logging.h"
#include "gflags/gflags.h"

#include <vector>

#define VLOG_LEVEL 2
DEFINE_int32(mtu, 1100,
             "If set, it will change mixer rtp packet size.");
DEFINE_bool(video_mixer_use_record_stream, false,
            "If set, it will have the recording stream function to enable.");
DECLARE_bool(save_dot_file);

namespace orbit {
using namespace std;
namespace {
  static gboolean video_mixer_bus_message (GstBus * bus, GstMessage * message, gpointer pipeline) {
    VLOG(2)<<"Got message"<<GST_MESSAGE_TYPE (message)<<"=========================================================================================================";
    GST_DEBUG ("got message %s",
               gst_message_type_get_name (GST_MESSAGE_TYPE (message)));
    switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR: {
      GError *err = NULL;
      gchar *dbg_info = NULL;
      gst_message_parse_error (message, &err, &dbg_info);
      g_printerr ("ERROR from element %s: %s\n",
                  GST_OBJECT_NAME (message->src), err->message);
      g_printerr ("Debugging info: %s\n", (dbg_info) ? dbg_info : "none");
      
      if(FLAGS_save_dot_file) {
        GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
                                           GST_DEBUG_GRAPH_SHOW_VERBOSE, "error");
      }
      g_error_free (err);
      g_free (dbg_info);
      break;
    }
    case GST_MESSAGE_EOS:
      break;
    default:
      break;
    }
    return TRUE;
  }

  static int GetMtu(){
    int flag_mtu = FLAGS_mtu;
    if(flag_mtu<500 || flag_mtu>1400){
      LOG(ERROR)<<"MTU can onley between 500 and 1400,mtu will be seted to 1100";
      flag_mtu = 1100;
    }
    return flag_mtu;
  }
}  // Annoymous namespace

  void VideoMixerRoom::Start() {
    running_ = true;
    pull_sink_thread_.reset(new boost::thread(boost::bind(&VideoMixerRoom::PullAppSinkData, this)));

    if (FLAGS_video_mixer_use_record_stream) {
      SetupRecordStream();
    }

  }

  void VideoMixerRoom::OnTransportStateChange(int stream_id, TransportState state) {
    if(state == TRANSPORT_READY) {
      /*
       * Start audio mixer loop only when at least one user joined.
       */
      if(audio_mixer_element_ && !audio_mixer_element_->IsStarted()){
        audio_mixer_element_->Start();
      }
    }
  }


  void VideoMixerRoom::OnPositionUpdated(){
    gst_bus_poll (GST_ELEMENT_BUS (pipeline_), GST_MESSAGE_ERROR,100 * GST_MSECOND);
  }

  void VideoMixerRoom::SetupLiveStream(bool support, bool need_return_video,
                                       const char* live_location) {
    VLOG(2)<<"=========================================";
    VLOG(2)<<"VideoMixerRoom :: StartLiveStream";
    if(support) {
      if(live_stream_processor_ == NULL) {
        live_stream_processor_ = new LiveStreamProcessor(pipeline_,tee_);
      }

      if(!live_stream_processor_->IsStarted()){
        live_stream_processor_->Start(live_location);
      }
    }
  }

  void VideoMixerRoom::SetupRecordStream() {
    if(stream_recorder_element_ == NULL) {
      stream_recorder_element_ = new StreamRecorderElement("");
      audio_mixer_element_->SetMixAllRtpPacketListener(this);
    }
  }
  /**
   *-------------------------------------------------------------------------------------
   *VideoMixerRoom functions
   *-------------------------------------------------------------------------------------
   */
  VideoMixerRoom::VideoMixerRoom(int32_t session_id, bool live_stream)
    : session_id_(session_id) {
    position_manager_ = new VideoMixerPositionManager(this);
    use_live_stream_ = live_stream;
    running_ = false;

    VLOG(google::FATAL)<<"Init audio conference room";
    pipeline_ = gst_pipeline_new( "video_mixer_room_pipeline" );
    GstClock* clock = gst_system_clock_obtain();
    gst_pipeline_use_clock(GST_PIPELINE(pipeline_), clock);

    app_sink_ = gst_element_factory_make("appsink","app_sink");
    caps_filter_ = gst_element_factory_make("capsfilter","Caps");
    g_object_set (app_sink_, "emit-signals", TRUE, NULL);
    gst_app_sink_set_max_buffers((GstAppSink*)app_sink_, 1);
    gst_app_sink_set_emit_signals((GstAppSink*)app_sink_, true);
    g_object_set(GST_OBJECT(app_sink_),"drop",false,"async",false,"sync",false, "blocksize",1400,NULL);

//    g_signal_connect( G_OBJECT(_sink), "new-buffer", G_CALLBACK(onNewBuffer), this);

//    GstAppSinkCallbacks* appsink_callbacks = (GstAppSinkCallbacks*)malloc(sizeof(GstAppSinkCallbacks));
//    appsink_callbacks->eos = NULL;
//    appsink_callbacks->new_preroll = on_new_sample;
//    appsink_callbacks->new_sample = on_new_sample;
//    gst_app_sink_set_callbacks(GST_APP_SINK(app_sink_), appsink_callbacks, this, NULL);

    video_mixer_ = gst_element_factory_make("compositor","video_mixer");
    g_object_set(GST_OBJECT(video_mixer_), "background" , 1, "latency", 1000,"start-time-selection",0,NULL);
    tee_ = gst_element_factory_make("tee","tee");
    vp8enc_ = gst_element_factory_make("vp8enc","vp8enc");

    g_object_set (G_OBJECT (vp8enc_),
        "deadline", G_GINT64_CONSTANT (100000),
        "threads", 16,
        "cpu-used", 8,
        "resize-allowed", TRUE,
        "keyframe-max-dist", 80,
        "cq-level", 7,
        "target-bitrate", 300000,
        "quality", 5.0,
        "error-resilient", true,
        "end-usage", 0,
        "min-quantizer",2,
        "overshoot",15,
        "buffer-initial-size",500,
        "buffer-optimal-size",600,
        NULL);
    rtp_pay_ = gst_element_factory_make("rtpvp8pay","vp8_pay");
    g_object_set(GST_OBJECT(rtp_pay_), "ssrc" , 55543, "pt", 100, "mtu", GetMtu(), NULL);
    gst_bin_add_many(GST_BIN(pipeline_),video_mixer_, caps_filter_,tee_, vp8enc_, rtp_pay_, app_sink_, NULL);
    gst_element_link_many(video_mixer_, caps_filter_, tee_,vp8enc_ ,rtp_pay_, app_sink_,NULL);
    VLOG(VLOG_LEVEL)<<"VideoMixer :: Start app sink thread";

    VLOG(VLOG_LEVEL)<<"VideoMixer :: Start app sink thread succeed";
    GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
    gst_bus_add_signal_watch(bus);
    g_signal_connect(bus, "message", G_CALLBACK (video_mixer_bus_message), this);
    gst_object_unref(bus);

    audio_mixer_element_.reset(AudioMixerFactory::Make(session_id_,
                                                       new AudioOption(),
                                                       this,
                                                       NULL));
  }

  void VideoMixerRoom::SyncElements() {
    gst_element_sync_state_with_parent(video_mixer_);
    gst_element_sync_state_with_parent(tee_);
    gst_element_sync_state_with_parent(vp8enc_);
    gst_element_sync_state_with_parent(rtp_pay_);
    gst_element_sync_state_with_parent(app_sink_);
    gst_element_sync_state_with_parent(caps_filter_);
  }

  void VideoMixerRoom::AddParticipant(TransportPlugin* plugin) {
   VLOG(2)<<"__________________AddParticipant__________________________________";

    Room::AddParticipant(plugin);
    position_manager_->AddStream((VideoMixerPluginImpl*)plugin);
    Rect rect = position_manager_->GetOutVideoSize();
    VLOG(2)<<"SetCaps: width = "<<rect.width<<" height="<<rect.height;
    GstCaps *caps = gst_caps_new_simple("video/x-raw", "media", G_TYPE_STRING, "video", "width",G_TYPE_INT,rect.width,
                                        "height", G_TYPE_INT, rect.height, NULL);
    g_object_set(G_OBJECT(caps_filter_), "caps", caps, NULL);
    gst_caps_unref(caps);
    position_manager_->UpdateVideoPosition();
    if(FLAGS_save_dot_file) {
    GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline_), GST_DEBUG_GRAPH_SHOW_VERBOSE, "addparticipent");
    }
    //Add audio buffer manager to audio mixer element.
    VideoMixerPlugin* mixer_plugin = (VideoMixerPlugin*)plugin;
    audio_mixer_element_->AddAudioBuffer(mixer_plugin->stream_id());
    audio_mixer_element_->RegisterMixResultListener(mixer_plugin->stream_id(), dynamic_cast<IAudioMixerRtpPacketListener*>(mixer_plugin));
  }

  bool VideoMixerRoom::RemoveParticipant(TransportPlugin* plugin) {
    VLOG(2)<<"__________________RemoveParticipant__________________________________";
    VideoMixerPluginImpl* mixer_plugin = (VideoMixerPluginImpl*)plugin;
    audio_mixer_element_->RemoveAudioBuffer(mixer_plugin->stream_id());
    audio_mixer_element_->UnregisterMixResultListener(mixer_plugin->stream_id());
    {
      boost::mutex::scoped_lock lock(room_plugin_mutex_);
      Room::RemoveParticipant(plugin);
    }
    position_manager_->RemoveStream((VideoMixerPluginImpl*)plugin);
    mixer_plugin->UnlinkElements();
    if(FLAGS_save_dot_file) {
    GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline_), GST_DEBUG_GRAPH_SHOW_VERBOSE, "RemoveParticipant");
    }
    return true;
  }
  void VideoMixerRoom::PullAppSinkData() {
    /* Set thread name */
    prctl(PR_SET_NAME, (unsigned long)"PullAppSinkData");

    GstSample *sample;
    GstBuffer*buffer;
    GstMapInfo info;
    guint size;
    uint32_t time_offset;
    uint32_t seq_offset;
//    while(!gst_app_sink_is_eos(GST_APP_SINK(app_sink_))){
    while(running_){
      g_signal_emit_by_name (app_sink_, "pull-sample", &sample, NULL);
      if(!running_){
        VLOG(2)<<"Receive eos from running";
        return;
      }
      if (sample) {
        buffer = gst_sample_get_buffer (sample);
        if (buffer == NULL) {
          gst_sample_unref (sample);
          LOG(ERROR) << "No buffer got from sample : GST_FLOW_ERROR";
          usleep(2000);
          continue;
        }
        if (!gst_buffer_map (buffer, &info, GST_MAP_READ)) {
          gst_sample_unref (sample);
          LOG(ERROR) << "Can not read buffer : GST_FLOW_ERROR";
          usleep(2000);
          continue;
        }
        size = info.size;
        int i = 0;
        for (auto _p : plugins_) {
          g_object_get(G_OBJECT(rtp_pay_),"seqnum",&seq_offset,"timestamp",&time_offset,NULL);
          VideoMixerPluginImpl* plugin = reinterpret_cast<VideoMixerPluginImpl*>(_p);
          VLOG(2)<<"\n-------------- length is "<<size
              <<" TimeStamp is "<<time_offset
              <<" SeqOffset is "<<seq_offset;
          dataPacket p;
          p.comp = 0;
          p.type = VIDEO_PACKET;
          memcpy(&(p.data[0]), info.data, size);
          p.length = size;
          plugin->RelayRtpPacket(p);

          if(i==0 && stream_recorder_element_) {
            const RtpHeader* h = reinterpret_cast<const RtpHeader*>(info.data);
            std::shared_ptr<MediaOutputPacket> webcast_pkt = std::make_shared<MediaOutputPacket>();
            webcast_pkt->timestamp = h->getTimestamp();
            webcast_pkt->seq_number = h->getSeqNumber();
            webcast_pkt->end_frame = h->getMarker();
            webcast_pkt->ssrc = h->getSSRC();
            unsigned char* tmp_buf = (unsigned char*)malloc(size-12);

            const unsigned char* data_buf = reinterpret_cast<const unsigned char*>(&(info.data[0]));

            memcpy(tmp_buf, data_buf+12, size-12);
            webcast_pkt->encoded_buf = tmp_buf;
            webcast_pkt->length = size-12;
            stream_recorder_element_->RelayMediaOutputPacket(webcast_pkt, VIDEO_PACKET);
          }
          i++;
        }
        gst_buffer_unmap (buffer, &info);
        gst_sample_unref (sample);
      } else {
        usleep(2000);
        continue;
      }
    }
    VLOG(VLOG_LEVEL)<<"Found it stoped!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!";
  }

  void VideoMixerRoom::Destroy() {
    LOG(INFO)<<"Destroy room "<<running_;
    running_ = false;
    LOG(INFO)<<"Destroy room ";
    GstPad* sinkpad = gst_element_get_static_pad (app_sink_, "sink");
    gst_pad_send_event (sinkpad, gst_event_new_eos ());
    gst_object_unref(sinkpad);
    if (pull_sink_thread_.get()!=NULL) {
      VLOG(3) << "Joining Pull_sink thread";
      pull_sink_thread_->join();
      VLOG(3) << "Thread terminated on destructor";
    }
    if(position_manager_ != NULL){
      delete position_manager_;
    }

    if(live_stream_processor_ != NULL){
      VLOG(3) << "destroy live stream processor";
      delete live_stream_processor_;
      live_stream_processor_ = NULL;
    } else {
      VLOG(3) << "live_stream_processor is null;";
    }

   if(stream_recorder_element_) {
//      delete stream_recorder_element_;
//      stream_recorder_element_ = NULL;
    }

    if(pipeline_ != NULL) {
      gst_element_set_state(GST_ELEMENT(pipeline_), GST_STATE_NULL);
      gst_object_unref (pipeline_);
    }
  }
  void VideoMixerRoom::OnPluginRemoved(int stream_id){
    int size = GetParticipantCount();
    if(size == 0){
      return;
    }
    Rect rect = position_manager_->GetOutVideoSize();
    VLOG(2)<<"SetCaps: width = "<<rect.width<<" height="<<rect.height;
    GstCaps *caps = gst_caps_new_simple("video/x-raw", "media", G_TYPE_STRING, "video", "width",G_TYPE_INT,rect.width,
                                        "height", G_TYPE_INT, rect.height, NULL);
    g_object_set(G_OBJECT(caps_filter_), "caps", caps, NULL);
    gst_caps_unref(caps);
    position_manager_->UpdateVideoPosition();
  }

  void VideoMixerRoom::IncomingRtpPacket(const int stream_id, const dataPacket& packet) {
    if(audio_mixer_element_) {
      audio_mixer_element_->PushPacket(stream_id, packet);
    }
  }

  void VideoMixerRoom::OnPacketLoss(int stream_id, int percent) {
    if(audio_mixer_element_) {
      audio_mixer_element_->OnPacketLoss(stream_id, percent);
    }
  }

  bool VideoMixerRoom::MuteStream(const int stream_id,const bool mute){
    bool success = false;
    if(audio_mixer_element_){
      success = audio_mixer_element_->Mute(stream_id, mute);
    }
    return success;
  }
  void VideoMixerRoom::OnAudioMixed(const char* outBuffer, int size){
    if(live_stream_processor_){
      live_stream_processor_->RelayRawAudioPacket(outBuffer, size);
    }
  }

  void VideoMixerRoom::OnAudioMixed(const std::shared_ptr<MediaOutputPacket> packet){
    if(stream_recorder_element_) {
      LOG(INFO) << "OnAudioMixed Relay seqNumber=" << packet->seq_number;
      stream_recorder_element_->RelayMediaOutputPacket(packet, AUDIO_PACKET);
    }
  }

  VideoMixerRoom::~VideoMixerRoom(){
    Destroy();
  }
}  // namespace orbit
