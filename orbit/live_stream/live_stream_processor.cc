/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: qingyong@orangelab.cn (Qingyong Zhang)
 *
 * webcast_impl.h
 * ---------------------------------------------------------------------------
 * Implements a gstreamer-based webcast module;
 * ---------------------------------------------------------------------------
 */

#include "stream_service/orbit/live_stream/live_stream_processor_impl.h"
#include "stream_service/orbit/live_stream/live_stream_processor.h"
#include "stream_service/orbit/media_definitions.h"
#include "stream_service/orbit/modules/media_packet.h"
#include "stream_service/orbit/rtp/rtp_format_util.h"
#include "gflags/gflags.h"
#include "gst/gst.h"
#include "gst/app/gstappsrc.h"

DEFINE_string(live_location, "rtmp://olive2.intviu.cn/live/33", "specify the rtmp push location.");
DEFINE_bool(save_dot_file, false, "Used to determine  whether  need to save pipeline to dot file. Default is false.");
DECLARE_bool(use_push_mode);
#define JITTER_TIME 1000 //1000ms
#define VLOG_LEVEL 2

namespace orbit {

using namespace std;
namespace {
  static gboolean bus_message (GstBus * bus, GstMessage * message, gpointer pipe) {
    VLOG(VLOG_LEVEL)<<"Got message"<<"=========================================";
    GST_DEBUG("got message %s", gst_message_type_get_name(GST_MESSAGE_TYPE(message)));

    switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR: {
      GError *err = NULL;
      gchar *dbg_info = NULL;

      gst_message_parse_error (message, &err, &dbg_info);
      g_printerr("ERROR from element %s: %s\n", GST_OBJECT_NAME(message->src), err->message);
      g_printerr ("Debugging info: %s\n", (dbg_info) ? dbg_info : "none");

      if(FLAGS_save_dot_file) {
        GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN (pipe), GST_DEBUG_GRAPH_SHOW_ALL, "error");
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

  static void enough_data(GstAppSrc * src, guint arg1, gpointer user_data) {
    VLOG(VLOG_LEVEL)<<"webcast :: enough_data";
  }

  static void video_need_data(GstElement* object, guint arg0, gpointer user_data) {
    VLOG(2)<<"===================LiveStreamProcessorImpl :: video data needed";
    bool printed = false;
    while(true) {
      LiveStreamProcessorImpl* processor = ((LiveStreamProcessorImpl*)user_data);
      if(processor->IsStopped()){
        return;
      }
      std::shared_ptr<orbit::dataPacket> rtp_packet = processor->PopVideoPacket();
      if (rtp_packet != NULL) {
        if (!printed && FLAGS_save_dot_file) {
          GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(processor->GetPipeline()),
          GST_DEBUG_GRAPH_SHOW_VERBOSE, "web_cast_video");
          printed = true;
        }
//        LOG(INFO) << "video arrival_time_ms = " << rtp_packet->arrival_time_ms
//                  << " remote_ntp_time_ms =" << rtp_packet->remote_ntp_time_ms
//                  << " rtp_timestamp = " << rtp_packet->rtp_timestamp;

        int size = rtp_packet->length;
        char *data = (char *)malloc(size);
        memcpy(data, rtp_packet->data, size);
        GstBuffer *buffer = gst_buffer_new_wrapped(data, size);
        GstFlowReturn ret = GST_FLOW_CUSTOM_SUCCESS_1;
        g_signal_emit_by_name(((LiveStreamProcessorImpl*)user_data)->GetVideoSrc(), "push-buffer", buffer, &ret );
        if (ret != GST_FLOW_OK) {
          LOG(ERROR) << "Push_buffer wrong....";
        }
        gst_buffer_unref(buffer);
        break;
      } else {
        usleep(2000);
      }
    }
  }

  static void audio_need_data(GstElement* element,guint arg0, gpointer user_data){
    VLOG(2)<<"===================LiveStreamProcessorImpl :: audio data needed";
    while(true){
      LiveStreamProcessorImpl* processor = (LiveStreamProcessorImpl*)user_data;
      if(processor->IsStopped()){
        return;
      }
      std::shared_ptr<orbit::dataPacket> item = processor->PopAudioPacket();
      if(item) {
        int size = item->length;
        char *data = (char *)malloc(size);
        memcpy(data, item->data, size);
        GstBuffer *buffer = gst_buffer_new_wrapped(data, size);

        GstFlowReturn ret = GST_FLOW_CUSTOM_SUCCESS_1;
//        LOG(INFO) << "audio arrival_time_ms = " << item->arrival_time_ms
//                  << " remote_ntp_time_ms =" << item->remote_ntp_time_ms
//                  << " rtp_timestamp = " << item->rtp_timestamp;

        g_signal_emit_by_name(processor->GetAudioSrc(), "push-buffer", buffer, &ret);
        if (ret != GST_FLOW_OK) {
          LOG(ERROR) << "Push_buffer wrong....";
        }
        gst_buffer_unref(buffer);
        break;
      } else {
        usleep(2000);
      }
    }
  }
} // end of annoymous namespace

