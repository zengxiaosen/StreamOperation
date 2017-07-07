// Copyright 2015 All Rights Reserved.
// Author: eric@orangelab.cn (Cheng Xu)

#include "videomixer_pipeline.h"

#include <gstreamer-1.5/gst/gst.h>
#include "glog/logging.h"
#include "gflags/gflags.h"
#include <gst/sdp/gstsdpmessage.h>
#include <commons/kmselementpadtype.h>
#include <gst/check/gstcheck.h>
#include <webrtcendpoint/kmsicecandidate.h>
#include "stream_service/stream_service.grpc.pb.h"
#include <map>
#include <queue>
#include <sstream>

#define KMS_VIDEO_PREFIX "video_src_"
#define KMS_AUDIO_PREFIX "audio_src_"

DECLARE_string(stun_server_ip);
DEFINE_bool(experimental_rtmp, false,
            "experimental with rtmp push.");
DEFINE_string(rtmp_location, "rtmp://turnserver.intviu.cn:1935/live/livestream/5",
              "specify the rtmp push location.");

namespace olive {
using namespace google::protobuf;
using std::map;
using std::queue;
using std::string;
using std::vector;
// Anonymous namespace
namespace {

typedef struct _VideoMixer {
  //TODO(QingyongZhang) lock count before resize it.
  GstElement *video_mixer;
  GstElement *encoder;
  GstElement *tee;
  int count = 0;
} VideoMixer;

typedef struct _AudioMixer {
  GstElement *audio_mixer;
  GstElement *audio_tee;
  GstElement *caps_filter;
  GstElement *opus_enc;
} AudioMixer;

typedef struct OnIceCandidateData {
  videomixer::Internal* klass;
  gchar *peer_sess_id;
  int32 stream_id;
} OnIceCandidateData;

typedef struct _KmsConnectData {
  /*Pads*/
  GstElement *pipeline;
  GstElement *endpoint;
  gboolean video_sink_linked = FALSE;
  gboolean audio_sink_linked = FALSE;
  VideoMixer* mixer;
  AudioMixer* audio_mixer;
} KmsConnectData;

static void kms_destroy_stream_data(gpointer data) {
  g_slice_free(KmsConnectData, data);
}

vector<AudioMixer*> mixers;
static void bus_msg(GstBus * bus, GstMessage * msg, gpointer pipe) {
  LOG(INFO)<< "-------------------------------------------------------------";
  LOG(INFO) << "bus_msg msg = "<<GST_MESSAGE_TYPE (msg);
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR: {
      GST_ERROR ("Error: %" GST_PTR_FORMAT, msg);
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe),
          GST_DEBUG_GRAPH_SHOW_ALL, "error");
      //fail ("Error received on bus");
      LOG(INFO) << "Error received on bus";
      break;
    }
    case GST_MESSAGE_WARNING: {
      GST_WARNING ("Warning: %" GST_PTR_FORMAT, msg);
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe),
          GST_DEBUG_GRAPH_SHOW_ALL, "warning");
      break;
    }
    default:
    break;
  }
}

static GstPad* tee_request_src_pad(GstElement *tee) {
  GstPadTemplate *tee_src_pad_template = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(tee), "src_%u");
  GstPad *tee_src_pad = gst_element_request_pad(tee, tee_src_pad_template, NULL, NULL);
  gst_object_unref(tee_src_pad_template);
  return tee_src_pad;
}

static GstPad* mixer_request_sink_pad(GstElement *mixer) {
  GstPadTemplate *mixer_sink_pad_template = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(mixer), "sink_%u");
  GstPad *mixer_sink_pad = gst_element_request_pad(mixer, mixer_sink_pad_template, NULL, NULL);
  gst_object_unref(mixer_sink_pad_template);
  return mixer_sink_pad;
}

