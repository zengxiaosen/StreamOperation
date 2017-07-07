/*
 * video_mixer_plugin.cc
 *
 *  Created on: Mar 4, 2016
 *      Author: vellee
 */


#include "video_mixer_plugin.h"
#include "video_mixer_room.h"

#include <boost/thread/mutex.hpp>

#include "stream_service/orbit/rtp/janus_rtcp_processor.h"
#include "stream_service/orbit/live_stream/common_defines.h"

// Queue
#include "stream_service/orbit/rtp/rtp_packet_queue.h"
#include "stream_service/orbit/rtp/rtp_headers.h"

// For gstraemer and related.
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <glib-2.0/glib.h>


#include "glog/logging.h"
#include "gflags/gflags.h"

// NOTE(chengxu): the rtp header's base size is definitely 12 bytes.
// But if the rtp header has extension or something, the rtp header
// size may vary. This is a serious warning -
// For examples:
// if the SDP has a line like: a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level
// it will indicate that the rtp header may have a extension of audio-level,
// in this case the rtp header size will be 20 instead of 12.
#define RTP_HEADER_BASE_SIZE 12

#define USE_FEC     0
#define DEFAULT_COMPLEXITY  8
#define BUFFER_SAMPLES  8000

#define VIDEO_PREBUFFERING_SIZE 25

// For video mixer.
#define VLOG_LEVEL 2
#define MIXER_SINK_PAD_TEMPLETE "sink_%u"

DECLARE_int32(video_width);
DECLARE_int32(video_height);
DECLARE_bool(save_dot_file);

namespace orbit {
using namespace std;
  VideoMixerPlugin::VideoMixerPlugin(int stream_id, int video_width, int video_height) {
    stream_id_ = stream_id;
    if(video_width == 0 || video_height == 0){
      limit_frame_size_ = false;
    } else {
      limit_frame_size_ = true;
      video_width_ = video_width;
      video_height_ = video_height;
    }
    if (!Init()) {
      LOG(FATAL) << "Create the audioRoom, init failed.";
    }
  }

  bool VideoMixerPlugin::Init() {
    return true;
  }

  void VideoMixerPlugin::Release() {
  }

  void VideoMixerPlugin::RelayMediaOutputPacket(std::shared_ptr<MediaOutputPacket> packet,
                                                packetType packet_type) {
    if (packet->encoded_buf == NULL) {
      LOG(ERROR) << "OutputPacket is invalid.";
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
      rtp_header.setPayloadType(VP8_90000_PT);
    } else {
      rtp_header.setPayloadType(OPUS_48000_PT);
    }

    memcpy(&(p.data[0]), &rtp_header, RTP_HEADER_BASE_SIZE);
    memcpy(&(p.data[RTP_HEADER_BASE_SIZE]), packet->encoded_buf, packet->length);
    p.length = packet->length + RTP_HEADER_BASE_SIZE;

    VLOG(4) << "stream " << stream_id() << "'s energy:" << packet->audio_energy;


    // Call the gateway to relay the RTP packet to the participant.
    RelayRtpPacket(p);
    VLOG(4) << "relay to gateway succeed...";
  }

  void VideoMixerPlugin::Stop(){
    stoped_ = true;
  }

  void VideoMixerPlugin::IncomingRtpPacket(const dataPacket& packet) {
    VLOG(3) <<"Incoming RTP packet : Type = "<<packet.type << " "<<is_stoped();
  }

  void VideoMixerPlugin::SendFirPacket() {
    // Send another FIR/PLI packet to the sender.
    /* Send a FIR to the new RTP forward publisher */
    char buf[20];
    memset(buf, 0, 20);
    int len = janus_rtcp_fir((char *)&buf, 20, &fir_index_);

    dataPacket p;
    p.comp = 0;
    p.type = VIDEO_PACKET;
    p.length = len;
    memcpy(&(p.data[0]), buf, len);

    RelayRtcpPacket(p);
    VLOG(4) << "Send RTCP...FIR....";

    /* Send a PLI too, just in case... */
    memset(buf, 0, 12);
    len = janus_rtcp_pli((char *)&buf, 12);

    p.length = len;
    memcpy(&(p.data[0]), buf, len);
    RelayRtcpPacket(p);
    VLOG(4) << "Send RTCP...PLI....";
  }