  /*
   * ------------------------------------------------------------------------------------------
   * LiveStreamProcessor segment
   * ------------------------------------------------------------------------------------------
   */
  LiveStreamProcessor::LiveStreamProcessor(bool has_audio, bool has_video) {
    internal_ = new LiveStreamProcessorImpl(has_audio, has_video);
  }

  LiveStreamProcessor::~LiveStreamProcessor() {
    if(internal_ != NULL){
      delete internal_;
      internal_ = NULL;
    }
  }

  LiveStreamProcessor::LiveStreamProcessor(GstElement* pipeline, GstElement* video_tee){
    internal_ = new LiveStreamProcessorImpl(pipeline, video_tee);
  }

  bool LiveStreamProcessor::Start(const char* rtmp_location){
    return internal_->Start(rtmp_location);
  }

  void LiveStreamProcessor::RelayRtpVideoPacket(const std::shared_ptr<MediaOutputPacket> packet){
    internal_->RelayRtpVideoPacket(packet);
  }

  void LiveStreamProcessor::RelayRawAudioPacket(const char* outBuffer, int size){
    internal_->RelayRawAudioPacket(outBuffer, size);
  }

  void LiveStreamProcessor::RelayRtpAudioPacket(const std::shared_ptr<MediaOutputPacket> packet){
    internal_->RelayRtpAudioPacket(packet);
  }

  void LiveStreamProcessor::RelayRtpAudioPacket(const dataPacket& packet) {
    internal_->RelayRtpAudioPacket(packet);
  }

  void LiveStreamProcessor::RelayRtpVideoPacket(const dataPacket& packet) {
    internal_->RelayRtpVideoPacket(packet);
  }

  void LiveStreamProcessor::SetKeyFrameNeedListener(std::shared_ptr<IKeyFrameNeedListener> listener) {
    internal_->SetKeyFrameNeedListener(listener);
  }

  bool LiveStreamProcessor::LiveStreamProcessor::IsStarted(){
    return internal_->IsStarted();
  }
   /*
    * ------------------------------------------------------------------------------------------
    * @END_OF LiveStreamProcessor segment
    * ------------------------------------------------------------------------------------------
    */

