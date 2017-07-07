/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: zhihan@orangelab.cn (Zhihan He)
 *
 * session_info.h
 * ---------------------------------------------------------------------------
 * Defines the exporte_var for the variables exported to /varz handler.
 * ---------------------------------------------------------------------------
 */


#ifndef SESSION_INFO_H__
#define SESSION_INFO_H__

#include "stream_service/orbit/base/singleton.h"
#include "stream_service/orbit/base/strutil.h"

#include "glog/logging.h"

#include "time.h"
#include <map> 
#include <memory> 
#include <vector> 
#include <mutex>
#include <string>

#include "stream_service/orbit/network_status.h"

#define MAX_SAVE_SESSION_COUNT 50
#define MAX_VALUE_SIZE 100

#define STREAM_STATUS_LIVE "live"
#define STREAM_STATUS_LEAVE "leave"

#define SESSION_STATUS_LIVE "live"
#define SESSION_STATUS_DESTORYED "destoryed"

namespace orbit {

//public method

// Get current date/time, format is YYYY-MM-DD HH:mm:ss,It's can used by javascript directly
const std::string GetDateTime(const time_t ref_time);

const std::string GetCurrentDateTimeString();

const std::string GetCurrentTimeWithMillisecond();

//what is this ?
//It's use for collect Session/Stream's info showing in the statusz's HTML.
//how to use:
//step 1: import header file:
//        #include "stream_service/orbit/base/session_info.h"
//step 2: get session manager's instance,It's a singleton class:
//        orbit::SessionInfoManager* session_info = 
//                Singleton<orbit::SessionInfoManager>::GetInstance();
//step 3: add session in manager:
//        session_info->AddSession(session_id);
//step 4: add stream in session:
//        orbit::SessionInfoPtr session = session_info->GetSessionInfoById(session_id);
//        if(session != NULL) {
//          session->AddStream(stream_id);
//        }
//add more data,you can use SetStreamCustomInfo method:
//        session->SetStreamCustomInfo(stream_id,map_key,show_value);
//then if you want to show this value,you can modify the statusz.tpl's html :
//        find {{#stream_infos}} line,then add <td class="text-right">{{custom_infos.map_key}}</td>
//        at the end. do not forget add <th> tag in <thead> tag for describe this data's meaning.
class SessionInfo;

class StreamInfo {
  typedef std::map<string, std::vector<string>> CustomInfoMap;
  public:
    ~StreamInfo() {
      VLOG(2) << "stream_id:" << stream_id_ << " freed";
    }

    StreamInfo(const int stream_id) {
      stream_id_ = stream_id;
      status_ = STREAM_STATUS_LIVE;
      create_time_ = time(NULL);
    }
    string GetUserName(){
      return user_name_;
    }
    string GetUserID() {
      return user_id_;
    }

    string GetOriginUserID() {
      return origin_userid_;
    }

    void SetOriginUserID(const string origin_userid) {
      origin_userid_ = origin_userid;
    }

    string GetStatus() {
      return status_;
    }

    time_t GetCreateTime() {
      return create_time_;
    }

    time_t GetLeaveTime() {
      std::lock_guard<std::mutex> guard(stream_mutex_);
      return leave_time_;
    }
    
   private:

    void ChangeStateToLeave() {
      std::lock_guard<std::mutex> guard(stream_mutex_);
      if(status_ == STREAM_STATUS_LEAVE) {
        LOG(INFO) << "Stream id :" << stream_id_ << " have removed. ignore";
        return;
      }
      status_ = STREAM_STATUS_LEAVE;
      leave_time_ = time(NULL);
    }

    void SetMoreInfo(const string user_name,const string user_id) {
      std::lock_guard<std::mutex> guard(stream_mutex_);
      user_name_ = user_name;
      user_id_ = user_id;
    }