  void VideoMixerPlugin::OnAudioMixed(const std::shared_ptr<MediaOutputPacket> packet){
    if(!stoped_) {
      RelayMediaOutputPacket(packet, AUDIO_PACKET);
    }
  }

  void VideoMixerPlugin::IncomingRtcpPacket(const dataPacket& packet) {
    TransportPlugin::IncomingRtcpPacket(packet);
    //RelayRtcpPacket(packet);
    if (packet.type == AUDIO_PACKET) {
      // Skip for now
    } else if (packet.type == VIDEO_PACKET) {
      // Skip for now as well.
    }
  }


  // Annoymous namespace
  namespace {
    static void video_need_data(GstElement* object, guint arg0, gpointer user_data) {
      //Wait until rtp packet filled
      VideoMixerPluginImpl* video_mixer_plugin_impl = (VideoMixerPluginImpl*) user_data;
      VLOG(4) << "Need data stream id = " << video_mixer_plugin_impl->stream_id();
      while (true) {
        if(video_mixer_plugin_impl->is_stoped()){
          return;
        }
        std::shared_ptr<orbit::dataPacket> rtp_packet = video_mixer_plugin_impl->PopVideoPacket();
        if (rtp_packet) {
          int size = rtp_packet->length;
          char *data = (char *)malloc(size);
          memcpy(data, rtp_packet->data, size);
          GstBuffer *buffer = gst_buffer_new_wrapped(data, size);
          
          GstFlowReturn ret = GST_FLOW_CUSTOM_SUCCESS_1;
          g_signal_emit_by_name(video_mixer_plugin_impl->GetVideoSrc(), "push-buffer", buffer, &ret);
          gst_buffer_unref(buffer);
          if (ret != GST_FLOW_OK) {
            LOG(ERROR)<< "Push_buffer wrong....";
          }
          break;
        } else {
          usleep(5000);
        }
      }
    }
    
    static int remove_element(gpointer user_data){
      VideoMixerPluginImpl* plugin = (VideoMixerPluginImpl*)user_data;
      plugin->RemoveElements();
      return 1;
    }
    static GstPadProbeReturn
    event_probe_cb (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
    {
      VLOG(VLOG_LEVEL)<<"-----------------------event_probe_cb-------------------";
      VideoMixerPluginImpl* plugin = (VideoMixerPluginImpl*)user_data;
      if (GST_EVENT_TYPE (GST_PAD_PROBE_INFO_DATA (info)) != GST_EVENT_EOS)
        return GST_PAD_PROBE_PASS;
      gst_pad_remove_probe (pad, GST_PAD_PROBE_INFO_ID (info));
      gst_pad_send_event (plugin->GetMixerSinkPad(), gst_event_new_eos ());
      g_idle_add(remove_element,user_data);
      return GST_PAD_PROBE_OK;
    }
  }  // annoymous namespace

  /**
   * -------------------------------------------------------------------------------------
   * VideoMixerPluginImpl functions
   * -------------------------------------------------------------------------------------
   */
  VideoMixerPluginImpl::VideoMixerPluginImpl(std::weak_ptr<Room> room, int stream_id,int video_width, int video_height) :
    VideoMixerPlugin(stream_id, video_width, video_height) {
    room_ = room;
  }