static void connect_video_src_with_mixer(GstElement *pipeline, VideoMixer *mixer, GstPad*srcPad) {

  //mixer->count = 0;
  LOG(INFO)<< "=============================================================";
  LOG(INFO)<<"use mixer address======"<<(int64)mixer;
  int count = mixer->count;
  LOG(INFO)<< "=============================Count = ================================"<<count;
  //TODO(QingyongZhang) lock count when resize it;
  GstPad* pad = mixer_request_sink_pad(mixer->video_mixer);
  if (!pad) {
    //TODO(QingyongZhang) error
    return;
  }

  GstElement *videobox = gst_element_factory_make("videobox", NULL);
  GstPad* boxSrcPad = gst_element_get_static_pad(videobox, "src");

  int heidht = count == 0?400:100;
  int width = count == 0?400:100;
  GstCaps *filtercaps = gst_caps_new_simple("video/x-raw", "width", G_TYPE_INT, width, "height", G_TYPE_INT, heidht,
      "framerate", GST_TYPE_FRACTION, 15, 1, "bpp", G_TYPE_INT, 16, "depth", G_TYPE_INT, 16, "endianness",
      G_TYPE_INT, G_BYTE_ORDER,NULL);
  GstElement *filter = gst_element_factory_make("capsfilter", NULL);
  g_object_set(G_OBJECT(filter), "caps", filtercaps, NULL);
  gst_caps_unref(filtercaps);
  gst_bin_add_many(GST_BIN(pipeline), videobox, filter, NULL);
  gst_element_link(filter, videobox);
  g_object_set(G_OBJECT(pad), "xpos", count==0?0:280, NULL);
  g_object_set(G_OBJECT(pad), "ypos", count ==0? 0:(20+120 * (count-1)), NULL);

  GstPadLinkReturn linkResult = gst_pad_link_full(boxSrcPad, pad, GST_PAD_LINK_CHECK_NOTHING);
  LOG(INFO)<< "connect_src_with_mixer...video mixer=" << linkResult;
  GstPad* filterSinkPad = gst_element_get_static_pad(filter, "sink");
  linkResult = gst_pad_link_full(srcPad, filterSinkPad, GST_PAD_LINK_CHECK_NOTHING);
  LOG(INFO)<< "connect_src_with_mixer...webrtc src=" << linkResult;
  gst_element_sync_state_with_parent(filter);
  gst_element_sync_state_with_parent(videobox);

  LOG(INFO)<< "VIDEO:connect_src_with_mixer...result=" << linkResult;
  mixer->count = ++count;
  LOG(INFO)<< "=============================Count = ================================"<<mixer->count;
}

static void sync_audio_mixer(AudioMixer * mixer) {
  gboolean result = gst_element_sync_state_with_parent(mixer->audio_tee);
  LOG(INFO)<< "==========================================================================================audio tee"<<result;
  result = gst_element_sync_state_with_parent(mixer->audio_mixer);
  LOG(INFO)<< "==========================================================================================audio mixer"<<result;
  result = gst_element_sync_state_with_parent(mixer->opus_enc);
  LOG(INFO)<< "==========================================================================================audio enc"<<result;
}

