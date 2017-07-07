/*
 * Copyright © 2016 Orangelab Inc. All Rights Reserved.
 * 
 * Author: qingyong@orangelab.cn (Qingyong Zhang)
 * Created on: Apr 25, 2016
 *
 *
 * stream_recorder_element.cc
 * --------------------------------------------------------
 *
 * --------------------------------------------------------
 */

#include "stream_recorder_element.h"
#include "stream_service/orbit/live_stream/common_defines.h"

DEFINE_string(record_path, "/tmp/orbit_recorder/default.webm",
              "Specify stream record location.");
DEFINE_string(record_format, "webm", "Set the default format for the recorded file. Current support 'webm' and 'mp4'");

DECLARE_bool(save_dot_file);
DEFINE_bool(use_push_mode, true, "Push rtp packet to appsrc else use pull mode for queue. ");
DEFINE_bool(use_ntp_time, true, "Use ntp time in packet to calculate packet dts. ");
#define JITTER_TIME 0 //2000ms
namespace orbit {
long long start = 0;
static GstBusSyncReply bus_sync_signal_handler (GstBus * bus, GstMessage * msg, gpointer data){
  GstElement* element = (GstElement*)data;
  LOG(INFO)<<"-----------------------bus_sync_signal_handler-------------------"
      << msg->type << ":"
      <<GST_ELEMENT_NAME(element)
      << gst_message_type_get_name(GST_MESSAGE_TYPE(msg));
}
static GstPadProbeReturn event_probe_cb (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
//  LOG(INFO)<<"-----------------------event_probe_cb-------------------"<<GST_EVENT_TYPE (GST_PAD_PROBE_INFO_DATA (info));
  if (GST_EVENT_TYPE (GST_PAD_PROBE_INFO_DATA (info)) == GST_EVENT_SEGMENT){
    GstEvent* event = GST_EVENT_CAST(GST_PAD_PROBE_INFO_DATA (info));
    const GstSegment* segment;
    gst_event_parse_segment(event, &segment);
//
//      LOG(INFO)<<"\nFlag is:"<<segment->flags<<"\n"
//          <<"rate is     :"<<segment->rate<<"\n"
//          <<"applied_rate is :"<<segment->applied_rate<<"\n"
//          <<"format is   :"<<segment->format<<"\n"
//          <<"base is     :"<<segment->base<<"\n"
//          <<"offset is   :"<<segment->offset<<"\n"
//          <<"start is    :"<<segment->start<<"\n"
//          <<"stop is     :"<<segment->stop<<"\n"
//          <<"time is     :"<<segment->time<<"\n"
//          <<"position is :"<<segment->position<<"\n"
//          <<"duration is :"<<segment->duration<<"\n";
//
//
//      if(segment->start> start){
//        start = segment->start;
//      } else {
//        LOG(INFO)<<"Do drop segment -------------------------";
//        return GST_PAD_PROBE_DROP;
//      }
////    if(segment->start<1000) {
////      return GST_PAD_PROBE_OK;
////    }
  }
////    return GST_PAD_PROBE_PASS;
//  LOG(INFO)<<"probe info : "<<GST_PAD_PROBE_INFO_TYPE(info)<<" "
//      <<GST_PAD_PROBE_INFO_EVENT(info);
//  VideoMixerPluginImpl* plugin = (VideoMixerPluginImpl*)user_data;
//  if (GST_EVENT_TYPE (GST_PAD_PROBE_INFO_DATA (info)) != GST_EVENT_EOS)
//    return GST_PAD_PROBE_PASS;
//  gst_pad_remove_probe (pad, GST_PAD_PROBE_INFO_ID (info));
//  gst_pad_send_event (plugin->GetMixerSinkPad(), gst_event_new_eos ());
//  g_idle_add(remove_element,user_data);
  return GST_PAD_PROBE_OK;
}
static gboolean bus_message (GstBus * bus, GstMessage * message, gpointer pipe) {
  VLOG(2)<< "Got message" << "========================================="
           << message->type << ":"
           << gst_message_type_get_name(GST_MESSAGE_TYPE(message));
  
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
  LOG(FATAL)<<"Get enought data error ,here we need to handler ! @QingyongZhang";
  LOG(INFO)<<"-----------------------------webcast :: enough_data===========================================";
}

StreamRecorderElement::StreamRecorderElement(const std::string& file_name,
                                               int encoding) {
  //Init and link video elements
  video_queue_ = std::make_shared<RtpPacketBuffer>(60);
  audio_queue_ = std::make_shared<RtpPacketBuffer>(100);

  pipeline_ = gst_pipeline_new( "stream_recorder_element_pipeline" );
  GstClock* clock = gst_system_clock_obtain();
  gst_pipeline_use_clock(GST_PIPELINE(pipeline_), clock);

  GstClockTime latency = 600 *1000*1000;
  gst_pipeline_set_latency(GST_PIPELINE(pipeline_), latency);

  GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
  g_signal_connect(bus, "message", G_CALLBACK (bus_message), this);
  gst_bus_add_signal_watch(bus);
  gst_object_unref(bus);

  running_ = true;
  std::string export_file = FLAGS_record_path;
  if (!file_name.empty()) {
    export_file = file_name;
  }

  video_encoding_ = encoding;

  if(FLAGS_record_format == "webm") {
    SetupRecorderElements_webm(export_file);
    SetupAudioElements_webm();
    SetupVideoElements_webm();
  } else {
    SetupRecorderElements_mp4(export_file);
    SetupAudioElements_mp4();
    SetupVideoElements_mp4();
  }


  gst_element_set_state(GST_ELEMENT(pipeline_), GST_STATE_PLAYING);
  if(FLAGS_save_dot_file){
    GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(pipeline_), GST_DEBUG_GRAPH_SHOW_VERBOSE, "stream_recorder_pipeline");
  }
}

