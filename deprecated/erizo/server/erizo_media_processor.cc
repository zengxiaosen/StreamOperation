/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * erizo_media_processor.cc
 * ---------------------------------------------------------------------------
 * Implements the media processor using erizo lib.
 * ---------------------------------------------------------------------------
 */

#include "erizo_media_processor.h"
#include "stream_service/erizo/WebRtcConnection.h"
#include "stream_service/erizo/OneToManyProcessor.h"

#include "stream_service/erizo/media/ExternalInput.cpp"
#include "stream_service/erizo/media/ExternalOutput.cpp"

#include <boost/thread/mutex.hpp>

#include "glog/logging.h"
#include "gflags/gflags.h"

#include <google/protobuf/text_format.h>

#include <vector>
#include <string>
#include <map>
#include <queue>

DEFINE_bool(use_muxer, false, "Flag to use muxer or not.");
DEFINE_bool(use_two_way, true, "Flag to use the two way connection.");
// "/home/project/xucheng2.mkv"
DEFINE_string(external_output, "",
              "Specify the external output file.");
// "/home/project/xucheng.mkv"
DEFINE_string(external_input, "",
              "SPecify the external input file.");

namespace olive {
using namespace google::protobuf;
using std::map;
using std::queue;
using std::string;
using std::vector;

  namespace {
    // Parse the JSON object to IceCandidate
    // {"candidate":"a=candidate:2 1 udp 2013266431 192.168.1.101 41606 typ host generation 0","sdpMLineIndex":"0","sdpMid":"video"}
    bool ParseJSONString(const string& json, IceCandidate* ice_candidate) {
      unsigned int start, end;
      string prefix = "candidate\":\"";
      start = json.find(prefix);
      if (start == string::npos) {
        return false;
      }
      start = start + prefix.size();
      end = json.find("\"", start);
      if (end == string::npos) {
        return false;
      }
      string tmp_ice = json.substr(start, end - start);
      LOG(INFO) << "tmp_ice=" << tmp_ice;
      ice_candidate->set_ice_candidate(tmp_ice);

      prefix = "sdpMid\":\"";
      start = json.find(prefix);
      start = start + prefix.size();
      if (start == string::npos) {
        return false;
      }
      end = json.find("\"", start);
      if (end == string::npos) {
        return false;
      }
      string tmp_sdp_mid = json.substr(start, end - start);
      LOG(INFO) << "tmp_sdp_mid=" << tmp_sdp_mid;
      ice_candidate->set_sdp_mid(tmp_sdp_mid);

      prefix = "sdpMLineIndex\":\"";
      start = json.find(prefix);
      start += prefix.size();
      if (start == string::npos) {
        return false;
      }
      end = json.find("\"", start);
      if (end == string::npos) {
        return false;
      }
      string sdp_m_line_index_str = json.substr(start, end - start);
      LOG(INFO) << "number:" << sdp_m_line_index_str;
      ice_candidate->set_sdp_m_line_index(std::stoi(sdp_m_line_index_str));

      return true;
    }
  }  // Annoymouse namespace

  namespace erizo_videocall {
    class Internal {
    public:
      typedef struct _StreamData {
        boost::mutex stream_data_mutex;
        int32 stream_id;
        erizo::WebRtcConnection* webrtc_endpoint;
        queue<IceCandidate> response_ice_candidates;
        bool ice_gathering_done;
      } StreamData;

      Internal();
      int CreateStream();
      bool CloseStream(int stream_id);
      bool AddIceCandidate(int stream_id, const IceCandidate& ice_candidate);
      bool ProcessSdpOffer(int stream_id, const string& sdp_offer, string* sdp_answer);
      void ReceiveIceCandidates(int stream_id, vector<IceCandidate>* ice_candidates);
    private:
      erizo::WebRtcConnection* CreateMediaPipeline(StreamData* stream_data);

      StreamData* GetStreamData(int32 stream_id) {
        auto stream_it = stream_data_.find(stream_id);
        if (stream_it == stream_data_.end()) {
          return NULL;
        }
        StreamData* stream_data_item = (*stream_it).second;
        return stream_data_item;
      }
      map<int32, StreamData*> stream_data_;

      erizo::OneToManyProcessor* muxer_;
      erizo::OneToManyProcessor* muxer1_;
      erizo::OneToManyProcessor* muxer2_;
      erizo::WebRtcConnection* conn_1_;
      erizo::WebRtcConnection* conn_2_;