static void connect_tee_with_other_mixers(AudioMixer* mixer) {
  //TODO Synchronized lock vector when connecting with mixers(QingyongZhang)
  LOG(INFO)<<"Connect tee with other mixer";
  vector<AudioMixer*>::iterator iterator;
  LOG(INFO)<<1;
  int index = 0;
  GstElement* tee = mixer->audio_tee;
  gboolean contains = false;
  for (iterator = mixers.begin(); iterator != mixers.end(); iterator++) {
    AudioMixer* tmpMixer = *iterator;
    if(tmpMixer == mixer) {
      contains = true;
      LOG(INFO)<<"contains";
      continue;
    }
    else {
      GstPad* teeSrcPad = tee_request_src_pad(tee);
      LOG(INFO)<<222;
      if (teeSrcPad) {
        LOG(INFO)<<3;
        GstPad* mixSinkPad = mixer_request_sink_pad(tmpMixer->audio_mixer);
        if (mixSinkPad) {
          LOG(INFO)<<4;
          GstPadLinkReturn linkResult = gst_pad_link(teeSrcPad, mixSinkPad);
          LOG(INFO)<<"index = "<<index<<" link result = "<<linkResult;
          gst_object_unref(mixSinkPad);
          sync_audio_mixer(tmpMixer);
        }
        gst_object_unref(teeSrcPad);
      }
      index ++;
    }
  }
  sync_audio_mixer(mixer);
  LOG(INFO)<<5;

  if(!contains) {
    mixers.push_back(mixer);
  }
  LOG(INFO)<< "=============================================================";

}
static void connect_mixer_with_other_tees(AudioMixer *mixer) {
  //TODO Synchronized lock vector when connecting with mixers(QingyongZhang)
  LOG(INFO)<<"Connect mixer with other tee";
  vector<AudioMixer*>::iterator iterator;
  int index = 0;
  LOG(INFO)<<1;
  gboolean contains = false;
  for (iterator = mixers.begin(); iterator != mixers.end(); iterator++) {
    LOG(INFO)<<2;
    AudioMixer* destMixer = *iterator;
    if(destMixer == mixer) {
      contains = true;
      LOG(INFO)<<"contains";
      continue;
    }
    GstPad* mixSinkPad = mixer_request_sink_pad(mixer->audio_mixer);
    LOG(INFO)<<22;
    if (mixSinkPad) {
      LOG(INFO)<<3;
      GstPad* teeSrcPad = tee_request_src_pad(destMixer->audio_tee);
      if (teeSrcPad) {
        GstPadLinkReturn linkResult = gst_pad_link(teeSrcPad, mixSinkPad);
        LOG(INFO)<<"index = "<<index<<" link result = "<<linkResult;
        LOG(INFO)<<4;
        sync_audio_mixer(destMixer);
        gst_object_unref(teeSrcPad);
      }
      gst_object_unref(mixSinkPad);
    }
    index ++;
    LOG(INFO)<<5;
  }
  sync_audio_mixer(mixer);
  if(!contains) {
    mixers.push_back(mixer);
  }
  LOG(INFO)<< "=============================================================";

}

static void connect_audio_src_with_mixer(GstElement *pipeline, AudioMixer *mixer, GstPad*srcPad) {
  LOG(INFO)<< "0000000000000000000000000000000000000000000000000000000000000000";
  GstPad * sinkPad = gst_element_get_static_pad(mixer->caps_filter,"sink");

  if( srcPad!= NULL &&sinkPad !=NULL) {
    GstPadLinkReturn result = gst_pad_link(srcPad, sinkPad);
    LOG(INFO)<< "AUDIO : connect_audio_src_with_mixer...audio mixer=" << result;
  }
  connect_tee_with_other_mixers(mixer);
  gst_element_sync_state_with_parent(mixer->caps_filter);
  sync_audio_mixer(mixer);
}

static void connect_audio_mixer_with_sink(GstElement *pipeline, AudioMixer *mixer, GstPad*sinkPad) {
  LOG(INFO)<< "0000000000000000000000000000000000000000000000000000000000000000";
//  GstPad *teeSrcPad = tee_request_src_pad(mixer->audio_tee);
  GstPad * srcPad = gst_element_get_static_pad(mixer->opus_enc,"src");
  if ( sinkPad != NULL && srcPad !=NULL) {
    GstPadLinkReturn result = gst_pad_link(srcPad, sinkPad);
    LOG(INFO)<< "AUDIO : connect_audio_mixer_with_sink...audio mixer=" << result;
  }
  connect_mixer_with_other_tees(mixer);
  gst_element_sync_state_with_parent(mixer->audio_tee);
  gst_element_sync_state_with_parent(mixer->caps_filter);
  gst_element_sync_state_with_parent(mixer->audio_mixer);
  gst_element_sync_state_with_parent(mixer->opus_enc);
  gst_element_set_state(pipeline, GST_STATE_PLAYING);
}

static void connect_video_mixer_with_sink(GstElement *pipeline, VideoMixer *mixer, GstPad*sinkPad) {
  GstPad* pad = tee_request_src_pad(mixer->tee);
  if (!pad) {
    //TODO(QingyongZhang) error
    return;
  }
  GstPadLinkReturn linResult = gst_pad_link_full(pad, sinkPad, GST_PAD_LINK_CHECK_NOTHING);
  gst_object_unref(pad);
  LOG(INFO)<< "=============================================================";
  LOG(INFO)<< "VIDEO:connect_video_mixer_with_sink...result=" << linResult;
}