  void LiveStreamProcessorImpl::SetupAudioElements() {
    audio_app_src_ = gst_element_factory_make("appsrc", "audio_app_src");
    GstCaps *caps = gst_caps_new_simple("application/x-rtp", "media", G_TYPE_STRING, "audio",
                                        "clock-rate", G_TYPE_INT, 48000,
                                        "payload", G_TYPE_INT, 111,
                                        "encoding-name", G_TYPE_STRING, "X-GST-OPUS-DRAFT-SPITTKA-00", NULL);
    g_object_set(G_OBJECT(audio_app_src_),
                 "caps", caps,
                 "is-live", true,
                 "stream-type", 0,
                 "format",GST_FORMAT_TIME,
                 "do-timestamp", false,
                 NULL);
    gst_caps_unref (caps);
    if(!FLAGS_use_push_mode) {
      g_signal_connect (audio_app_src_, "need-data", G_CALLBACK(audio_need_data), this);
    }
    g_signal_connect (audio_app_src_, "enough-data", G_CALLBACK(enough_data), this);
    VLOG(VLOG_LEVEL)<<"LiveStreamProcessorImpl :: SetupAudioElements";
    //GstElement* queue = gst_element_factory_make("queue",NULL);
    
    audio_jitter_ = gst_element_factory_make("rtpjitterbuffer","audio_jitter"); 
    GstElement* jitter = audio_jitter_;
    /*
    g_object_set(G_OBJECT(jitter),
                 ////        "mode",2 ,
                 "latency", 500,
                 "drop-on-latency", false,
                 "max-dropout-time", 500,
                 "max-misorder-time", 0,
                 "do-lost", true, NULL);
    */
    opusdepay_ = gst_element_factory_make("rtpopusdepay","rtpopusdepay1");
    GstElement * opusdepay = opusdepay_;
    opusdec_ = gst_element_factory_make("opusdec","opusdec1");
    GstElement * opusdec = opusdec_;
    audioconvert_ = gst_element_factory_make("audioconvert","audioconvert1");
    GstElement * audioconvert = audioconvert_;
    audioresample_ = gst_element_factory_make("audioresample","audioresample1");;
    GstElement * audioresample = audioresample_;
    voaacenc_ = gst_element_factory_make("voaacenc", NULL);
    g_object_set(G_OBJECT(voaacenc_), "bitrate", 80000, NULL);

    gst_bin_add_many(GST_BIN(pipeline_),audio_app_src_, jitter, opusdepay, opusdec, audioconvert, audioresample, voaacenc_,NULL);
    if(!gst_element_link_many(audio_app_src_, jitter, opusdepay, opusdec, audioconvert, audioresample, voaacenc_, NULL)){
      LOG(INFO) << "couldn't link audio elements";
    }
  }