StreamRecorderElement::~StreamRecorderElement() {
  FlushData();
  Destroy();
}

void StreamRecorderElement::FlushData() {
  LOG(INFO)<<"Do flush data";
  bool flushed;
  audio_queue_->SetPrebufferingSize(0);
  video_queue_->SetPrebufferingSize(0);

  while(true) {
    flushed = true;
    if(!audio_queue_->IsEmpty()) {
      std::shared_ptr<dataPacket> dataPacket = audio_queue_->PopPacket();
      _PushAudioPacket(dataPacket);
      flushed = false;
    }
    if(!video_queue_->IsEmpty()) {
      std::shared_ptr<dataPacket> dataPacket = video_queue_->PopPacket();
      _PushVideoPacket(dataPacket);
      flushed = false;
    }
    if(flushed) {
      break;
    }
  }
}
void StreamRecorderElement::Destroy() {
  LOG(INFO) << "StreamRecorderElement destroy";
  GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN (pipeline_), GST_DEBUG_GRAPH_SHOW_VERBOSE, "destroy");
  running_ = false;
  GstFlowReturn ret;
  g_signal_emit_by_name(audio_src_, "end-of-stream", &ret);
  g_signal_emit_by_name(video_src_, "end-of-stream", &ret);
  GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));

  while(true) {
    GstMessage* msg = gst_bus_poll(bus, GST_MESSAGE_ANY, GST_CLOCK_TIME_NONE);
//    LOG(INFO)<<"==========="<<msg->type;
    if(msg->type == GST_MESSAGE_EOS){
      break;
    }
  }
  if (pipeline_ != NULL) {
    gst_element_set_state (pipeline_, GST_STATE_NULL);
    gst_object_unref(pipeline_);
    pipeline_ = NULL;
  }
}

void StreamRecorderElement::Start() {
}