static void connect_sink_with_peer(GstElement * element, GstPad * pad, gpointer user_data) {
  LOG(INFO)<< "=============================================================";
  LOG(INFO) << "connect_sink...pad=" << GST_PAD_NAME (pad);

  KmsConnectData *data = (KmsConnectData*) user_data;

  GST_DEBUG_OBJECT (pad, "New pad %" GST_PTR_FORMAT, element);

  gchar * name = GST_OBJECT_NAME(pad);
  if (g_str_has_prefix (name, "video_src")) {
    connect_video_src_with_mixer(data->pipeline,data->mixer,pad);
  } else if(g_str_has_prefix (name, "sink_video")) {
    connect_video_mixer_with_sink(data->pipeline,data->mixer,pad);
  } else if(g_str_has_prefix(name,"audio_src")) {
    connect_audio_src_with_mixer(data->pipeline,data->audio_mixer,pad);
  } else if(g_str_has_prefix(name,"sink_audio")) {
    connect_audio_mixer_with_sink(data->pipeline,data->audio_mixer,pad);
  }
  gst_element_sync_state_with_parent(element);
}

static GArray * create_codecs_array(gchar * codecs[]) {
  GArray *a = g_array_new(FALSE, TRUE, sizeof(GValue));
  int i;

  for (i = 0; i < g_strv_length(codecs); i++) {
    GValue v = G_VALUE_INIT;
    GstStructure *s;

    g_value_init(&v, GST_TYPE_STRUCTURE);
    s = gst_structure_new(codecs[i], NULL, NULL);
    gst_value_set_structure(&v, s);
    gst_structure_free(s);
    g_array_append_val(a, v);
  }

  return a;
}

}  // namespace annoymous

namespace videomixer {
class Internal {
public:
  typedef struct _StreamData {
    // TODO(chengxu): add a lock to use this class
    int32 stream_id;
    gchar *gst_sess_id;

    GstElement* webrtc_endpoint;
    //queue<string> response_ice_candidates;
    vector<string> ice_candidates;
    queue<IceCandidate> response_ice_candidates;
    gboolean ice_gathering_done;
    KmsConnectData *connect_data;
  } StreamData;
  map<int32, StreamData*> stream_data;
  GstElement* pipeline;
  VideoMixer* mixer;
  Internal() {
    LOG(INFO)<<"==================================";
    LOG(INFO)<<"1.Create pipeline";
    pipeline = gst_pipeline_new(NULL);
    GstClock* clock = gst_system_clock_obtain();
    gst_pipeline_use_clock(GST_PIPELINE(pipeline), clock);
    g_object_unref(clock);

    // TODO for rtmp 
    GstElement *branchtee;
    branchtee = gst_element_factory_make("tee", "branchtee");

    LOG(INFO)<<"2.Create video mixer";
    //Init mixer
    mixer = g_slice_new0(VideoMixer);
    mixer->video_mixer = gst_element_factory_make("videomixer", "videomixer");
    mixer->tee = gst_element_factory_make("tee", NULL);

    GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    gst_bus_add_signal_watch(bus);
    g_signal_connect(bus, "message", G_CALLBACK (bus_msg), pipeline);
    gst_object_unref(bus);

    // Add vp8enc
    GstElement * encoder = gst_element_factory_make("vp8enc", NULL);
    //GstElement * parser = gst_element_factory_make("vp8parser", NULL);
    g_object_set (G_OBJECT (encoder),
        "deadline", G_GINT64_CONSTANT (100000),
        "threads", 1,
        "cpu-used", 10,
        "resize-allowed", TRUE,
        "target-bitrate", 300000,
        "end-usage", /* cbr */1,
        NULL);
    GstElement *videorate = gst_element_factory_make("videorate", "video_rate");
    gst_bin_add_many(GST_BIN(pipeline), mixer->video_mixer, branchtee,encoder, videorate, mixer->tee, NULL);
    gboolean linkResult = gst_element_link_many(mixer->video_mixer, branchtee, videorate, encoder, mixer->tee, NULL);

    // rmtp
    if (FLAGS_experimental_rtmp) {
      GstElement *x264enc, *flvmux, *rtmpsink, *rqueue;
      rqueue = gst_element_factory_make("queue","videoqueueElement");
      x264enc = gst_element_factory_make("x264enc", "x264encElement");
      g_object_set(GST_OBJECT(x264enc), "bitrate" , 256, NULL);
      g_object_set(GST_OBJECT(x264enc), "tune" , 4, NULL);
      flvmux = gst_element_factory_make("flvmux","flvmux");
      rtmpsink = gst_element_factory_make("rtmpsink", "videoSinkElement");
      g_object_set(GST_OBJECT(rtmpsink), "location" , FLAGS_rtmp_location.c_str(), NULL);
      g_object_set(GST_OBJECT(rtmpsink), "sync" , false, NULL);
      g_object_set(GST_OBJECT(rtmpsink), "async" , false, NULL);
      gst_bin_add_many(GST_BIN(pipeline), x264enc,flvmux,rqueue, rtmpsink, NULL);
      
      gst_element_link_many(rqueue, x264enc, flvmux, rtmpsink, NULL);
      
      GstPad *newTeePad1 = tee_request_src_pad(branchtee);
      GstPad *newSinkPad1 = gst_element_get_static_pad(rqueue, "sink");
      gst_pad_link_full(newTeePad1, newSinkPad1, GST_PAD_LINK_CHECK_NOTHING); 
    }
    // end rmtp

    gst_element_set_state(pipeline, GST_STATE_PLAYING);
  }
  void FinishMediaPipeline(int32 stream_id) {
    StreamData* stream_data_item = GetStreamData(stream_id);
    if (stream_data_item == NULL) {
      LOG(ERROR)<< "CreateMediaPipeline: stream_id is invalid.";
      return;
    }

    //GstElement* pipeline = session_data_item->pipeline;
    GstElement* answerer = stream_data_item->webrtc_endpoint;
    gchar* answerer_sess_id = stream_data_item->gst_sess_id;
    gboolean ret;

    g_signal_emit_by_name(answerer, "gather-candidates", answerer_sess_id, &ret);
    if (ret == false) {
      LOG(INFO)<<"gather-candidates fail";
    }
  }