    //append more info for one custom key
    void AppendCustomInfo(const string custom_key,const string custom_value) {
      std::lock_guard<std::mutex> guard(stream_mutex_);
      std::vector<string> vectorValue = custom_info_map_[custom_key];
      vectorValue.push_back(custom_value);
      if(vectorValue.size() > MAX_VALUE_SIZE) {
        vectorValue.erase(vectorValue.begin(),vectorValue.begin() + (vectorValue.size() - MAX_VALUE_SIZE));
      }
      custom_info_map_[custom_key] = vectorValue;
    }

    //use to create custom info key and value,they keeped in 
    void SetCustomInfo(const string custom_key,const string custom_value) {
      //if the old key comming,it's can not cover the old value
      //custom_info_map_.insert(std::pair<string, string>(custom_key, custom_value));
      std::lock_guard<std::mutex> guard(stream_mutex_);
      std::vector<string> vectorValue;
      vectorValue.push_back(custom_value);
      custom_info_map_[custom_key] = vectorValue;
    }

    string GetAsJson(bool show_leave = false) {
        std::lock_guard<std::mutex> guard(stream_mutex_);

        // Hide closed stream
        if (show_leave == false && status_ == STREAM_STATUS_LEAVE) {
          return "";
        }
        
        //handle customInfo
        string custom_info_string = "{";
        std::map<string,std::vector<string>>::iterator it;
        for(it = custom_info_map_.begin(); it != custom_info_map_.end(); it++) {
          custom_info_string += it->first + ":" + GetValueFromVector(it->second) + ",";
        }
        if(',' == custom_info_string.back()) {
          custom_info_string.pop_back();
        }
        custom_info_string += "}";


        return StringPrintf("{stream_id:%d,user_name:\"%s\",user_id:\"%s\",status:\"%s\",create_time:\"%s\",leave_time:\"%s\",custom_infos: %s }", stream_id_,
          user_name_.c_str(),
          user_id_.c_str(),
          status_.c_str(),
          GetDateTime(create_time_).c_str(),
          (status_ == STREAM_STATUS_LEAVE ? GetDateTime(leave_time_).c_str() : ""),
          custom_info_string.c_str()
        );
    }

    int stream_id_;
    string status_;
    string user_name_ = "";
    string user_id_ = "";
    string origin_userid_ = "";
    time_t create_time_;
    time_t leave_time_;
    CustomInfoMap custom_info_map_;
    std::mutex stream_mutex_;

    string GetValueFromVector(vector<string> &values) {
      if(values.empty()) {
        return "\"\"";
      }
      if(values.size() == 1) {
        return "\"" + values.front() + "\"";
      }
      string json_string = "[";
      for (std::vector<string>::iterator it=values.begin(); it!=values.end(); ++it) {
        json_string += "\"" + *it + "\",";
      }
      if(',' == json_string.back()) {
        json_string.pop_back();
      }
      json_string += "]";
      return json_string;
    }
    friend class SessionInfo;
};

//adviced by daxing
typedef std::shared_ptr<StreamInfo> StreamInfoPtr;
typedef std::map<int, StreamInfoPtr> StreamInfoMap;
typedef std::pair<int, StreamInfoPtr> StreamInfoPair;
 
class SessionInfo {
  public:

    ~SessionInfo() {
      VLOG(2) << "session_id:" << session_id_ << " freed";
    }

    SessionInfo(const int session_id) {
      session_id_ = session_id;
      create_time_ = time(NULL);
    }

    void AddStream(const int stream_id);

    void RemoveStream(const int stream_id);

    void SetMoreInfo(const int stream_id, const string message);

    void SetStreamCustomInfo(const int stream_id, 
                             const string custom_key,
			                 const string custom_value);
    void SetStreamCustomInfo(const int stream_id, 
                             const string custom_key,
			                 const unsigned int custom_value);
    void SetStreamCustomInfo(const int stream_id,
                             const string custom_key,
			                 const int custom_value);
    void SetStreamCustomInfo(const int stream_id, 
                             const string custom_key,
			                 NETWORK_STATUS custom_value);