void StreamRecorderElement::SetupRecorderElements_webm(const std::string& file_location) {
  muxer_ = gst_element_factory_make("webmmux","webmmux");
  g_object_set(G_OBJECT(muxer_),"version", 1, NULL);
  file_sink_ = gst_element_factory_make("filesink","filesink");
  g_object_set(G_OBJECT(file_sink_),
      "location", file_location.c_str(),
      "async", false,
      "sync", false,
      "blocksize", 1500,
      NULL);

  GstCaps* caps = gst_caps_new_simple("video/webm", NULL);
  gst_bin_add_many(GST_BIN(pipeline_),muxer_, file_sink_, NULL);
  g_object_set(GST_OBJECT(muxer_), "streamable", false, NULL);
  gst_element_link_filtered(muxer_, file_sink_, caps);

  if(caps != NULL) {
    gst_caps_unref (caps);
  }
}

void StreamRecorderElement::SetupRecorderElements_mp4(const std::string& file_location) {
  muxer_ = gst_element_factory_make("qtmux","mp4mux");
  file_sink_ = gst_element_factory_make("filesink","filesink");
  g_object_set(G_OBJECT(file_sink_),"location", file_location.c_str(),
      "sync", false,
      NULL);

  GstCaps* caps = gst_caps_new_simple("video/quicktime", NULL);
  gst_bin_add_many(GST_BIN(pipeline_),muxer_, file_sink_, NULL);
  g_object_set(GST_OBJECT(muxer_), "streamable", false, NULL);

  gst_element_link_filtered(muxer_, file_sink_, caps);
  if(caps != NULL) {
    gst_caps_unref (caps);
  }
}


void StreamRecorderElement::PushAudioPacket(const dataPacket& packet) {
  if (!video_queue_data_ || audio_src_ == NULL) {
    return;
  }
  if(FLAGS_use_push_mode) {
    audio_queue_->PushPacket(packet);
    if(audio_queue_->PrebufferingDone()){
      std::shared_ptr<dataPacket> dataPacket = audio_queue_->PopPacket();
      _PushAudioPacket(dataPacket);
    }
  } else {
    audio_queue_->PushPacket(packet);
  }
}

void StreamRecorderElement::PushVideoPacket(const dataPacket& packet){
  video_queue_data_ = true;
  if (running_ && IsValidVideoPacket(packet)) {
    /**
     * stream_runing_time = (current_rtp_time - start_rtp_time)/90000 * 1000
     * unit is ms
     */
    if(FLAGS_use_push_mode){
      video_queue_->PushPacket(packet);
      if(video_queue_->PrebufferingDone()){
        std::shared_ptr<dataPacket> dataPacket = video_queue_->PopPacket();
        _PushVideoPacket(dataPacket);
      }
    } else {
      video_queue_->PushPacket(packet);
    }
  }
}

void StreamRecorderElement::SetupAudioElements_webm() {
  GstCaps *caps = NULL;
  GstCaps *mux_caps = NULL;
  audio_src_ = gst_element_factory_make("appsrc", NULL);
  caps = gst_caps_new_simple("application/x-rtp", "media", G_TYPE_STRING, "audio",
      "encoding-name", G_TYPE_STRING, "OPUS", "clock-rate", G_TYPE_INT,
      48000 , "payload", G_TYPE_INT, OPUS_48000_PT, NULL);

  g_object_set(G_OBJECT(audio_src_),
      "caps", caps,
      "stream-type", 0,
      "format", GST_FORMAT_TIME,
      "do-timestamp", false,
      "max-bytes", 0,
      "is-live", true,NULL);
//  g_signal_connect(audio_src_, "need-data", G_CALLBACK(audio_need_data), this);
  audio_jitter_buffer_ = gst_element_factory_make("queue", "audio_jitter_buffer");
  g_object_set(G_OBJECT(audio_jitter_buffer_),
    "max-size-buffers", 0,
    "max-size-bytes", 0,
    "max-size-time", 0,
    "min-threshold-buffers", 20,
    "generate-buffer-list", true,
    NULL);
  rtp_opus_depay_ = gst_element_factory_make("rtpopusdepay", "opusdepay");
  assert(audio_src_);
//  assert(gst_audio_queue_);
  assert(audio_jitter_buffer_);
  assert(rtp_opus_depay_);
  gst_bin_add_many(GST_BIN(pipeline_),audio_src_, audio_jitter_buffer_, rtp_opus_depay_, NULL);

  bool result = gst_element_link_many(audio_src_,audio_jitter_buffer_, rtp_opus_depay_,NULL);
  assert(result);

  mux_caps = gst_caps_new_simple("audio/x-opus",
      "media", G_TYPE_STRING, "audio",
          "encoding-name", G_TYPE_STRING, "OPUS",
          "channels", G_TYPE_INT, 2,
          "rate", G_TYPE_INT, 48000,
          NULL);
  result = gst_element_link_filtered(rtp_opus_depay_, muxer_, mux_caps);
  assert(result);
  if(caps != NULL) {
    gst_caps_unref (caps);
  }
  if(mux_caps != NULL){
    gst_caps_unref (mux_caps);
  }
}