  void CreateMediaPipeline(int32 stream_id) {
    StreamData* stream_data_item = GetStreamData(stream_id);
    if (stream_data_item == NULL) {
      LOG(ERROR) << "CreateMediaPipeline: stream_id is invalid.";
      return;
    }

    LOG(INFO) << "startCreateMediaPipline";
    gchar * audio_codec = "opus/48000/2";
    gchar * video_codec = "VP8/90000";

    gchar *audio_codecs[] = {audio_codec, NULL};
    gchar *video_codecs[] = {video_codec, NULL};

    GstElement* answerer = gst_element_factory_make ("webrtcendpoint", NULL);
    if (!FLAGS_stun_server_ip.empty()) {
      g_object_set (answerer, "stun-server", FLAGS_stun_server_ip.c_str(), NULL);
      LOG(INFO) << "add stun-server:" << FLAGS_stun_server_ip;
    }
    LOG(INFO) << "startCreateMediaPipline"<<"webrtcendpoint";

    GArray *audio_codecs_array = create_codecs_array (audio_codecs);
    GArray *video_codecs_array = create_codecs_array (video_codecs);
    g_object_set (answerer, "num-audio-medias", 1, "audio-codecs",
        g_array_ref (audio_codecs_array), "num-video-medias", 1, "video-codecs",
        g_array_ref (video_codecs_array), NULL);
    g_array_unref (audio_codecs_array);
    g_array_unref (video_codecs_array);

    LOG(INFO) << "create-session";

    gchar *answerer_sess_id;
    /* Session creation */
    g_signal_emit_by_name (answerer, "create-session", &answerer_sess_id);
    GST_DEBUG_OBJECT (answerer, "Created session with id '%s'", answerer_sess_id);

    /* Trickle ICE management */
    OnIceCandidateData* ice_cand_data = new OnIceCandidateData;
    ice_cand_data->klass = this;
    ice_cand_data->peer_sess_id = answerer_sess_id;
    ice_cand_data->stream_id = stream_id;
    LOG(INFO) << " stream_id=" << stream_id;
    g_signal_connect (G_OBJECT (answerer), "on-ice-candidate", G_CALLBACK(on_ice_candidate_static), ice_cand_data);
    g_signal_connect (G_OBJECT (answerer), "on-ice-gathering-done", G_CALLBACK(on_ice_gathering_done_static), ice_cand_data);

    /*Create audio mixer*/
    AudioMixer *audio_mixer = g_slice_new0(AudioMixer);
    audio_mixer->caps_filter = gst_element_factory_make("capsfilter", NULL);
    audio_mixer->audio_mixer = gst_element_factory_make("audiomixer", NULL);
    audio_mixer->opus_enc = gst_element_factory_make("opusenc", NULL);;
    audio_mixer->audio_tee = gst_element_factory_make("tee", NULL);
    GstCaps*caps = gst_caps_new_simple ("audio/x-raw", "format", G_TYPE_STRING, "S16LE",
        "rate", G_TYPE_INT, 48000, "channels", G_TYPE_INT, 2, NULL);

    g_object_set (G_OBJECT (audio_mixer->caps_filter ), "caps", caps, NULL);
    /* Add elements */
    gst_bin_add (GST_BIN (pipeline), answerer);

    gst_bin_add_many (GST_BIN (pipeline), audio_mixer->audio_tee , audio_mixer->audio_mixer ,audio_mixer->opus_enc,audio_mixer->caps_filter,NULL);
    gst_element_sync_state_with_parent(audio_mixer->caps_filter);
    gst_element_sync_state_with_parent(audio_mixer->audio_mixer);
    gst_element_sync_state_with_parent(audio_mixer->opus_enc);
    gst_element_sync_state_with_parent(audio_mixer->audio_tee);

    GstPad* capsPad = gst_element_get_static_pad(audio_mixer->caps_filter,"src");
    GstPad* teeSinPad = gst_element_get_static_pad(audio_mixer->audio_tee,"sink");

    GstPadLinkReturn linkCaps =gst_pad_link(capsPad,teeSinPad);
    LOG(INFO)<<"^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^"<<linkCaps;
//    GstPad* teeSrc = tee_request_src_pad(audio_mixer->audio_tee);
//    GstPad* mixerPad = mixer_request_sink_pad(audio_mixer->audio_mixer);
//    GstPadLinkReturn link = gst_pad_link(teeSrc,mixerPad);
    gst_element_link(audio_mixer->audio_mixer,audio_mixer->opus_enc);
    stream_data_item->connect_data = g_slice_new0(KmsConnectData);
    stream_data_item->connect_data->endpoint = answerer;
    stream_data_item->connect_data->pipeline = pipeline;
    stream_data_item->connect_data->mixer = this->mixer;
    stream_data_item->connect_data->audio_mixer=audio_mixer;
    g_signal_connect_data (answerer, "pad_added", G_CALLBACK (connect_sink_with_peer), stream_data_item->connect_data,
        (GClosureNotify) kms_destroy_stream_data, G_CONNECT_AFTER);
    gst_element_sync_state_with_parent(answerer);
    /* Request new pad */
    kms_element_request_srcpad (answerer, KMS_ELEMENT_PAD_TYPE_AUDIO);
    kms_element_request_srcpad (answerer, KMS_ELEMENT_PAD_TYPE_VIDEO);

    LOG(INFO) << "stream_data insert.";

    stream_data_item->webrtc_endpoint = answerer;
    stream_data_item->gst_sess_id = answerer_sess_id;
  }

