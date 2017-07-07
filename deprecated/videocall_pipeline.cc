// Copyright 2015 All Rights Reserved.
// Author: eric@orangelab.cn (Cheng Xu)

#include "videocall_pipeline.h"

#include <gstreamer-1.5/gst/gst.h>
#include "glog/logging.h"

#include <gst/sdp/gstsdpmessage.h>
#include <commons/kmselementpadtype.h>
#include <gst/check/gstcheck.h>
#include <webrtcendpoint/kmsicecandidate.h>
#include "stream_service/stream_service.grpc.pb.h"

#include <map>
#include <queue>

#define KMS_VIDEO_PREFIX "video_src_"
#define KMS_AUDIO_PREFIX "audio_src_"

namespace olive {
using namespace google::protobuf;
using std::map;
using std::queue;
using std::string;
using std::vector;
// Anonymous namespace 
namespace {

typedef struct _VideoMixer {
  GstElement *video_mixer;
  GstElement *encoder;
} VideoMixer;

typedef struct OnIceCandidateData {
  videocall::Internal* klass;
  gchar *peer_sess_id;
  int32 stream_id;
} OnIceCandidateData;

typedef struct _KmsConnectData {
  /*Pads*/
  GstElement *pipeline;
  GstElement *endpoint;
  GstPad *videoSinkPad = NULL;
  GstPad *audioSrcPad = NULL;
  GstPad *videoSrcPad = NULL;
  GstPad *audioSinkPad = NULL;
  gboolean video_sink_linked = FALSE;
  gboolean audio_sink_linked = FALSE;
  GstElement *vp8enc;
  GstElement *opusParse;
} KmsConnectData;
vector<KmsConnectData*> pads;
static void kms_detroy_stream_data(gpointer data) {
  g_slice_free(KmsConnectData, data);
}

static void bus_msg(GstBus * bus, GstMessage * msg, gpointer pipe) {
  LOG(INFO)<< "=============================================================";
  LOG(INFO) << "bus_msg msg = "<<GST_MESSAGE_TYPE (msg);
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR: {
      GST_ERROR ("Error: %" GST_PTR_FORMAT, msg);
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe),
          GST_DEBUG_GRAPH_SHOW_ALL, "error");
      //fail ("Error received on bus");
      LOG(FATAL) << "Error received on bus";
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
static void printLine() {
  LOG(INFO)<< "=============================================================";
}
static void doConnect() {
  printLine();
  int size = pads.size();
  LOG(INFO)<< " pads size = "<<size;
  if (size == 2) {
    KmsConnectData *first = pads[0];
    KmsConnectData *second = pads[1];
    if (!first->video_sink_linked) {
      if (first->videoSrcPad && second->videoSinkPad) {

        GstPad *vp8Sink = gst_element_get_static_pad(first->vp8enc, "sink");
        GstPadLinkReturn linkToVp8sink = gst_pad_link(first->videoSrcPad, vp8Sink);
        LOG(INFO)<<"first video link linkToVp8sink pad result = "<<linkToVp8sink;
        first->video_sink_linked = TRUE;
        GstPad *vp8Src = gst_element_get_static_pad(first->vp8enc, "src");
        GstPadLinkReturn linkToVp8src = gst_pad_link_full(vp8Src, second->videoSinkPad, GST_PAD_LINK_CHECK_NOTHING);
        LOG(INFO)<<"first video link linkToVp8src pad result = "<<linkToVp8src;

        gst_element_sync_state_with_parent(first->vp8enc);
        GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(first->pipeline), GST_DEBUG_GRAPH_SHOW_ALL, "FIRST_video_pipeline");
      }
    }
    if (!first->audio_sink_linked) {
      if (first->audioSinkPad && second->audioSrcPad) {
        GstPadLinkReturn linkToVp8src = gst_pad_link_full(second->audioSrcPad, first->audioSinkPad,
            GST_PAD_LINK_CHECK_NOTHING);
//        GstPad *opusSink = gst_element_get_static_pad(first->opusParse, "sink");
//        GstPad *opusSrc = gst_element_get_static_pad(first->opusParse, "src");

//        GstPadLinkReturn linkToVp8src = gst_pad_link_full(second->audioSrcPad, opusSink, GST_PAD_LINK_CHECK_NOTHING);
//
//        GstPadLinkReturn linkFirstToSecond = gst_pad_link_full(opusSrc, first->audioSinkPad, GST_PAD_LINK_CHECK_NOTHING);
        first->audio_sink_linked = TRUE;
//        LOG(INFO)<<"first audio link sink pad result = "<<linkFirstToSecond;
        LOG(INFO)<<"first audio link src pad result = "<<linkToVp8src;
      }
    }
    if (!second->video_sink_linked) {
      if (second->videoSrcPad && first->videoSinkPad) {

        GstPad *vp8Sink = gst_element_get_static_pad(second->vp8enc, "sink");
        GstPadLinkReturn linkToVp8sink = gst_pad_link(second->videoSrcPad, vp8Sink);
        GstPad *vp8Src = gst_element_get_static_pad(second->vp8enc, "src");
        GstPadLinkReturn linkToVp8src = gst_pad_link_full(vp8Src, first->videoSinkPad, GST_PAD_LINK_CHECK_NOTHING);
        LOG(INFO)<<"second video link linkToVp8sink pad result = "<<linkToVp8sink;
        LOG(INFO)<<"second video link linkToVp8src pad result = "<<linkToVp8src;
        second->video_sink_linked = TRUE;
        GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(first->pipeline), GST_DEBUG_GRAPH_SHOW_ALL, "SECOND_video_pipeline");
        gst_element_sync_state_with_parent(second->vp8enc);
      }
    }
    if (!second->audio_sink_linked) {
      if (second->audioSinkPad && first->audioSrcPad) {

        GstPadLinkReturn linkToVp8src = gst_pad_link_full(first->audioSrcPad, second->audioSinkPad,
            GST_PAD_LINK_CHECK_NOTHING);
//        GstPad *opusSink = gst_element_get_static_pad(second->opusParse, "sink");
//        GstPad *opusSrc = gst_element_get_static_pad(second->opusParse, "src");
//        GstPadLinkReturn linkToVp8src = gst_pad_link_full(first->audioSrcPad, opusSink, GST_PAD_LINK_CHECK_CAPS);
//        GstPadLinkReturn linkFirstToSecond = gst_pad_link_full(opusSrc, second->audioSinkPad, GST_PAD_LINK_CHECK_NOTHING);
//        second->audio_sink_linked = TRUE;
//        LOG(INFO)<<"second audio link sink pad result = "<<linkFirstToSecond;
        LOG(INFO)<<"second audio link src pad result = "<<linkToVp8src;
      }
    }
  }
}
static void connect_sink_with_peer(GstElement * element, GstPad * pad, gpointer user_data) {
  LOG(INFO)<< "=============================================================";
  LOG(INFO) << "connect_sink...pad=" << GST_PAD_NAME (pad);

  KmsConnectData *data = (KmsConnectData*) user_data;

  GST_DEBUG_OBJECT (pad, "New pad %" GST_PTR_FORMAT, element);

  gchar * name = GST_OBJECT_NAME(pad);
  gboolean connect = TRUE;
  if (g_str_has_prefix (name, "video_src")) {
    data->videoSrcPad = pad;
  } else if(g_str_has_prefix (name, "sink_video")) {
    data->videoSinkPad = pad;
  } else if(g_str_has_prefix(name,"audio_src")) {
    data->audioSrcPad = pad;
  } else if(g_str_has_prefix(name,"sink_audio")) {
    data->audioSinkPad = pad;
  } else {
    connect = FALSE;
  }
  if(connect) {
    doConnect();
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

namespace videocall {
class Internal {
public:
  typedef struct _StreamData {
    // TODO(chengxu): add a lock to use this class
    int32 stream_id;
    gchar *gst_sess_id;

    GstElement* webrtc_endpoint;
    queue<IceCandidate> response_ice_candidates;
    //vector<string> ice_candidates;
    gboolean ice_gathering_done;
    KmsConnectData *connect_data;
  } StreamData;
  map<int32, StreamData*> stream_data;
  GstElement* pipeline;
//	VideoMixer* mixer;
  Internal() {
    pipeline = gst_pipeline_new(NULL);
    GstClock* clock = gst_system_clock_obtain();
    gst_pipeline_use_clock(GST_PIPELINE(pipeline), clock);
    g_object_unref(clock);
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
      LOG(FATAL)<<"gather-candidates fail";
    }
  }

  void CreateMediaPipeline(int32 stream_id) {
    StreamData* stream_data_item = GetStreamData(stream_id);
    if (stream_data_item == NULL) {
      LOG(ERROR) << "CreateMediaPipeline: stream_id is invalid.";
      return;
    }

    LOG(INFO) << "startCreateMediaPipline";
//		stream_data_item->videoMixer = mixer;
    gchar * audio_codec = "PCMU/8000";
    gchar * video_codec = "VP8/90000";
    gboolean bundle = TRUE;

    GArray *audio_codecs_array, *video_codecs_array;
    gchar *audio_codecs[] = {audio_codec, NULL};
    gchar *video_codecs[] = {video_codec, NULL};

    GstElement* answerer = gst_element_factory_make ("webrtcendpoint", NULL);

    GstElement* vp8 = gst_element_factory_make("vp8parse",NULL);
    GstElement* opusParse = gst_element_factory_make("opusparse",NULL);
//    g_object_set (G_OBJECT (vp8),
//                      "deadline", G_GINT64_CONSTANT (200000),
//                      "threads", 1,
//                      "cpu-used", 16,
//                      "resize-allowed", TRUE,
//                      "target-bitrate", 300000,
//                      "end-usage", /* cbr */ 1,
//                      NULL);

    GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    gboolean ret;

    gst_bus_add_signal_watch (bus);
    g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);

    audio_codecs_array = create_codecs_array (audio_codecs);
    video_codecs_array = create_codecs_array (video_codecs);
    g_object_set (answerer, "num-audio-medias", 1, "audio-codecs",
        g_array_ref (audio_codecs_array), "num-video-medias", 1, "video-codecs",
        g_array_ref (video_codecs_array),NULL);
    GstCaps*c = gst_caps_from_string ("audio/x-opus" );
    g_object_set (answerer, "audio-caps", c, NULL);
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

    /* Add elements */
    gst_bin_add (GST_BIN (pipeline), answerer);

    stream_data_item->connect_data = g_slice_new0(KmsConnectData);
    stream_data_item->connect_data->endpoint = answerer;
    stream_data_item->connect_data->pipeline = pipeline;
    stream_data_item->connect_data->vp8enc = vp8;
    stream_data_item->connect_data->opusParse = opusParse;

    gst_bin_add_many(GST_BIN(pipeline),vp8,opusParse,NULL);
    pads.push_back(stream_data_item->connect_data);
    g_signal_connect_data (answerer, "pad_added", G_CALLBACK (connect_sink_with_peer), stream_data_item->connect_data,
        (GClosureNotify) kms_detroy_stream_data, G_CONNECT_AFTER);
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
          LOG(INFO) << "@@@@@@@@@@ice_candidate_str" << cand.ice_candidate();
          stream_data_item->response_ice_candidates.pop();
      }
  }

  int CreateStream() {
    int stream_id = rand();
    StreamData *data = GetStreamData(stream_id);
    while(data) {
      stream_id = rand();
      data = GetStreamData(stream_id);
    }
    StreamData* new_stream_data_item = new StreamData;
    LOG(INFO) << "createStream";
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
      LOG(FATAL) << "gst_sdp_message_parse_buffer(sdp_offer) fails.";
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
//		LOG(INFO) << "answer_sdp_str=" << answer_sdp_str;
    FinishMediaPipeline(stream_id);
    return true;
  }

  bool AddIceCandidate(int stream_id, const IceCandidate& ice_candidate) {
    StreamData* stream_data_item = GetStreamData(stream_id);
    if (stream_data_item == NULL) {
      LOG(ERROR) << "AddIceCandidate: stream_id is invalid.";
      return false;
    }

    GstElement* answerer = stream_data_item->webrtc_endpoint;
    gchar* answerer_sess_id = stream_data_item->gst_sess_id;

    // TODO: add_ice_candidate
    KmsIceCandidate* candidate;
    candidate = kms_ice_candidate_new (ice_candidate.ice_candidate().c_str(),
				       ice_candidate.sdp_mid().c_str(),
				       (guint8)(ice_candidate.sdp_m_line_index()), NULL);
    gboolean ret;
    g_signal_emit_by_name (answerer, "add-ice-candidate", answerer_sess_id,
        candidate, &ret);
    if (ret) {
      LOG(ERROR) << "add-ice-candidate(" << answerer_sess_id << ") failed.";
      return false;
    }

    return true;
  }

private:
  static void on_ice_gathering_done_static (GstElement *webrtcendpoint,
      gpointer gdata,
      OnIceCandidateData * data) {
    //OnIceCandidateData* data = (OnIceCandidateData*)gdata;
    videocall::Internal* internal = data->klass;
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
    videocall::Internal* internal = data->klass;
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
      
      LOG(INFO) << "stream_id=" << stream_id;
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
  // namespace videocall

VideoCallPipeline::VideoCallPipeline() {
internal_ = new videocall::Internal;
LOG(INFO)<< "internal_=" << internal_;
}
VideoCallPipeline::~VideoCallPipeline() {
  // TODO(chengxu): release all the memory and resource.
  delete internal_;
}

int VideoCallPipeline::CreateStream() {
  return internal_->CreateStream();
}
bool VideoCallPipeline::CloseStream(int stream_id) {
  return internal_->CloseStream(stream_id);
}
bool VideoCallPipeline::AddIceCandidate(int stream_id, const IceCandidate& ice_candidate) {
  return internal_->AddIceCandidate(stream_id, ice_candidate);
}
bool VideoCallPipeline::ProcessSdpOffer(int stream_id, const string& sdp_offer, string* sdp_answer) {
  return internal_->ProcessSdpOffer(stream_id, sdp_offer, sdp_answer);
}
void VideoCallPipeline::ReceiveIceCandidates(int stream_id, vector<IceCandidate>* ice_candidates) {
  internal_->ReceiveIceCandidates(stream_id, ice_candidates);
}

}  // namespace olive