    void AppendStreamCustomInfo(const int stream_id, 
                                const string custom_key,
				                const string custom_value);

    void _ChangeStateToDestoryed() {
      std::lock_guard<std::mutex> guard(session_mutex_);
      if(status_ == SESSION_STATUS_DESTORYED) {
        VLOG(2) << "session id :" << session_id_ << " have removed. ignore";
        return;
      }
      status_ = SESSION_STATUS_DESTORYED;
      destroyed_time_ = time(NULL);
    }

    int GetSessionId() {
      std::lock_guard<std::mutex> guard(session_mutex_);
      return session_id_;
    }

    string GetRoomID() {
      return room_id_;
    }

    time_t GetCreateTime() {
      return create_time_;
    }

    time_t GetDestoryTime() {
      std::lock_guard<std::mutex> guard(session_mutex_);
      return destroyed_time_;
    }

    string GetAsJson(const std::string room_id = "");
    string GetAsJson(const int stream_id);

    string GetStatus() {
      std::lock_guard<std::mutex> guard(session_mutex_);
      return status_;
    }

    int StreamCountTotal() {
      std::lock_guard<std::mutex> guard(session_mutex_);
      return stream_map_.size();
    }

    std::vector<int> GetStreamsId() {
      std::lock_guard<std::mutex> guard(session_mutex_);
      std::vector<int> vec_streams_id;
      for (std::map<int, StreamInfoPtr>::iterator it=stream_map_.begin(); it!=stream_map_.end(); ++it) {
        vec_streams_id.push_back(it->first);
      }
      return vec_streams_id;
    }

    // Get live streams id
    std::vector<int> GetLiveStreamsId() {
      std::lock_guard<std::mutex> guard(session_mutex_);
      std::vector<int> vec_streams_id;
      for (std::map<int, StreamInfoPtr>::iterator it=stream_map_.begin(); it!=stream_map_.end(); ++it) {
        StreamInfoPtr stream_ptr = it->second;
        if (stream_ptr && stream_ptr->GetStatus() == STREAM_STATUS_LIVE) {
          vec_streams_id.push_back(it->first);
        }
      }
      return vec_streams_id;
    }

    bool HasLiveStreams() {
      std::vector<int> live_streams = GetLiveStreamsId();
      if (live_streams.size() == 0) {
        return false;
      } else {
        return true;
      }
    }
    
    StreamInfoPtr GetStreamInfoById(const int stream_id);
  private:


    std::mutex session_mutex_;
    StreamInfoMap stream_map_;
    int session_id_;
    int total_stream_count_ = 0;
    string room_id_ = "";
    string room_type_ = "";
    string status_ = SESSION_STATUS_LIVE;
    StreamInfoPtr _GetStreamInfoById(const int stream_id);
    time_t create_time_;
    time_t destroyed_time_;
};

//adviced by daxing
typedef std::shared_ptr<SessionInfo> SessionInfoPtr;
//want to see some destroyed session
typedef std::vector<SessionInfoPtr> SessionInfoVector;

class SessionInfoManager {
  public:
    void AddSession(const int session_id);
    bool RemoveSession(const int session_id);

    //void AddStreamInSession(const int stream_id,const int session_id);
    //bool RemoveStreamInSession(const int stream_id,const int session_id);

    SessionInfoPtr GetSessionInfoById(const int session_id);

    int SessionCountTotal() {
      return total_session_count_;
    }

    int SessionCountNow() {
      return current_session_count_;
    }

    string GetAsJson(const std::string room_id = "");
    string GetAsJson(const int session_id, const int stream_id);

  private:
    std::mutex session_manager_mutex_;
    SessionInfoVector session_vector_;
    int current_session_count_ = 0;
    int total_session_count_ = 0;
    SessionInfoPtr _GetSessionInfoById(const int session_id);
    DEFINE_AS_SINGLETON(SessionInfoManager);
};

}  // namespace orbit

#endif  // SESSION_INFO_H__