  void ReceiveIceCandidates(int stream_id, vector<IceCandidate>* ice_candidates) {
    StreamData* stream_data_item = GetStreamData(stream_id);

      /*
    while (!stream_data_item->response_ice_candidates.empty()) {
      string ice_candidate_str = stream_data_item->response_ice_candidates.front();
      //LOG(INFO) << "ice_candidate_str" << ice_candidate_str;
      ice_candidates->push_back(ice_candidate_str);
      stream_data_item->response_ice_candidates.pop();
    }*/
      
      
      while(!stream_data_item->response_ice_candidates.empty()) {
          IceCandidate cand = stream_data_item->response_ice_candidates.front();
          ice_candidates->push_back(cand);
          //LOG(INFO) << "@@@@@@@@@@ice_candidate_str" << cand.ice_candidate();
          stream_data_item->response_ice_candidates.pop();
      }
  }

  int CreateStream() {
    // TODO(chengxu): Add the session management code here.
    int stream_id = rand();
    StreamData *data = GetStreamData(stream_id);
    while(data) {
      stream_id = rand();
      data = GetStreamData(stream_id);
    }
    StreamData* new_stream_data_item = new StreamData;
    LOG(INFO) << "createStream" << " stream_id=" << stream_id;
    LOG(INFO) << "total stream=" << stream_data.size();
    new_stream_data_item->stream_id = stream_id;
    stream_data.insert(
        std::pair<int32, StreamData*>(stream_id, new_stream_data_item));
    LOG(INFO) << "BeforeCreateMediapipeline";
    CreateMediaPipeline(stream_id);
    return stream_id;
  }