      erizo::ExternalOutput* sinker_;
      erizo::ExternalInput* input_;
    };  // class Internal
  } // namespace erizo_videocall

ErizoVideoCallPipeline::ErizoVideoCallPipeline() {
  internal_ = new erizo_videocall::Internal;
}
ErizoVideoCallPipeline::~ErizoVideoCallPipeline() {
  // TODO(chengxu): release all the memory and resource.
  delete internal_;
}

int ErizoVideoCallPipeline::CreateStream() {
  return internal_->CreateStream();
}
bool ErizoVideoCallPipeline::CloseStream(int stream_id) {
  return internal_->CloseStream(stream_id);
}
bool ErizoVideoCallPipeline::AddIceCandidate(int stream_id, const IceCandidate& ice_candidate) {
  return internal_->AddIceCandidate(stream_id, ice_candidate);
}
bool ErizoVideoCallPipeline::ProcessSdpOffer(int stream_id, const string& sdp_offer, string* sdp_answer) {
  return internal_->ProcessSdpOffer(stream_id, sdp_offer, sdp_answer);
}
void ErizoVideoCallPipeline::ReceiveIceCandidates(int stream_id, vector<IceCandidate>* ice_candidates) {
  internal_->ReceiveIceCandidates(stream_id, ice_candidates);
}

  namespace erizo_videocall {
    Internal::Internal() {
      muxer_ = new erizo::OneToManyProcessor();
      muxer1_ = new erizo::OneToManyProcessor();
      muxer2_ = new erizo::OneToManyProcessor();
      if (!FLAGS_external_output.empty()) {
        sinker_ = new erizo::ExternalOutput(FLAGS_external_output);
      }
      if (!FLAGS_external_input.empty()) {
        input_ = new erizo::ExternalInput(FLAGS_external_input);
      }
    }

    static int come_count = 0;

    class MyConnectionListener : public erizo::WebRtcConnectionEventListener {
    public:
      MyConnectionListener(erizo_videocall::Internal::StreamData* stream_data) {
        stream_data_ = stream_data;
      }
      void notifyEvent(erizo::WebRTCEvent newEvent, const std::string& message) override {
        LOG(INFO) << "event triggered..." << newEvent;
        if (newEvent == erizo::CONN_CANDIDATE) {
          // New candidate get....
          LOG(INFO) << message;
          IceCandidate tmp_candidate;
          if (!ParseJSONString(message, &tmp_candidate)) {
            LOG(ERROR) << "Parse the ice_candidate error.";
          }
          stream_data_->response_ice_candidates.push(tmp_candidate);
        }
      }
    private:
      erizo_videocall::Internal::StreamData* stream_data_;
    };

    erizo::WebRtcConnection* Internal::CreateMediaPipeline(StreamData* stream_data) {
      bool audioEnabled = true;
      bool videoEnabled = true;
      erizo::IceConfig ice_config;
      bool trickleEnabled = true;
      MyConnectionListener* listener = new MyConnectionListener(stream_data);

      LOG(INFO) << "Create Connection";

      erizo::WebRtcConnection *newConn = new erizo::WebRtcConnection(
                                                                     audioEnabled, videoEnabled, ice_config, trickleEnabled, listener);

      newConn->init();
      //newConn->createOffer();
      return newConn;
    }

    int Internal::CreateStream() {
      int stream_id = rand();
      StreamData *data = GetStreamData(stream_id);
      while(data) {
        stream_id = rand();
        data = GetStreamData(stream_id);
      }
      StreamData* new_stream_data_item = new StreamData;
      {
        boost::mutex::scoped_lock lock(new_stream_data_item->stream_data_mutex);
        LOG(INFO) << "createStream";
        new_stream_data_item->stream_id = stream_id;
        stream_data_.insert(
            std::pair<int32, StreamData*>(stream_id, new_stream_data_item));
        LOG(INFO) << "BeforeCreateMediapipeline";
        new_stream_data_item->webrtc_endpoint = CreateMediaPipeline(new_stream_data_item);
      }
      return stream_id;
    }

    bool Internal::CloseStream(int stream_id) {
      LOG(INFO) << "CloseStream()";
      return true;
    }