void StreamRecorderElement::SetupAudioElements_mp4() {
  GstCaps *caps = NULL;
  GstCaps *mux_caps = NULL;
  audio_src_ = gst_element_factory_make("appsrc", "audio_src");
  caps = gst_caps_new_simple("application/x-rtp", "media", G_TYPE_STRING, "audio",
      "encoding-name", G_TYPE_STRING, "OPUS", "clock-rate", G_TYPE_INT,
      48000 , "payload", G_TYPE_INT, OPUS_48000_PT, NULL);

  g_object_set(G_OBJECT(audio_src_),
      "caps", caps,
      "stream-type", 0,
      "format", GST_FORMAT_TIME,
      "block", true,
      "do-timestamp", false,
      "is-live", true,NULL);
//  g_signal_connect(audio_src_, "need-data", G_CALLBACK(audio_need_data), this);
  audio_jitter_buffer_ = gst_element_factory_make("queue", "audio_jitter_buffer");
  g_object_set(G_OBJECT(audio_jitter_buffer_),
    "max-size-buffers", 0,
    "max-size-bytes", 0,
    "max-size-time", 0,
    "min-threshold-buffers", 20,
    "generate-buffer-list", true,
    NULL);
  rtp_opus_depay_ = gst_element_factory_make("rtpopusdepay", "opusdepay");

  GstElement * opusdec = gst_element_factory_make("opusdec","opusdec");
  GstElement *voaacenc_ = gst_element_factory_make("voaacenc", NULL);
  gst_bin_add_many(GST_BIN(pipeline_),audio_src_, audio_jitter_buffer_, rtp_opus_depay_,
      opusdec, voaacenc_, NULL);
  bool link_audio_elements = gst_element_link_many(audio_src_,audio_jitter_buffer_, rtp_opus_depay_,
      opusdec, voaacenc_, NULL);

  LOG(INFO)<<"Link audio result is "<<link_audio_elements;
  mux_caps = gst_caps_new_simple("audio/mpeg",
      "media", G_TYPE_STRING, "audio",
      "channels", G_TYPE_INT, 2,
      "rate", G_TYPE_INT, 48000,
      "stream-format", G_TYPE_STRING, "raw",
       NULL);
  link_audio_elements = gst_element_link_filtered(voaacenc_, muxer_, mux_caps);

  if(caps != NULL) {
    gst_caps_unref (caps);
  }
  if(mux_caps != NULL){
    gst_caps_unref (mux_caps);
  }
}

void StreamRecorderElement::RelayMediaOutputPacket(const std::shared_ptr<MediaOutputPacket> packet,
                                                       packetType packet_type) {
  if (packet->encoded_buf == NULL) {
    LOG(ERROR) << "OutputPacket is invalid.";
    return;
  }

  if (!running_) {
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
    rtp_header.setPayloadType(video_encoding_);
  } else {
    rtp_header.setPayloadType(OPUS_48000_PT);
  }

  memcpy(&(p.data[0]), &rtp_header, RTP_HEADER_BASE_SIZE);
  memcpy(&(p.data[RTP_HEADER_BASE_SIZE]), packet->encoded_buf, packet->length);
  p.length = packet->length + RTP_HEADER_BASE_SIZE;

  if (packet_type == VIDEO_PACKET) {
    PushVideoPacket(p);
  } else {
    PushAudioPacket(p);
  }
}