  bool CloseStream(int stream_id) {
    LOG(INFO) << "CloseStream()";
    return true;
  }

  bool ProcessSdpOffer(int stream_id, const string& sdp_offer,
      string* sdp_answer) {
    StreamData* stream_data_item = GetStreamData(stream_id);
    if (stream_data_item == NULL) {
      LOG(ERROR) << "ProcessSdpOffer: stream_id is invalid.";
      return false;
    }

    GstElement* answerer = stream_data_item->webrtc_endpoint;
    gchar* answerer_sess_id = stream_data_item->gst_sess_id;

    GstSDPMessage *offer_sdp_message;
    if (!gst_sdp_message_new (&offer_sdp_message) == GST_SDP_OK) {
      LOG(FATAL) << "gst_sdp_message_new(answer_sdp_message) fails.";
    }
    if (gst_sdp_message_parse_buffer((const guint8 *)
            sdp_offer.c_str(), -1,
            offer_sdp_message) != GST_SDP_OK) {
      LOG(INFO)<<"==================================";
      LOG(INFO) << "gst_sdp_message_parse_buffer(sdp_offer) fails.";
      LOG(INFO)<<"==================================";
    } else {
    }

    GstSDPMessage *answer;
    g_signal_emit_by_name (answerer, "process-offer",
        answerer_sess_id, offer_sdp_message,
        &answer);
    if (answer == NULL) {
      LOG(ERROR) << "process-offer fails.";
      return false;
    }
    gchar* answer_sdp_str = gst_sdp_message_as_text (answer);
    *sdp_answer = answer_sdp_str;
    FinishMediaPipeline(stream_id);
    return true;
  }

  bool AddIceCandidate(int stream_id, const IceCandidate& ice_candidate) {
    //LOG(INFO)<<"Add ice candidate";
    StreamData* stream_data_item = GetStreamData(stream_id);
    if (stream_data_item == NULL) {
      LOG(ERROR) << "AddIceCandidate: stream_id is invalid.";
      return false;
    }

    GstElement* answerer = stream_data_item->webrtc_endpoint;
    gchar* answerer_sess_id = stream_data_item->gst_sess_id;
    guint8 index = (guint8)ice_candidate.sdp_m_line_index();

    KmsIceCandidate * candidate = kms_ice_candidate_new (ice_candidate.ice_candidate().c_str(),
							 ice_candidate.sdp_mid().c_str(),index, NULL);
    if(candidate == NULL) {
      LOG(INFO)<<"-----------------------Create ice candidate error--------------------------------";
      return false;
    } else {
      gboolean ret;
      g_signal_emit_by_name (answerer, "add-ice-candidate", answerer_sess_id,
          candidate, &ret);
      if (ret == NULL) {
        LOG(ERROR) << "add-ice-candidate(" << answerer_sess_id << ") failed.";
        return false;
      }
    }

    return true;
  }

private:
  static void on_ice_gathering_done_static (GstElement *webrtcendpoint,
      gpointer gdata,
      OnIceCandidateData * data) {
    //OnIceCandidateData* data = (OnIceCandidateData*)gdata;
    videomixer::Internal* internal = data->klass;
    internal->on_ice_gathering_done(webrtcendpoint, gdata, data);
  }

  void on_ice_gathering_done (GstElement *webrtcendpoint,
      gpointer gdata,
      OnIceCandidateData* data) {
    //OnIceCandidateData* data = (OnIceCandidateData*)gdata;
    int stream_id = data->stream_id;
    LOG(INFO) << "stream_id=" << stream_id;
    StreamData* stream_data_item = GetStreamData(stream_id);
    if (stream_data_item == NULL) {
      LOG(ERROR) << "CreateMediaPipeline: stream_id is invalid.";
      return;
    }

    stream_data_item->ice_gathering_done = true;
  }