    bool Internal::ProcessSdpOffer(int stream_id,
                                   const string& sdp_offer,
                                   string* sdp_answer) {
      LOG(INFO) << "sdp_offer=" << sdp_offer;
 
      StreamData* stream_data_item = GetStreamData(stream_id);
      if (stream_data_item == NULL) {
        LOG(ERROR) << "ProcessSdpOffer: stream_id is invalid.";
        return false;
      }

      {
        boost::mutex::scoped_lock lock(stream_data_item->stream_data_mutex);
        erizo::WebRtcConnection* conn = stream_data_item->webrtc_endpoint;

        LOG(INFO) << "conn->getCurrentState" << conn->getCurrentState();
        //conn->setRemoteSdp(sdp_offer);

        //while (conn->getCurrentState() < 103) {
        //  LOG(INFO) << "Wait for the conn state is ready...";
        //  sleep(1);
        //}
        //conn->createOffer();

        if (!FLAGS_external_output.empty()) {
          LOG(INFO) << "sinker_=" << sinker_;
          conn->setVideoSink(sinker_);
          conn->setAudioSink(sinker_);
          sinker_->init();
        }


        if (!FLAGS_external_input.empty()) {
          input_->setVideoSink(conn);
          input_->setAudioSink(conn);
          input_->init();
        }

        if (FLAGS_use_muxer) {
          conn->setAudioSink(muxer_);
          conn->setVideoSink(muxer_);
            
          muxer_->setPublisher(conn);        
          muxer_->addSubscriber(conn, std::to_string(stream_data_item->stream_id));
          LOG(INFO) << "Create subscriber";
        }

        if (FLAGS_use_two_way) {
          come_count++;
          if (come_count == 1) {
            conn_1_ = conn;
          } else if (come_count == 2) {
            conn_2_ = conn;
            muxer1_->setPublisher(conn_1_);
            muxer2_->setPublisher(conn_2_);

            conn_1_->setAudioSink(muxer1_);
            conn_1_->setVideoSink(muxer1_);
            
            conn_2_->setAudioSink(muxer2_);
            conn_2_->setVideoSink(muxer2_);

            muxer2_->addSubscriber(conn_1_, std::to_string(stream_data_item->stream_id));
            muxer1_->addSubscriber(conn_2_, std::to_string(stream_data_item->stream_id));
          }
          LOG(INFO) << "Create subscriber";

          //stream_data_
        }
        //conn->createOffer();
        //*sdp_answer = conn->getAnswer();
        //LOG(INFO) << "sdp_answer=" << *sdp_answer;
        
        //conn->createOffer();
        //*sdp_answer = conn->getAnswer();

        conn->setRemoteSdp(sdp_offer);
        *sdp_answer = conn->getAnswer();
        //if (conn->processOffer(sdp_offer, sdp_answer)) {
          LOG(INFO) << "sdp_answer=" << *sdp_answer;
          //*sdp_answer = conn->getLocalSdp();
          return true;
          //}
      }
      return false;
    }

    bool Internal::AddIceCandidate(int stream_id,
                                   const olive::IceCandidate& ice_candidate) {
      LOG(INFO) << "AddIceCandidate:stream_id=" << stream_id;
      string str;
      google::protobuf::TextFormat::PrintToString(ice_candidate, &str);
      LOG(INFO) << str;

      StreamData* stream_data_item = GetStreamData(stream_id);
      if (stream_data_item == NULL) {
        LOG(ERROR) << "AddIceCandidate: stream_id is invalid.";
        return false;
      }
      {
        boost::mutex::scoped_lock lock(stream_data_item->stream_data_mutex);

        erizo::WebRtcConnection* conn = stream_data_item->webrtc_endpoint;

        string ice_str = ice_candidate.ice_candidate();
        string sdp_mid = ice_candidate.sdp_mid();
        int32 sdp_m_line_index = ice_candidate.sdp_m_line_index();
        ice_str = "a=" + ice_str;
        conn->addRemoteCandidate(sdp_mid, sdp_m_line_index, ice_str);
      }
      return true;
    }

    void Internal::ReceiveIceCandidates(int stream_id,
                                        vector<IceCandidate>* ice_candidates) {
      StreamData* stream_data_item = GetStreamData(stream_id);
      if (stream_data_item == NULL) {
        LOG(ERROR) << "AddIceCandidate: stream_id is invalid.";
        return;
      }
      
      {
        boost::mutex::scoped_lock lock(stream_data_item->stream_data_mutex);

        while(!stream_data_item->response_ice_candidates.empty()) {
          IceCandidate cand = stream_data_item->response_ice_candidates.front();
          ice_candidates->push_back(cand);
          LOG(INFO) << "@@@@@@@@@@ice_candidate_str" << cand.ice_candidate();
          stream_data_item->response_ice_candidates.pop();
        }
      }
      return;
    }
  }  // erizo_videocall

}  // namespace olive