  void LiveStreamProcessorImpl::SetupVideoElements() {
    VLOG(VLOG_LEVEL)<<"LiveStreamProcessorImpl :: SetupVideoElements";
    video_convert_ = gst_element_factory_make("videoconvert","convert");
    video_rate_ = gst_element_factory_make("videorate",NULL);
    g_object_set(G_OBJECT(video_rate_),"drop-only", true,"max-rate", 15,NULL);

    video_scale_ = gst_element_factory_make("videoscale", NULL);
    scale_caps_filter_ = gst_element_factory_make("capsfilter", NULL);
    GstCaps *scale_caps = gst_caps_new_simple("video/x-raw", "width", G_TYPE_INT, 640, "height", G_TYPE_INT, 480, NULL);

    g_object_set(G_OBJECT(scale_caps_filter_), "caps", scale_caps, NULL);
    gst_caps_unref (scale_caps);

    x264enc_ = gst_element_factory_make("x264enc", NULL);
    g_object_set(G_OBJECT(x264enc_),
        //Auto change the h264 encoder's target bitrate.
        "bitrate" , 512,
        "threads", 10,
        /**
         *  speed-preset : Preset name for speed/quality tradeoff options (can affect decode compatibility - impose restrictions separately for your target decoder)
         *   flags: readable, writable
         *   Enum "GstX264EncPreset" Default: 6, "medium"
         *      (0): None             - No preset
         *      (1): ultrafast        - ultrafast
         *      (2): superfast        - superfast
         *      (3): veryfast         - veryfast
         *      (4): faster           - faster
         *      (5): fast             - fast
         *      (6): medium           - medium
         *      (7): slow             - slow
         *      (8): slower           - slower
         *      (9): veryslow         - veryslow
         *      (10): placebo          - placebo
         */
        "speed-preset", 5,
        "quantizer", 21,
        "tune", 0x00000004,
        /**
         * Here we set pass to 5(Constant Quality)
         * Encoding pass/type
         * flags: readable, writable
         * Enum "GstX264EncPass" Default: 0, "cbr"
         *    (0): cbr              - Constant Bitrate Encoding
         *    (4): quant            - Constant Quantizer
         *    (5): qual             - Constant Quality
         *    (17): pass1            - VBR Encoding - Pass 1
         *    (18): pass2            - VBR Encoding - Pass 2
         *    (19): pass3            - VBR Encoding - Pass 3
         */
        "pass", 5,
        NULL);
    if (inst_type == LIVE_INTERNAL_PIPELINE) {
      video_app_src_ = gst_element_factory_make("appsrc", "video_app_src");
      GstCaps *caps = gst_caps_new_simple("application/x-rtp", "media", G_TYPE_STRING, "video", "clock-rate", G_TYPE_INT,
        90000, "encoding-name", G_TYPE_STRING, "VP8",
                                          NULL);
      g_object_set(G_OBJECT(video_app_src_),
                   "caps", caps,
                   "is-live", true,
                   "stream-type", 0,
                   "format",GST_FORMAT_TIME,
                   "do-timestamp", false,
                   NULL);
      gst_caps_unref (caps);
      video_jitter_ = gst_element_factory_make("rtpjitterbuffer","video_jitter");
      GstElement * jitter = video_jitter_;
      /*
      g_object_set(G_OBJECT(jitter),
                   ////        "mode",2 ,
                   "latency", 500,
                   "drop-on-latency", true,
                   "max-dropout-time", 500,
                   "max-misorder-time", 0,
                   "do-lost", true, NULL);
      */
      video_rtp_depay_ = gst_element_factory_make("rtpvp8depay", "video_rtp_depay");
      vp8dec_ = gst_element_factory_make("vp8dec", "vp8dec");
      g_object_set(G_OBJECT(vp8dec_),"threads",3,
        "post-processing" , true,
        "deblocking-level", 3,
        "noise-level", 0,
        "post-processing-flags", 0x00000403,
        NULL);
      clock_overlay_ = gst_element_factory_make("clockoverlay", "clock_overlay");

      gst_bin_add_many(GST_BIN(pipeline_),video_app_src_, jitter, video_rtp_depay_, vp8dec_,
          video_convert_, video_scale_, scale_caps_filter_, x264enc_, NULL);

      GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
      gst_bus_add_signal_watch(bus);
      g_signal_connect(bus, "message", G_CALLBACK (bus_message), this);
      gst_object_unref(bus);

      bool link_video_result = gst_element_link_many(video_app_src_, jitter, video_rtp_depay_, vp8dec_,
          video_convert_, video_scale_, scale_caps_filter_, x264enc_, NULL);
      VLOG(VLOG_LEVEL)<<"LiveStreamProcessorImpl :: link_video_result = "<<link_video_result;
      if(!FLAGS_use_push_mode) {
        g_signal_connect (video_app_src_, "need-data", G_CALLBACK(video_need_data), this);
      }
      g_signal_connect (video_app_src_, "enough-data", G_CALLBACK(enough_data), this);
    } else {
      g_assert (pipeline_ != NULL);
      g_assert (video_tee_ != NULL);
      tee_queue_ = gst_element_factory_make("queue", NULL);
      g_object_set(G_OBJECT(tee_queue_), "max-size-buffers", 20480000, NULL);
      gst_bin_add_many(GST_BIN(pipeline_), video_convert_ ,video_tee_, tee_queue_,x264enc_, NULL);
      gst_element_link_many(video_tee_, tee_queue_, video_convert_, x264enc_, NULL);
    }
  }

  LiveStreamProcessorImpl::LiveStreamProcessorImpl(bool has_audio, bool has_video) {
    //Init and link video elements
    video_queue_ = std::make_shared<RtpPacketBuffer>(20);
    audio_queue_ = std::make_shared<RtpPacketBuffer>(30);
 
    inst_type = LIVE_INTERNAL_PIPELINE;
    VLOG(VLOG_LEVEL)<<"Init webcast impl";
    pipeline_ = gst_pipeline_new( "pipeline" );
    GstClock* clock = gst_system_clock_obtain();
    gst_pipeline_use_clock(GST_PIPELINE(pipeline_), clock);
    if(has_audio){
      SetupAudioElements();
    }
    if (has_video) {
      SetupVideoElements();
    }
    SetupRtmpElements();
  }

  LiveStreamProcessorImpl::LiveStreamProcessorImpl(GstElement* pipeline, GstElement* video_tee){
    inst_type = LIVE_EXTERNAL_PIPELINE;
    pipeline_ = pipeline;
    this->video_tee_ = video_tee;
    SetupVideoElements();
    SetupAudioElements();
    SetupRtmpElements();
    SyncElements();
  }