void StreamRecorderElement::SetupVideoElements_webm() {
  GstCaps *mux_caps = NULL;
  std::string encoding_name = "VP8";
  std::string video_decoder = "vp8dec";
  std::string rtp_depay_element = "rtpvp8depay";
  std::string rtp_caps = "video/x-vp8";
  switch(video_encoding_) {
  case VP8_90000_PT:
    encoding_name = "VP8";
    rtp_depay_element = "rtpvp8depay";
    video_decoder = "vp8dec";
    rtp_caps = "video/x-vp8";
    break;
  case VP9_90000_PT:
    encoding_name = "VP9";
    rtp_depay_element = "rtpvp9depay";
    video_decoder = "vp9dec";
    rtp_caps = "video/x-vp9";
    break;
  case H264_90000_PT:
    encoding_name = "H264";
    rtp_depay_element = "rtph264depay";
    video_decoder = "avdec_h264";
    rtp_caps = "video/x-h264";
    break;
  }


  video_src_ = gst_element_factory_make("appsrc", NULL);
  GstCaps *caps = gst_caps_new_simple("application/x-rtp",
      "media", G_TYPE_STRING, "video",
      "clock-rate", G_TYPE_INT, 90000,
      "payload", G_TYPE_INT, video_encoding_,
      "encoding-name", G_TYPE_STRING, encoding_name.c_str(), NULL);
  g_object_set(G_OBJECT(video_src_),
      "caps", caps,
      "stream-type", 0,
      "format", GST_FORMAT_TIME,
      "do-timestamp", false,
      "max-bytes", 0,
      "block", true,
      "is-live", true,NULL);

  //  g_signal_connect(video_src_, "need-data", G_CALLBACK(video_need_data), this);
  video_jitter_buffer_ = gst_element_factory_make("queue", "video_jitter_buffer");
  g_object_set(G_OBJECT(video_jitter_buffer_),
  "max-size-buffers" ,0,
  "max-size-bytes"  ,0,
  "max-size-time", 0,
  "min-threshold-buffers", 15,
  "generate-buffer-list", true,
  NULL);


  rtp_depay_ = gst_element_factory_make(rtp_depay_element.c_str(), "video_rtp_depay");
  gst_bin_add_many(GST_BIN(pipeline_),video_src_, video_jitter_buffer_, rtp_depay_,NULL);
  gst_element_link_many(video_src_, video_jitter_buffer_, rtp_depay_, NULL);

  mux_caps = gst_caps_new_simple(rtp_caps.c_str(),
      "media", G_TYPE_STRING, "video",
      "clock-rate", G_TYPE_INT, 90000,
      "payload", G_TYPE_INT, video_encoding_,
      "encoding-name", G_TYPE_STRING, encoding_name.c_str(), NULL);
  gst_element_link_filtered(rtp_depay_, muxer_, mux_caps);
  GstPad* pad = gst_element_get_static_pad(muxer_, "src");
  if(pad != NULL) {
    LOG(INFO)<<"++++++++++++++GetPad";
    gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, event_probe_cb, this, NULL);
  } else {
    LOG(INFO)<<"++++++++++++++GetPad is null";
  }

  if(caps != NULL) {
    gst_caps_unref (caps);
  }
  if(mux_caps != NULL) {
    gst_caps_unref (mux_caps);
  }
}

