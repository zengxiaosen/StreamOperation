// Copyright 2015 All Rights Reserved.
// Author: eric@orangelab.cn (Cheng Xu)

#include "videolive_pipeline.h"

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


#define KMS_VIDEO_PREFIX "video_src_"
#define KMS_AUDIO_PREFIX "audio_src_"

// rtmp flags
DECLARE_string(stun_server_ip);
DECLARE_string(public_ip);
DEFINE_bool(open_live, true, "flag for open rtmp.");
DEFINE_string(live_location, "rtmp://push.bj.bcelive.com/live/fj1r7agt79n4f56uf81", "specify the rtmp push location.");
// end rtmp flags

namespace olive {
  using namespace google::protobuf;
  using std::map;
  using std::queue;
  using std::string;
  using std::vector;
  
  // Anonymous namespace
  namespace {
    
    typedef struct OnIceCandidateData {
      videolive::Internal* klass;
      gchar *peer_sess_id;
      int32 stream_id;
    } OnIceCandidateData;
    
    typedef struct _KmsConnectData {
      /*Pads*/
      GstElement *pipeline;
      GstElement *endpoint;
      GstElement *vp8enc;
      GstElement *opusParse;
      // rtmp branch
      GstElement *audioTee;
      GstElement *videoTee;
      GstElement *flvmux;
      // end
    } KmsConnectData;
    
    // pads 栈
    vector<KmsConnectData*> pads;
    
    static void kms_detroy_stream_data(gpointer data) {
      g_slice_free(KmsConnectData, data);
    }

    static gboolean set_pipeline_to_null (gpointer data) {
      gst_element_set_state ((GstElement *)data, GST_STATE_PLAYING);
      LOG(INFO)<< "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<set_pipeline_to_null";
      return false;
    }
    
    static void bus_msg(GstBus * bus, GstMessage * msg, gpointer pipe) {
      LOG(INFO)<< "=============================================================";
      LOG(INFO) << "bus_msg msg = "<<GST_MESSAGE_TYPE (msg);
      switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_ERROR: {
          GST_ERROR ("Error: %" GST_PTR_FORMAT, msg);
          GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe),
                                             GST_DEBUG_GRAPH_SHOW_ALL, "error");
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
    
    static gboolean join_video_live(GstElement *pipeline, GstElement *videoSrcTee, GstElement *flvmux) {
      
      LOG(INFO)<<"START: live a video stream";
      // START: sub pipeline to process video
      GstElement* capsfilter = gst_element_factory_make("capsfilter", NULL);
      GstCaps* caps = gst_caps_new_simple ("video/x-raw", "framerate", GST_TYPE_FRACTION, 15, 1,
                                           "width", G_TYPE_INT, 640, "height", G_TYPE_INT, 360, NULL);
      g_object_set(GST_OBJECT(capsfilter), "caps", caps, NULL);
      // timeoverlay
      GstElement* timeoverlay = gst_element_factory_make("timeoverlay", NULL);
      // queue
      GstElement* videoqueue = gst_element_factory_make("queue", "videoqueue");
      // video codec
      GstElement* x264enc = gst_element_factory_make("x264enc", NULL);
      g_object_set(GST_OBJECT(x264enc), "bitrate" , 256, NULL);
      g_object_set(GST_OBJECT(x264enc), "tune" , 4, NULL);
      // add to pipeline and link them
      gst_bin_add_many(GST_BIN(pipeline), capsfilter,timeoverlay, videoqueue, x264enc, NULL);
      gboolean linkManyRet = gst_element_link_many(capsfilter, timeoverlay, videoqueue, x264enc, NULL);
      LOG(INFO)<<">> create sub-pipeline: capsfilter => timeoverlay => queue => x264enc, link result true/false :"<< linkManyRet;
      // END: sub pipeline to process video
      
      // link from video src
      GstPad *teeSrcPad   = gst_element_get_request_pad(videoSrcTee, "src_%u");
      GstPad *capsSinkPad = gst_element_get_static_pad(capsfilter, "sink");
      GstPadLinkReturn linkFromVideoSrcToCaps = gst_pad_link(teeSrcPad, capsSinkPad);
      
      LOG(INFO)<<">> link: video src => capsfilter, link result:"<<linkFromVideoSrcToCaps;
      
      // link to flvmux
      GstPad *x264encSrcPad = gst_element_get_static_pad(x264enc, "src");
      GstPad *flvmuxSinkPad = gst_element_get_request_pad (flvmux, "video");
      GstPadLinkReturn linkToFlvmux = gst_pad_link(x264encSrcPad, flvmuxSinkPad);
      LOG(INFO)<<">> link: x264enc => flvmux, link result:"<<linkToFlvmux;
      
      LOG(INFO)<<"END: live a video stream";
      
      gst_element_sync_state_with_parent(videoSrcTee);
      gst_element_sync_state_with_parent(capsfilter);
      gst_element_sync_state_with_parent(timeoverlay);
      gst_element_sync_state_with_parent(videoqueue);
      gst_element_sync_state_with_parent(x264enc);
      
    }
    