  void LiveStreamProcessorImpl::SetupRtmpElements(){
    VLOG(VLOG_LEVEL)<<"LiveStreamProcessorImpl :: SetupRtmpElements";
    flv_mux_ = gst_element_factory_make("flvmux", NULL);
    g_object_set(G_OBJECT(flv_mux_), "streamable", true, NULL);
    gst_bin_add(GST_BIN(pipeline_), flv_mux_);

    // Link audio elements
    if(voaacenc_ != NULL) {
      GstPad *voaacencSrcPad = gst_element_get_static_pad(voaacenc_, "src");
      GstPad *audioSinkPad = gst_element_get_request_pad (flv_mux_, "audio");
      gst_pad_link(voaacencSrcPad, audioSinkPad);
    }

    // link to flvmux
    if (x264enc_ != NULL) {
      GstPad *x264encSrcPad = gst_element_get_static_pad(x264enc_, "src");
      GstPad *videoSinkPad = gst_element_get_request_pad (flv_mux_, "video");
      gst_pad_link(x264encSrcPad, videoSinkPad);
    }

    //Use tmp test address
    rtmp_sink_ = gst_element_factory_make("rtmpsink", NULL);
    g_object_set(G_OBJECT(rtmp_sink_), "location", FLAGS_live_location.c_str(), NULL);
    g_object_set(G_OBJECT(rtmp_sink_), "sync" , false, NULL);
    //g_object_set(GST_OBJECT(rtmp_sink_), "live", 1, NULL);
    g_object_set(G_OBJECT(rtmp_sink_), "async" , false, NULL);
    gst_bin_add(GST_BIN(pipeline_), rtmp_sink_);
    gst_element_link(flv_mux_, rtmp_sink_);
  }

  void LiveStreamProcessorImpl::SyncElements() {
    if(audio_app_src_ != NULL) {
      gst_element_sync_state_with_parent(audio_app_src_);
    }
    if(voaacenc_ != NULL) {
      gst_element_sync_state_with_parent(voaacenc_);
    }
    if(video_app_src_ != NULL) {
      gst_element_sync_state_with_parent(video_app_src_);
    }

    if(video_convert_ != NULL) {
      gst_element_sync_state_with_parent(video_convert_);
    }
    if(x264enc_ != NULL) {
      gst_element_sync_state_with_parent(x264enc_);
    }

    if(flv_mux_ != NULL){
      gst_element_sync_state_with_parent(flv_mux_);
    }
    if(rtmp_sink_ != NULL) {
      gst_element_sync_state_with_parent(rtmp_sink_);
    }

    if(video_rtp_depay_ != NULL) {
      gst_element_sync_state_with_parent(video_rtp_depay_);
    }
    if(vp8dec_ != NULL) {
      gst_element_sync_state_with_parent(vp8dec_);
    }
    if(video_tee_ != NULL) {
      gst_element_sync_state_with_parent(video_tee_);
    }
    if(video_rate_ != NULL) {
      gst_element_sync_state_with_parent(video_rate_);
    }
  }

  LiveStreamProcessorImpl::~LiveStreamProcessorImpl() {
    Destroy();
  }

  bool LiveStreamProcessorImpl::IsStarted() {
    return started_;
  }

  bool LiveStreamProcessorImpl::IsStopped() {
    return stopped_;
  }

  bool LiveStreamProcessorImpl::Start(const char* rtmp_location) {
    VLOG(2)<<"LiveStreamProcessorImpl :: Start webcast";
    if(started_) {
      return true;
    }

    //endof need delete
    if(!IsValidRtmpLocation(rtmp_location)){
      if (!FLAGS_live_location.empty()) {
        rtmp_location = FLAGS_live_location.c_str();
      } else {
        LOG(ERROR) << "invalid rtmp_location";
        return false;
      }
    }
    started_ = true;
    string location(rtmp_location);
    //location.append(" live=1");
    LOG(INFO)<<"Rtmp location is "<<location;
    g_object_set(G_OBJECT(rtmp_sink_), "location", location.c_str(), NULL);
    gst_element_set_state(GST_ELEMENT(pipeline_), GST_STATE_PLAYING);
    if(FLAGS_save_dot_file) {
      GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(pipeline_), GST_DEBUG_GRAPH_SHOW_ALL, "web_cast");
    }
    return true;
  }