  static void on_ice_candidate_static (GstElement * self, gchar * sess_id,
      KmsIceCandidate * candidate,
      OnIceCandidateData * data) {
    videomixer::Internal* internal = data->klass;
    internal->on_ice_candidate(self, sess_id, candidate, data);
  }

  void on_ice_candidate (GstElement * self, gchar * sess_id,
      KmsIceCandidate * candidate, OnIceCandidateData * data)
  {
    // Send the ice canddiate to someone.
    int32 stream_id = data->stream_id;

    StreamData* stream_data_item = GetStreamData(stream_id);
    if (stream_data_item == NULL) {
      LOG(ERROR) << "OnICE_Candidate: stream_id " << "(" << stream_id << ") is invalid.";
      return;
    }

    //gchar* ice_candidate_str = kms_ice_candidate_get_candidate(candidate);
      
      /*
      stringstream candidateStrBuffer;
      candidateStrBuffer << kms_ice_candidate_get_candidate(candidate) << "|";
      candidateStrBuffer << kms_ice_candidate_get_sdp_mid(candidate) << "|";
      candidateStrBuffer << (int)kms_ice_candidate_get_sdp_m_line_index(candidate);
      string ice_candidate_str = candidateStrBuffer.str();
      candidateStrBuffer.str("");*/
      
      IceCandidate tmpCandidate;// = new IceCandidate;
      tmpCandidate.set_ice_candidate(kms_ice_candidate_get_candidate(candidate));
      tmpCandidate.set_sdp_mid(kms_ice_candidate_get_sdp_mid(candidate));
      tmpCandidate.set_sdp_m_line_index(kms_ice_candidate_get_sdp_m_line_index(candidate));
      
      
    //stream_data_item->ice_candidates.push_back(ice_candidate_str);
    //stream_data_item->response_ice_candidates.push(ice_candidate_str);
      //LOG(INFO) << "add_ice_candidate: " << tmpCandidate.ice_candidate();
      stream_data_item->response_ice_candidates.push(tmpCandidate);
  }

  gboolean kms_element_request_srcpad (GstElement * src, KmsElementPadType pad_type) {
    gchar *padname;

    g_signal_emit_by_name (src, "request-new-pad", pad_type, NULL, GST_PAD_SRC,
			   &padname);
    if (padname == NULL) {
      return FALSE;
    }

    GST_DEBUG_OBJECT (src, "Requested pad %s", padname);
    g_free (padname);
    
    return TRUE;
  }

  StreamData* GetStreamData(int32 stream_id) {
    auto stream_it = stream_data.find(stream_id);
    if (stream_it == stream_data.end()) {
      return NULL;
    }
    StreamData* stream_data_item = (*stream_it).second;
    return stream_data_item;
  }

};  // class Internal
}
  // namespace videomixer

VideoMixerPipeline::VideoMixerPipeline() {
  internal_ = new videomixer::Internal;
  LOG(INFO)<< "internal_=" << internal_;
}
VideoMixerPipeline::~VideoMixerPipeline() {
  // TODO(chengxu): release all the memory and resource.
  delete internal_;
}

int VideoMixerPipeline::CreateStream() {
  return internal_->CreateStream();
}
bool VideoMixerPipeline::CloseStream(int stream_id) {
  return internal_->CloseStream(stream_id);
}
bool VideoMixerPipeline::AddIceCandidate(int stream_id, const IceCandidate& ice_candidate) {
  return internal_->AddIceCandidate(stream_id, ice_candidate);
}
bool VideoMixerPipeline::ProcessSdpOffer(int stream_id, const string& sdp_offer, string* sdp_answer) {
  return internal_->ProcessSdpOffer(stream_id, sdp_offer, sdp_answer);
}
void VideoMixerPipeline::ReceiveIceCandidates(int stream_id, vector<IceCandidate>* ice_candidates) {
  internal_->ReceiveIceCandidates(stream_id, ice_candidates);
}

}  // namespace olive