void StreamRecorderElement::_PushAudioPacket(std::shared_ptr<dataPacket> dataPacket) {
  const RtpHeader *h = reinterpret_cast<const RtpHeader*>(dataPacket->data);
  if(audio_start_time_ == 0) {
    audio_start_time_ = h->getTimestamp();
    GstClock *clock  = GST_ELEMENT_CLOCK(audio_src_);
    GstClockTime base_time = GST_ELEMENT_CAST(audio_src_)->base_time;
    GstClockTime now = gst_clock_get_time(clock);
    audio_delay_time_ = now - base_time;
  }
  if(audio_ntp_start_time_ == 0 && dataPacket->remote_ntp_time_ms != -1) {
    LOG(INFO)<<"Audio start use ntp time ==================";
    audio_ntp_start_time_ = dataPacket->remote_ntp_time_ms - (h->getTimestamp() - audio_start_time_)/48;
  }
  GstFlowReturn ret = GST_FLOW_CUSTOM_SUCCESS_1;
  char *data = (char*)malloc(dataPacket->length);
  memcpy(data, dataPacket->data, dataPacket->length);
  GstBuffer *buffer = gst_buffer_new_wrapped(data, dataPacket->length);
  long time = 0;
  if(audio_ntp_start_time_ > 0 && FLAGS_use_ntp_time) {
    time = dataPacket->remote_ntp_time_ms - audio_ntp_start_time_;
  } else {
    time = (h->getTimestamp() - audio_start_time_)/48;
  }
  guint64 times = (time + JITTER_TIME) * 1000000 + audio_delay_time_;
  VLOG(12)<<"Audio time is "<< times<< "   " << time;
  GST_BUFFER_DTS(buffer) = (time + JITTER_TIME) * 1000000 + audio_delay_time_;
//    GST_BUFFER_PTS(buffer) = 0;
  g_signal_emit_by_name(audio_src_, "push_buffer", buffer, &ret);
  gst_buffer_unref(buffer);
}

void StreamRecorderElement::_PushVideoPacket(std::shared_ptr<dataPacket> dataPacket) {
  const RtpHeader* h = reinterpret_cast<const RtpHeader*>(dataPacket->data);
  if(video_start_time_ == 0) {
    video_start_time_ = h->getTimestamp();
    GstClock *clock  = GST_ELEMENT_CLOCK(video_src_);
    GstClockTime base_time = GST_ELEMENT_CAST(video_src_)->base_time;
    GstClockTime now = gst_clock_get_time(clock);
    video_delay_time_ = now - base_time;
  }

  if(video_ntp_start_time_ == 0 && dataPacket->remote_ntp_time_ms != -1) {
    LOG(INFO)<<"Video start use ntp time ==================";
    video_ntp_start_time_ = dataPacket->remote_ntp_time_ms - (h->getTimestamp() - video_start_time_)/90;
  }

  GstFlowReturn ret = GST_FLOW_CUSTOM_SUCCESS_1;
  char *data = (char*)malloc(dataPacket->length);
  memcpy(data, dataPacket->data, dataPacket->length);
  GstBuffer *buffer = gst_buffer_new_wrapped(data, dataPacket->length);
  long time = 0;
  if(video_ntp_start_time_ > 0 && FLAGS_use_ntp_time) {
    time = dataPacket->remote_ntp_time_ms - video_ntp_start_time_;
  } else {
    time = (h->getTimestamp() - video_start_time_)/90;
  }
  guint64 times = (time + JITTER_TIME) * 1000000 + video_delay_time_;
  VLOG(12)<<"Video time is "<< times<< "   " << time;
  GST_BUFFER_DTS(buffer) = (time + JITTER_TIME) * 1000000 + video_delay_time_;
//      GST_BUFFER_PTS(buffer) = 0;
  g_signal_emit_by_name(video_src_, "push_buffer", buffer, &ret);
     gst_buffer_unref(buffer);
}