  void LiveStreamProcessorImpl::RelayRawAudioPacket(const char* outBuffer, int size) {
    VLOG(2)<<"Receive audio packet";
    const std::shared_ptr<audioDataPacket> item(new audioDataPacket);
    memcpy(item->data, outBuffer, size);
    item->length = size;
    boost::mutex::scoped_lock lock(queue_mutex_);
    queue_.push_front(item);
  }

  void LiveStreamProcessorImpl::SetKeyFrameNeedListener(std::shared_ptr<IKeyFrameNeedListener> listener) {
    this->key_frame_listener_ = listener;
  }

  std::shared_ptr<dataPacket> LiveStreamProcessorImpl::PopVideoPacket() {
    std::shared_ptr<dataPacket> data_packet = video_queue_->PopPacket();
    if (data_packet == NULL) {
      return data_packet;
    }
    const RtpHeader* h = reinterpret_cast<const RtpHeader*>(data_packet->data);
    uint16_t seq = h->getSeqNumber();
    if (last_video_seq_ == -1) {
      last_video_seq_ = seq;
      return data_packet;
    }
    if (seq > last_video_seq_) {
      last_video_seq_ = seq;
      return data_packet;
    }
    if (seq < last_video_seq_ && abs(seq - last_video_seq_) > 8000) {
      last_video_seq_ = seq;
      return data_packet;
    } else {
      LOG(INFO) << "Lost video late sample seq=" << seq;
      return NULL;
    }
  }

  std::shared_ptr<dataPacket> LiveStreamProcessorImpl::PopAudioPacket() {
    std::shared_ptr<dataPacket> data_packet = audio_queue_->PopPacket();
    if (data_packet == NULL) {
      return data_packet;
    }
    const RtpHeader* h = reinterpret_cast<const RtpHeader*>(data_packet->data);
    uint16_t seq = h->getSeqNumber();
    if (last_audio_seq_ == -1) {
      last_audio_seq_ = seq;
      return data_packet;
    }
    if (seq > last_audio_seq_) {
      last_audio_seq_ = seq;
      return data_packet;
    }
    if (seq < last_audio_seq_ && abs(seq - last_audio_seq_) > 8000) {
      last_audio_seq_ = seq;
      return data_packet;
    } else {
      LOG(INFO) << "Lost audio late sample seq=" << seq;
      return NULL;
    }
  }

  void LiveStreamProcessorImpl::RelayRtpAudioPacket(const dataPacket& packet) {
    if (!video_queue_data_) {
      return;
    }
    if(FLAGS_use_push_mode) {
      const RtpHeader *h = reinterpret_cast<const RtpHeader*>(packet.data);
      if(audio_start_time_ == 0) {
        audio_start_time_ = h->getTimestamp();
        GstClock *clock  = GST_ELEMENT_CLOCK(audio_app_src_);
        GstClockTime base_time = GST_ELEMENT_CAST(audio_app_src_)->base_time;
        GstClockTime now = gst_clock_get_time(clock);
        audio_delay_time_ = now - base_time;
      }
      if(audio_ntp_start_time_ == 0 && packet.remote_ntp_time_ms != -1) {
        audio_ntp_start_time_ = packet.remote_ntp_time_ms - (h->getTimestamp() - audio_start_time_)/48;
      }
       // LOG(INFO)<<"Audio delay time = "<<audio_delay_time_/1000/1000;
      GstFlowReturn ret = GST_FLOW_CUSTOM_SUCCESS_1;
      char *data = (char*)malloc(packet.length);
      memcpy(data, packet.data, packet.length);
      GstBuffer *buffer = gst_buffer_new_wrapped(data, packet.length);
      long time = 0;
      if(audio_ntp_start_time_ > 0) {
        time = packet.remote_ntp_time_ms - audio_ntp_start_time_;
      } else {
        time = (h->getTimestamp() - audio_start_time_)/48;
      }
      GST_BUFFER_DTS(buffer) = (time + JITTER_TIME) * 1000000 + audio_delay_time_;
      GST_BUFFER_PTS(buffer) = 0;
      g_signal_emit_by_name(audio_app_src_, "push_buffer", buffer, &ret);
      gst_buffer_unref(buffer);
    } else {
      audio_queue_->PushPacket(packet);
    }
  }