    static gboolean join_audio_live(GstElement *pipeline, GstElement *audioSrcTee, GstElement *flvmux) {
      
      LOG(INFO)<<"START: live a audio stream";
      
      // audioconvert
      GstElement* opusdec = gst_element_factory_make("opusdec", NULL);
      // audioconvert
      GstElement* audioconvert = gst_element_factory_make("audioconvert", NULL);
      // voaacenc bitrate=64000
      GstElement* voaacenc = gst_element_factory_make("voaacenc", NULL);
      g_object_set(GST_OBJECT(voaacenc), "bitrate" , 16000/*64000*/, NULL);
      
      // add audio branch to pipeline
      gst_bin_add_many(GST_BIN(pipeline),opusdec,audioconvert,voaacenc, NULL);
      gst_element_link_many(opusdec,audioconvert,voaacenc, NULL);
      
      // link from audio src
      GstPad *teeSrcPad = gst_element_get_request_pad(audioSrcTee, "src_%u");
      GstPad *audioconvertSinkPad = gst_element_get_static_pad(opusdec, "sink");
      GstPadLinkReturn linkFromAudioSrc = gst_pad_link(teeSrcPad, audioconvertSinkPad);
      LOG(INFO)<<">> link: audio src => audioconvert, link result:"<<linkFromAudioSrc;
      
      // link to flvmux
      GstPad *voaacencSrcPad = gst_element_get_static_pad(voaacenc, "src");
      GstPad *flvmuxSinkPad = gst_element_get_request_pad (flvmux, "audio");
      GstPadLinkReturn linkToFlvmux = gst_pad_link(voaacencSrcPad, flvmuxSinkPad);
      LOG(INFO)<<">> link: voaacenc => flvmux, link result:"<<linkToFlvmux;
      
      LOG(INFO)<<"END: live a audio stream";
      
      gst_element_sync_state_with_parent(voaacenc);
      gst_element_sync_state_with_parent(audioSrcTee);
      gst_element_sync_state_with_parent(audioconvert);
      gst_element_sync_state_with_parent(opusdec);
    
    }
    