void StreamRecorderElement::SetupVideoElements_mp4() {
  GstCaps *caps = NULL;
  GstCaps *mux_caps = NULL;
  std::string encoding_name = "VP8";
  std::string video_decoder = "vp8dec";
  std::string rtp_depay_element = "rtpvp8depay";
  std::string rtp_caps = "video/x-vp8";
  switch(video_encoding_) {
  case VP8_90000_PT:
    encoding_name = "VP8";
    rtp_depay_element = "rtpvp8depay";
    video_decoder = "vp8dec";
    rtp_caps = "video/x-vp8";
    break;
  case VP9_90000_PT:
    encoding_name = "VP9";
    rtp_depay_element = "rtpvp9depay";
    video_decoder = "vp9dec";
    rtp_caps = "video/x-vp9";
    break;
  case H264_90000_PT:
    encoding_name = "H264";
    rtp_depay_element = "rtph264depay";
    video_decoder = "avdec_h264";
    rtp_caps = "video/x-h264";
    break;
  }
  video_src_ = gst_element_factory_make("appsrc", "video_src");
  g_signal_connect (video_src_, "enough-data", G_CALLBACK(enough_data), this);
  caps = gst_caps_new_simple("application/x-rtp",
      "media", G_TYPE_STRING, "video",
      "clock-rate", G_TYPE_INT, 90000,
      "payload", G_TYPE_INT, video_encoding_,
      "encoding-name", G_TYPE_STRING, encoding_name.c_str(), NULL);
  g_object_set(G_OBJECT(video_src_),
      "caps", caps,
      "stream-type", 0,
      "block", true,
      "format", GST_FORMAT_TIME,
      "do-timestamp", false,
      "is-live", true,NULL);
//  g_signal_connect(video_src_, "need-data", G_CALLBACK(video_need_data), this);
  video_jitter_buffer_ = gst_element_factory_make("queue", "video_jitter_buffer");
    g_object_set(G_OBJECT(video_jitter_buffer_),
    "max-size-buffers" ,0,
    "max-size-bytes"  ,0,
    "max-size-time", 0,
    "min-threshold-buffers", 15,
    "generate-buffer-list", true,
    NULL);
  rtp_depay_ = gst_element_factory_make(rtp_depay_element.c_str(), "video_rtp_depay");
  GstElement* decoder = gst_element_factory_make(video_decoder.c_str(), "video_decoder");
  GstElement* h264enc = gst_element_factory_make("x264enc", "h264_encoder");
  GstElement* h264parse = gst_element_factory_make("h264parse", "parser");
  GstElement* video_scale_ = gst_element_factory_make("videoscale", NULL);
  GstElement* scale_caps_filter_ = gst_element_factory_make("capsfilter", NULL);
  GstElement* video_rate = gst_element_factory_make("videorate", NULL);
  GstCaps *scale_caps = gst_caps_new_simple("video/x-raw",
      "width", G_TYPE_INT, 640,
      "height", G_TYPE_INT, 480,
      "format", G_TYPE_STRING, "I420",
      NULL);
  g_object_set(G_OBJECT(scale_caps_filter_), "caps", scale_caps, NULL);
  gst_caps_unref (scale_caps);
  g_object_set(G_OBJECT(h264enc),
      //Auto change the h264 encoder's target bitrate.
      "threads", 5,
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
      "quantizer", 23,
      "tune", 0x00000004,
      "bitrate", 300,
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

  assert(video_src_);
  assert(video_jitter_buffer_);
  assert(rtp_depay_);
  assert(decoder);
  assert(video_scale_);
  assert(scale_caps_filter_);
  assert(h264enc);
  assert(video_rate);
  assert(h264parse);

  gst_bin_add_many(GST_BIN(pipeline_),video_src_, video_jitter_buffer_, rtp_depay_,
      decoder, video_scale_, scale_caps_filter_, video_rate, h264enc, h264parse, NULL);
  gst_element_link_many(video_src_, video_jitter_buffer_, rtp_depay_, decoder,
      video_scale_, scale_caps_filter_, video_rate, h264enc, h264parse, NULL);

  mux_caps = gst_caps_new_simple("video/x-h264",
      "profile", G_TYPE_STRING, "baseline",
      "stream_format", G_TYPE_STRING, "avc",
      "alignment",  G_TYPE_STRING, "au",
      NULL);
  gst_element_link_filtered(h264parse, muxer_, mux_caps);


  if(caps != NULL) {
    gst_caps_unref (caps);
  }
  if(mux_caps != NULL) {
    gst_caps_unref (mux_caps);
  }
}

} /* namespace orbit */