  void LiveStreamProcessorImpl::RelayRtpVideoPacket(const dataPacket& packet) {
    video_queue_data_ = true;
    if (IsValidVideoPacket(packet)) {
      /**
       * stream_runing_time = (current_rtp_time - start_rtp_time)/90000 * 1000
       * unit is ms
       */
      if(FLAGS_use_push_mode){
        const RtpHeader* h = reinterpret_cast<const RtpHeader*>(packet.data);
        if(video_start_time_ == 0) {
          video_start_time_ = h->getTimestamp();
          GstClock *clock  = GST_ELEMENT_CLOCK(video_app_src_);
          GstClockTime base_time = GST_ELEMENT_CAST(video_app_src_)->base_time;
          GstClockTime now = gst_clock_get_time(clock);
              video_delay_time_ = now - base_time;
        }
        if(video_ntp_start_time_ == 0 && packet.remote_ntp_time_ms != -1) {
          video_ntp_start_time_ = packet.remote_ntp_time_ms - (h->getTimestamp() - video_start_time_)/90;
        }
        GstFlowReturn ret = GST_FLOW_CUSTOM_SUCCESS_1;
        char *data = (char*)malloc(packet.length);
        memcpy(data, packet.data, packet.length);
        GstBuffer *buffer = gst_buffer_new_wrapped(data, packet.length);

        GstClock *clock  = GST_ELEMENT_CLOCK(video_app_src_);
        GstClockTime base_time = GST_ELEMENT_CAST(video_app_src_)->base_time;
        GstClockTime now = gst_clock_get_time(clock);
        long time = 0;
        if(video_ntp_start_time_ > 0) {
          time = packet.remote_ntp_time_ms - video_ntp_start_time_;
        } else {
          time = (h->getTimestamp() - video_start_time_)/90;
        }
        GST_BUFFER_DTS(buffer) = (time + JITTER_TIME) * 1000000 + video_delay_time_;
        GST_BUFFER_PTS(buffer) = 0;
        g_signal_emit_by_name(video_app_src_, "push_buffer", buffer, &ret);
        gst_buffer_unref(buffer);
      } else {
        video_queue_->PushPacket(packet);
      }
    }
  }

  void LiveStreamProcessorImpl:: RelayRtpAudioPacket(const std::shared_ptr<MediaOutputPacket> packet){
    if (!video_queue_data_) {
      return;
    }

    packetType packet_type = AUDIO_PACKET;

    dataPacket p;
    p.comp = 0;
    p.type = packet_type;

    RtpHeader rtp_header;
    rtp_header.setTimestamp(packet->timestamp);
    rtp_header.setSeqNumber(packet->seq_number);
    rtp_header.setSSRC(packet->ssrc);
    rtp_header.setMarker(packet->end_frame);
    rtp_header.setPayloadType(OPUS_48000_PT);

    memcpy(&(p.data[0]), &rtp_header, RTP_HEADER_BASE_SIZE);
    memcpy(&(p.data[RTP_HEADER_BASE_SIZE]), packet->encoded_buf, packet->length);
    p.length = packet->length + RTP_HEADER_BASE_SIZE;

    RelayRtpAudioPacket(p);
//    audio_queue_->PushPacket((const char *)&(p.data[0]), p.length);
  }

  void LiveStreamProcessorImpl::RelayRtpVideoPacket(const std::shared_ptr<MediaOutputPacket> packet) {
    packetType packet_type = VIDEO_PACKET;

    dataPacket p;
    p.comp = 0;
    p.type = packet_type;

    RtpHeader rtp_header;
    rtp_header.setTimestamp(packet->timestamp);
    rtp_header.setSeqNumber(packet->seq_number);
    rtp_header.setSSRC(packet->ssrc);
    rtp_header.setMarker(packet->end_frame);
    rtp_header.setPayloadType(VP8_90000_PT);

    memcpy(&(p.data[0]), &rtp_header, RTP_HEADER_BASE_SIZE);
    memcpy(&(p.data[RTP_HEADER_BASE_SIZE]), packet->encoded_buf, packet->length);
    p.length = packet->length + RTP_HEADER_BASE_SIZE;

    RelayRtpVideoPacket(p);
  }