  void VideoMixerPluginImpl::OnTransportStateChange(TransportState state) {
    if(state == TRANSPORT_READY) {
      if (app_src_ == NULL) {
        auto mixer_room = room_.lock();
        if (mixer_room) {
          if (mixer_room.get() != NULL) {
            ((VideoMixerRoom*)mixer_room.get())->OnTransportStateChange(stream_id(), state);
          }
          GstCaps *caps;
          GstElement* pipeline = ((VideoMixerRoom*)mixer_room.get())->GetMixerPipeline();
          GstElement* video_mixer = ((VideoMixerRoom*)mixer_room.get())->GetVideoMixer();
          VLOG(VLOG_LEVEL)<<"-----------------------LinkElement-------------------";
          /*
           * Init appsrc element
           */
          app_src_ = gst_element_factory_make("appsrc", NULL);
          caps = gst_caps_new_simple("application/x-rtp", "media", G_TYPE_STRING, "video", "clock-rate", G_TYPE_INT,
                                     90000, "payload", G_TYPE_INT, VP8_90000_PT, "encoding-name", G_TYPE_STRING, "VP8", NULL);
          g_object_set(G_OBJECT(app_src_), "caps", caps, NULL);
          g_object_set(G_OBJECT(app_src_),
                       "stream-type", 0,
                       "format", GST_FORMAT_TIME,
                       //        "min-latency", gint64(0),
                       //        "max-latency", gint64(0),
                       "do-timestamp", false,
                       "is-live", true,NULL);
          g_signal_connect(app_src_, "need-data", G_CALLBACK(video_need_data), this);

          rtp_vp8_depay_ = gst_element_factory_make("rtpvp8depay", NULL);
          vp8_dec_ = gst_element_factory_make("vp8dec", NULL);
          g_object_set(G_OBJECT(vp8_dec_),"threads",3,
                       "post-processing" , true,
                       "deblocking-level", 3,
                       "noise-level", 0,
                       "post-processing-flags", 0x00000403,
                       NULL);

          /*
           * Init jitter buffer
           */
          jitter_buffer_ = gst_element_factory_make("rtpjitterbuffer", NULL);
          //    g_object_set(G_OBJECT(jitter_buffer_), "latency", 600, "drop-on-latency", true, NULL);

          queue_ = gst_element_factory_make("queue2", NULL);
          video_rate_ = gst_element_factory_make("videorate", NULL);
          g_object_set(G_OBJECT(video_rate_),"drop-only", true,"max-rate","15",NULL);

          gst_bin_add_many(GST_BIN(pipeline), app_src_, jitter_buffer_, rtp_vp8_depay_, vp8_dec_,video_rate_,queue_,
                           NULL);
          gst_element_link_many(app_src_, jitter_buffer_, rtp_vp8_depay_, vp8_dec_,video_rate_,queue_, NULL);

          sink_pad_ = RequestElementPad(video_mixer, "sink_%u");
          g_assert(sink_pad_);
          if(limit_video_size()){
            g_object_set(G_OBJECT(sink_pad_), "xpos", x_, "ypos", y_, "width", FLAGS_video_width, "height",FLAGS_video_height, NULL);
          } else {
            g_object_set(G_OBJECT(sink_pad_), "xpos", x_, "ypos", y_, NULL);
          }
          //    g_object_set(G_OBJECT(sink_pad_), "xpos", position_->x(), "ypos", position_->y(), NULL);
          GstPad* queue_src_pad = gst_element_get_static_pad(queue_, "src");
          GstPadLinkReturn linkResult = gst_pad_link_full(queue_src_pad, sink_pad_, GST_PAD_LINK_CHECK_DEFAULT);

          VLOG(VLOG_LEVEL)<<"-----------------------LinkResult-------------------"<<linkResult;
          gst_object_unref(queue_src_pad);

          //g_signal_connect (app_src_, "enough-data", G_CALLBACK(enough_data), this);
          gst_element_sync_state_with_parent(app_src_);
          gst_element_sync_state_with_parent(rtp_vp8_depay_);
          gst_element_sync_state_with_parent(vp8_dec_);
          gst_element_sync_state_with_parent(jitter_buffer_);
          ((VideoMixerRoom*)mixer_room.get())->SyncElements();
          gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PLAYING);
          if(FLAGS_save_dot_file){
            GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(pipeline), GST_DEBUG_GRAPH_SHOW_VERBOSE, "video_mixer_add_participant");
          }

          if(caps != NULL) {
            gst_caps_unref (caps);
          }
        }
      }
    }
  }

  void VideoMixerPluginImpl::IncomingRtpPacket(const dataPacket& packet) {
    VLOG(VLOG_LEVEL)<<"-----------------------IncomingRtpPacket-------------------";
    if(is_stoped()){
      return;
    }
    if (packet.type == AUDIO_PACKET) {
      auto mixer_room = room_.lock();
      if(mixer_room) {
        mixer_room->IncomingRtpPacket(stream_id(), packet);
      }
    } else if (packet.type == VIDEO_PACKET) {
      video_queue_.pushPacket((const char *)&(packet.data[0]), packet.length);
    }
  }

  void VideoMixerPluginImpl::UnlinkElements() {
    VideoMixerPlugin::Stop();
    VLOG(VLOG_LEVEL)<<"-----------------------UnlinkElements-------------------";
    gst_pad_add_probe (sink_pad_, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, event_probe_cb, this, NULL);
    gst_app_src_end_of_stream((GstAppSrc*)app_src_);
    RemoveElements();
  }

  std::shared_ptr<dataPacket> VideoMixerPluginImpl::PopVideoPacket() {
    RtpPacketQueue* inbuf = &video_queue_;

    //Here we cache 30 rtp packet.
    if (inbuf->getSize() < VIDEO_PREBUFFERING_SIZE) {
      return NULL;
    } else {
      std::shared_ptr<orbit::dataPacket> rtp_packet = inbuf->popPacket(true, true);
      if(rtp_packet) {
      const RtpHeader *header = reinterpret_cast<const RtpHeader*>(rtp_packet->data);
      int seq = (int)header->getSeqNumber();
      VLOG(2)<<"-------------------seq ="<<seq << " size is "<<inbuf->getSize();
      }
      return rtp_packet;
    }
  }

  void VideoMixerPluginImpl::UpdateVideoPosition(int x, int y,bool imediately) {
    VLOG(VLOG_LEVEL)<<"-----------------------UpdateVideoPosition-------------------";
    x_ = x;
    y_ = y;
    if(app_src_ != NULL && sink_pad_ != NULL){
      g_object_set(G_OBJECT(sink_pad_), "xpos", x, "ypos", y, NULL);
    } else {
      return;
    }
    auto mixer_room = room_.lock();
    if(imediately && !mixer_room){
      gst_bus_poll (GST_ELEMENT_BUS (((VideoMixerRoom*)mixer_room.get())->GetMixerPipeline()), GST_MESSAGE_ERROR,100*GST_MSECOND);
    }
    if(FLAGS_save_dot_file) {
      GstElement* pipeline = ((VideoMixerRoom*)mixer_room.get())->GetMixerPipeline();
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline), GST_DEBUG_GRAPH_SHOW_VERBOSE, "UpdateVideoPosition");
    }
  }
  
  void VideoMixerPluginImpl::RemoveElements(){
    VLOG(VLOG_LEVEL)<<"-----------------------RemoveElements-------------------";
    auto mixer_room = room_.lock();
    if(!mixer_room) {
      return;
    }
    VideoMixerRoom* room = (VideoMixerRoom*)mixer_room.get();
    GstElement* pipeline = room->GetMixerPipeline();
    GstBin* bin = GST_BIN (pipeline);
    gst_element_set_state (video_rate_, GST_STATE_NULL);
    gst_bin_remove(bin, video_rate_);
    
    gst_element_set_state (app_src_, GST_STATE_NULL);
    gst_bin_remove (bin, app_src_);
    
    gst_element_set_state (rtp_vp8_depay_, GST_STATE_NULL);
    gst_bin_remove (bin, rtp_vp8_depay_);
    
    gst_element_set_state (vp8_dec_, GST_STATE_NULL);
    gst_bin_remove (bin, vp8_dec_);

    gst_element_set_state (jitter_buffer_, GST_STATE_NULL);
    gst_bin_remove (bin, jitter_buffer_);
    
    gst_element_set_state (queue_, GST_STATE_NULL);
    gst_bin_remove (bin, queue_);
    GstElement* video_mixer = room->GetVideoMixer();
    gst_element_remove_pad(video_mixer, sink_pad_);
    //  gst_object_unref(sink_pad_);
    room->OnPluginRemoved(stream_id());

    if(FLAGS_save_dot_file){
      GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(pipeline), GST_DEBUG_GRAPH_SHOW_VERBOSE, "removeelements");
    }
  }
  VideoMixerPluginImpl::~VideoMixerPluginImpl() {
  }
}  // namespace orbit