    static void connect_sink_with_peer(GstElement * element, GstPad * pad, gpointer user_data) {
      KmsConnectData *data = (KmsConnectData*) user_data;
      gchar * name = GST_OBJECT_NAME(pad);
      gboolean connect = TRUE;
      if (g_str_has_prefix (name, "video_src")) {
        GstPad *teeSinkPad = gst_element_get_static_pad(data->videoTee, "sink");
        GstPadLinkReturn ret = gst_pad_link(pad, teeSinkPad);
        LOG(INFO)<<"New video src pad. link to video tee. link result: "<<ret;
      } else if(g_str_has_prefix(name,"audio_src")) {
        GstPad *teeSinkPad = gst_element_get_static_pad(data->audioTee, "sink");
        GstPadLinkReturn ret = gst_pad_link(pad, teeSinkPad);
        LOG(INFO)<<"New audio src pad. link to audio tee. link result: "<<ret;
      } else {
        connect = FALSE;
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
  
  namespace videolive {
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
      GstElement *flvmux, *rtmpsink;
      int32 try_type = 1;// TODO for test
      
      Internal() {
        pipeline = gst_pipeline_new(NULL);
        GstClock* clock = gst_system_clock_obtain();
        gst_pipeline_use_clock(GST_PIPELINE(pipeline), clock);
        g_object_unref(clock);
        
        // rtmp
        flvmux = gst_element_factory_make("flvmux", NULL);
        gst_bin_add(GST_BIN(pipeline), flvmux);
        /*
        rtmpsink = gst_element_factory_make("rtmpsink", NULL);
        g_object_set(GST_OBJECT(rtmpsink), "location", FLAGS_live_location.c_str(), NULL);
        g_object_set(GST_OBJECT(rtmpsink), "sync" , false, NULL);
        g_object_set(GST_OBJECT(rtmpsink), "async" , false, NULL);
        gst_bin_add_many(GST_BIN(pipeline), flvmux, rtmpsink, NULL);
        gst_element_link(flvmux, rtmpsink);
        */
        // end rtmp
        
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
      
        gchar * audio_codec = "opus/48000/2";
        gchar * video_codec = "VP8/90000";
        gboolean bundle = TRUE;
        
        GArray *audio_codecs_array, *video_codecs_array;
        gchar *audio_codecs[] = {audio_codec, NULL};
        gchar *video_codecs[] = {video_codec, NULL};
        
        GstElement* answerer = gst_element_factory_make ("webrtcendpoint", NULL);
        GstElement* vp8 = gst_element_factory_make("vp8parse",NULL);
        GstElement* opusParse = gst_element_factory_make("opusparse",NULL);

        if (!FLAGS_stun_server_ip.empty()) {
           g_object_set (answerer, "stun-server", FLAGS_stun_server_ip.c_str(), NULL);
           LOG(INFO) << "add stun-server:" << FLAGS_stun_server_ip;
        }

        // register message bus
        GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
        gst_bus_add_signal_watch (bus);
        g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);

 
        audio_codecs_array = create_codecs_array (audio_codecs);
        video_codecs_array = create_codecs_array (video_codecs);
        g_object_set (answerer, "num-audio-medias", 1, "audio-codecs",
                      g_array_ref (audio_codecs_array), "num-video-medias", 1, "video-codecs",
                      g_array_ref (video_codecs_array),NULL);
        
        //GstCaps*c = gst_caps_from_string ("audio/x-opus" );
        //g_object_set (answerer, "audio-caps", c, NULL);
        g_array_unref (audio_codecs_array);
        g_array_unref (video_codecs_array);
        
        LOG(INFO) << "create-session";
        
        gchar *answerer_sess_id;
        /* Session creation */
        g_signal_emit_by_name (answerer, "create-session", &answerer_sess_id);
        GST_DEBUG_OBJECT (answerer, "Created session with id '%s'", answerer_sess_id);
        
        /* add audio/video tee for rtmp */
        GstElement *audioTee = gst_element_factory_make("tee", NULL);
        GstElement *videoTee = gst_element_factory_make("tee", NULL);
        gst_bin_add_many(GST_BIN(pipeline), audioTee, videoTee, NULL);
        
        
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
        stream_data_item->connect_data->audioTee = audioTee;
        stream_data_item->connect_data->videoTee = videoTee;
        stream_data_item->connect_data->flvmux = flvmux;
        if (FLAGS_open_live) {
          // 接入视频
          join_video_live(pipeline, videoTee, flvmux);
          join_audio_live(pipeline, audioTee, flvmux);
          LOG(INFO)<<">>>> FLAGS_open_live";
        }
        
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
        // TODO change pipeline to null after 60 seconds
        //g_timeout_add(60*1000, set_pipeline_to_null, (gpointer)pipeline);
      }
      
      void ReceiveIceCandidates(int stream_id, vector<IceCandidate>* ice_candidates) {
        StreamData* stream_data_item = GetStreamData(stream_id);
        
        while(!stream_data_item->response_ice_candidates.empty()) {
          IceCandidate cand = stream_data_item->response_ice_candidates.front();
          string candidate_str = cand.ice_candidate(); 
          if (!FLAGS_public_ip.empty()) {
            // filter candidate if pulbic ip is not empty
            int found = candidate_str.find(FLAGS_public_ip.c_str());
            if (found > 0) {
              ice_candidates->push_back(cand);
              VLOG(3) << "Public ip Server candidate :" << candidate_str;
            } else {
              VLOG(3) << "Filter server candidate :" << candidate_str;
            }
          } else {
            VLOG(3) << "Native server candidate :" << candidate_str;
            ice_candidates->push_back(cand);
          }
          stream_data_item->response_ice_candidates.pop();
        }
      }

      // 处理pipeline的定制消息
      bool ProcessMessage(int message_type, void *data) {
        if (message_type == 1) { // TODO define the message_type as enum
            rtmpsink = gst_element_factory_make("rtmpsink", NULL);
            string *location = (string *)data;
            g_object_set(GST_OBJECT(rtmpsink), "location", location->c_str(), NULL);
            g_object_set(GST_OBJECT(rtmpsink), "sync" , false, NULL);
            g_object_set(GST_OBJECT(rtmpsink), "async" , false, NULL);
            gst_bin_add(GST_BIN(pipeline), rtmpsink);
            gst_element_link(flvmux, rtmpsink);
            gst_element_sync_state_with_parent(rtmpsink);
        }
        return true; 
      }
      
      int CreateStream() {
        // TODO(chengxu): Add the session management code here.
        int stream_id = rand();
        StreamData *data = GetStreamData(stream_id);
        while(data) {
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
        StreamData *data = GetStreamData(stream_id);
        if (NULL != data) {
          // TODO 
          // 1. free stream from pipeline
          // 2. free webrtc_endpoint : freeWebrtcEndpoint()
          // 3. free response_ice_candidates : 
          // 4. free connect_data : void freeKmsConnectData(KmsConnectData *cd);
          // -- free endpoint, vp8enc, opusParse,audioTee,videoTee,flvmux 
          //Stream_data.erase(data);   
        }
        return true;
      }

      bool CloseAll() {
        LOG(INFO)<<">>>>> Close pipeline.";
        gst_element_set_state(pipeline, GST_STATE_NULL);
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
        if (!ret) {
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
        videolive::Internal* internal = data->klass;
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
        videolive::Internal* internal = data->klass;
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
        // make an IceCandidate and push to queue, wait for client to fetch
        IceCandidate tmpCandidate;
        tmpCandidate.set_ice_candidate(kms_ice_candidate_get_candidate(candidate));
        tmpCandidate.set_sdp_mid(kms_ice_candidate_get_sdp_mid(candidate));
        tmpCandidate.set_sdp_m_line_index(kms_ice_candidate_get_sdp_m_line_index(candidate));
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
  // namespace videolive
  
  VideoLivePipeline::VideoLivePipeline() {
    internal_ = new videolive::Internal;
    LOG(INFO)<< "internal_=" << internal_;
  }
  VideoLivePipeline::~VideoLivePipeline() {
    // TODO(chengxu): release all the memory and resource.
    delete internal_;
  }
  
  int VideoLivePipeline::CreateStream() {
    return internal_->CreateStream();
  }
  bool VideoLivePipeline::CloseStream(int stream_id) {
    return internal_->CloseStream(stream_id);
  }
  bool VideoLivePipeline::CloseAll() {
    return internal_->CloseAll();
  }
  bool VideoLivePipeline::AddIceCandidate(int stream_id, const IceCandidate& ice_candidate) {
    return internal_->AddIceCandidate(stream_id, ice_candidate);
  }
  bool VideoLivePipeline::ProcessSdpOffer(int stream_id, const string& sdp_offer, string* sdp_answer) {
    return internal_->ProcessSdpOffer(stream_id, sdp_offer, sdp_answer);
  }
  void VideoLivePipeline::ReceiveIceCandidates(int stream_id, vector<IceCandidate>* ice_candidates) {
    internal_->ReceiveIceCandidates(stream_id, ice_candidates);
  }
  bool VideoLivePipeline::ProcessMessage(int stream_id, int message_type, void *data) {
    return internal_->ProcessMessage(message_type, data);
  }
}  // namespace olive