  void LiveStreamProcessorImpl::DestroyElements() {
    // Destroy all the audio GstElement
    if (audio_app_src_) {
      gst_object_unref(GST_OBJECT(audio_app_src_));
      audio_app_src_ = NULL;
    }
    if (voaacenc_) {
      gst_object_unref(GST_OBJECT(voaacenc_));
      voaacenc_ = NULL;
    }
    if (opusdepay_) {
      gst_object_unref(GST_OBJECT(opusdepay_));
      opusdepay_ = NULL;
    }
    if (opusdec_) {
      gst_object_unref(GST_OBJECT(opusdec_));
      opusdec_ = NULL;
    }
    if (audioconvert_) {
      gst_object_unref(GST_OBJECT(audioconvert_));
      audioconvert_ = NULL;
    }
    if (audioresample_) {
      gst_object_unref(GST_OBJECT(audioresample_));
      audioresample_ = NULL;
    }
    if (audio_jitter_) {
      gst_object_unref(GST_OBJECT(audio_jitter_));
      audio_jitter_ = NULL;
    }
    
      // Destroy all the video GstElement
    if (video_app_src_) {
      gst_object_unref(GST_OBJECT(video_app_src_));
      video_app_src_ = NULL;
    }
    if (clock_overlay_) {
      gst_object_unref(GST_OBJECT(clock_overlay_));
      clock_overlay_ = NULL;
    }
    if (video_convert_) {
      gst_object_unref(GST_OBJECT(video_convert_));
      video_convert_ = NULL;
    }
    if (video_rate_) {
      gst_object_unref(GST_OBJECT(video_rate_));
      video_rate_ = NULL;
    }
    if (x264enc_) {
      gst_object_unref(GST_OBJECT(x264enc_));
      x264enc_ = NULL;
    }
    if (video_rtp_depay_) {
      gst_object_unref(GST_OBJECT(video_rtp_depay_));
      video_rtp_depay_ = NULL;
    }
    if (vp8dec_) {
      gst_object_unref(GST_OBJECT(vp8dec_));
      vp8dec_ = NULL;
    }

   if (video_scale_) {
      gst_object_unref(GST_OBJECT(video_scale_));
      video_scale_ = NULL;
    }

   if (scale_caps_filter_) {
      gst_object_unref(GST_OBJECT(scale_caps_filter_));
      scale_caps_filter_ = NULL;
    }

    if (video_jitter_) {
      gst_object_unref(GST_OBJECT(video_jitter_));
      video_jitter_ = NULL;
    }

    // Destroy the rtmp GstElement
    if (flv_mux_) {
      gst_object_unref(GST_OBJECT(flv_mux_));
      flv_mux_ = NULL;
    }
    if (rtmp_sink_) {
      gst_object_unref(GST_OBJECT(rtmp_sink_));
      rtmp_sink_ = NULL;
    }
    if (video_tee_) {
      gst_object_unref(GST_OBJECT(video_tee_));
      video_tee_ = NULL;
    }
    if (tee_queue_) {
      gst_object_unref(GST_OBJECT(tee_queue_));
      tee_queue_ = NULL;
    }
  }

  void LiveStreamProcessorImpl::Destroy() {
    LOG(INFO)<<"LiveStreamProcessorImpl :: Destroy";
    stopped_ = true;
    if(pipeline_){
      gst_element_set_state (pipeline_, GST_STATE_NULL);
      gst_object_unref (pipeline_);
      pipeline_ = NULL;
      // NOTE(chengxu): the GstElements in the pipeline seems to be recycled
      // automatically so we don't need to unref and destroy them explicitly?
      //DestroyElements();
    }
    boost::mutex::scoped_lock lock(queue_mutex_);
    queue_.clear();
  }
}  // namespace orbit
  
