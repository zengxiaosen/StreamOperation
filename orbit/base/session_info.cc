/*
 * Copyright 2016 (C) Orangelab Inc. All Rights Reserved.
 * Author: zhihan@orangelab.cn (Zhihan He)
 *
 * session_info.h --- defines the utils functions for getting session information.
 */

#include "session_info.h"

#include "glog/logging.h"
#include "folly/json.h"

#include <math.h>
#include "sys/times.h"
#include "sys/vtimes.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "stream_service/orbit/network_status.h"

namespace orbit {
  using namespace std;

  std::string GetDateWithFormat(time_t ref_time, string format) {
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&ref_time);
    // Visit http://en.cppreference.com/w/cpp/chrono/c/strftime
    // for more information about date/time format
    strftime(buf, sizeof(buf), format.c_str(), &tstruct);
    //strftime(buf, sizeof(buf), "%Y-%m-%d %X", &tstruct);

    return buf;
  }

  //public method
  const std::string GetDateTime(const time_t ref_time) {
    return GetDateWithFormat(ref_time, "%Y-%m-%d %X");
  }

  const std::string GetCurrentDateTimeString() {
    return GetDateTime(time(NULL));
  }

  const std::string GetCurrentTimeWithMillisecond() {
    time_t currentTime = time(NULL);
    timeval curTime;
    gettimeofday(&curTime, NULL);
    string retTime = GetDateWithFormat(currentTime, "%H:%M:%S.");
    return (retTime + std::to_string(curTime.tv_usec));
  }

  //session sort method.show live session at before,
  //and move the destoryed session at the end
  //sort order:
  //1.alive alive compare createTime
  //2.leave leave compare leaveTime
  //3.alive leave leave move to end
  static bool comparator(const std::shared_ptr<SessionInfo> &info1, const std::shared_ptr<SessionInfo> &info2) {
    if(info1->GetStatus() == SESSION_STATUS_DESTORYED 
      && info2->GetStatus() == SESSION_STATUS_LIVE) {
      return false;
    } else if(info1->GetStatus() == SESSION_STATUS_LIVE 
      && info2->GetStatus() == SESSION_STATUS_DESTORYED) {
      return true;
    } else {
      if(info1->GetStatus() == SESSION_STATUS_DESTORYED 
        && info2->GetStatus() == SESSION_STATUS_DESTORYED) {
        return (info1->GetDestoryTime() > info2->GetDestoryTime());
      } else {
        return (info1->GetCreateTime() > info2->GetCreateTime());
      }
    }
  }

  // stream map comparator
  static bool ComparatorStream (const StreamInfoPair& info1, const StreamInfoPair& info2) {
    if (info1.second->GetStatus() == STREAM_STATUS_LEAVE &&
        info2.second->GetStatus() == STREAM_STATUS_LIVE) {
      return false;
    } else if (info1.second->GetStatus() == STREAM_STATUS_LIVE &&
               info2.second->GetStatus() == STREAM_STATUS_LEAVE) {
      return true;
    } else {
      if (info1.second->GetStatus() == STREAM_STATUS_LEAVE &&
          info2.second->GetStatus() == STREAM_STATUS_LEAVE) {
        return (info1.second->GetLeaveTime() > info2.second->GetLeaveTime());
      } else {
        return (info1.second->GetCreateTime() > info2.second->GetCreateTime());
      }
    }
  }
  
  void SessionInfo::AddStream(const int stream_id) {
    std::lock_guard<std::mutex> guard(session_mutex_);
    if(_GetStreamInfoById(stream_id) != NULL) {
      VLOG(2) << "stream id " << stream_id << " have added to map,ignored";
      return;
    }
    total_stream_count_++;
    //StreamInfoPtr stream_info(new StreamInfo(stream_id));
    StreamInfoPtr stream_info = std::make_shared<StreamInfo>(stream_id);//(new StreamInfo(stream_id));
    stream_map_.insert(std::pair<int, StreamInfoPtr>(stream_id, stream_info));
  }

  void SessionInfo::RemoveStream(const int stream_id) {
    std::lock_guard<std::mutex> guard(session_mutex_);
    StreamInfoPtr search_stream = _GetStreamInfoById(stream_id);
    if(search_stream != NULL) {
      search_stream->ChangeStateToLeave();
    }
  }

  void SessionInfo::SetMoreInfo(const int stream_id, const string message) {
    std::lock_guard<std::mutex> guard(session_mutex_);
    StreamInfoPtr search_stream = _GetStreamInfoById(stream_id);
    if(search_stream == NULL) {
      try {
        folly::json::serialization_opts opts;
        opts.parse_numbers_as_strings = true;
        auto session_json_data = folly::parseJson(message,opts);

        auto room_id = session_json_data["room_id"];

        if(room_id.isString()) {
          room_id_ = room_id.asString().toStdString();
        }

        auto room_type = session_json_data["room_type"];

        if(room_type.isString()) {
          room_type_ = room_type.asString().toStdString();
        }

      } catch (...) {
        LOG(INFO) << "convert string to json error:" << message;
      }
    } else {
      try {
        folly::json::serialization_opts opts;
        opts.parse_numbers_as_strings = true;
        auto stream_json_data = folly::parseJson(message,opts);

        auto user_name = stream_json_data["user_name"];
        auto user_id = stream_json_data["user_id"];

        if(user_name.isString() 
            && user_id.isString()) {
          string name = user_name.asString().toStdString();
          string id = user_id.asString().toStdString();
          search_stream->SetMoreInfo(name,id);
        }
        auto origin_user_id = stream_json_data["origin_user_id"];
        if(origin_user_id != NULL) {
          search_stream->SetOriginUserID(origin_user_id.asString().toStdString());
        }
      } catch (...) {
        LOG(ERROR) << "convert string to json error:" << message;
      }
    }
  }

  void SessionInfo::SetStreamCustomInfo(const int stream_id, 
                                        const string custom_key,
                                        const string custom_value) {
    std::lock_guard<std::mutex> guard(session_mutex_);
    StreamInfoPtr search_stream = _GetStreamInfoById(stream_id);
    if(search_stream == NULL) {
      LOG(INFO) << "no stream info(" << stream_id << ") in session:" << session_id_;
      return;
    }
    search_stream->SetCustomInfo(custom_key,custom_value);
  }

  void SessionInfo::SetStreamCustomInfo(const int stream_id, 
                                        const string custom_key,
                                        const int custom_value) {
    SetStreamCustomInfo(stream_id,custom_key,StringPrintf("%d",custom_value));
  }

  void SessionInfo::SetStreamCustomInfo(const int stream_id, 
                                        const string custom_key,
                                        const unsigned int custom_value) {
    SetStreamCustomInfo(stream_id,custom_key,StringPrintf("%u",custom_value));
  }

  void SessionInfo::SetStreamCustomInfo(const int stream_id, 
                                        const string custom_key,
					NETWORK_STATUS custom_value) {
    std::string network_status;
    switch (custom_value) {
    case NET_UNKNOWN:
      network_status = "Unkown";
      break;
    case NET_GREAT:
      network_status = "Great";
      break;
    case NET_GOOD:
      network_status = "Good";
      break;
    case NET_NOT_GOOD:
      network_status = "Not Good";
      break;
    case NET_BAD:
      network_status = "Bad";
      break;
    case NET_VERY_BAD:
      network_status = "Very Bad";
      break;
    case NET_CANT_CONNECT:
      network_status = "Can't connect";
      break;
    default:
      break;
    }
    SetStreamCustomInfo(stream_id,custom_key,network_status);
  }

  void SessionInfo::AppendStreamCustomInfo(const int stream_id, 
                                           const string custom_key,
                                           const string custom_value) {
    std::lock_guard<std::mutex> guard(session_mutex_);
    StreamInfoPtr search_stream = _GetStreamInfoById(stream_id);
    if(search_stream == NULL) {
      LOG(INFO) << "no stream info(" << stream_id << ") in session:" << session_id_;
      return;
    }
    search_stream->AppendCustomInfo(custom_key,custom_value);
  }

  StreamInfoPtr SessionInfo::GetStreamInfoById(const int stream_id) {
    std::lock_guard<std::mutex> guard(session_mutex_);
    return _GetStreamInfoById(stream_id);
  }

  string SessionInfo::GetAsJson(const std::string room_id) {
    std::lock_guard<std::mutex> guard(session_mutex_);

    // Hide the closed session
    bool show_leave = false;
    if (room_id.length() > 0) {
      if (room_id.compare("all") == 0 || room_id_.compare(room_id) == 0) {
        show_leave = true;
      } else {
        return "";
      }
    } else if (status_ == SESSION_STATUS_DESTORYED) {
      return "";
    }

    std::map<int,StreamInfoPtr>::iterator it;
    std::string return_string = 
      StringPrintf("{ session_id : %d , room_id : \"%s\" , room_type : \"%s\" , status : \"%s\" , create_time : \"%s\", destory_time : \"%s\", stream_infos: [",
        session_id_,
        room_id_.c_str(),
        room_type_.c_str(),
        status_.c_str(),
        GetDateTime(create_time_).c_str(),
        (status_ == SESSION_STATUS_DESTORYED ? GetDateTime(destroyed_time_).c_str() : ""));

    // Resort the stream in statusz
    std::vector<std::pair<int, StreamInfoPtr>> stream_vec(stream_map_.begin(), stream_map_.end());
    std::sort(stream_vec.begin(), stream_vec.end(), ComparatorStream);

    for (auto it = stream_vec.begin(); it != stream_vec.end(); it ++) {
      std::string stream_info = it->second->GetAsJson(show_leave);
      if (stream_info == "") {
        continue;
      }
      return_string +=  stream_info + ",";
    }

    if(',' == return_string.back()) {
      return_string.pop_back();
    }
    return_string += "]}";
    return return_string;
  }

  string SessionInfo::GetAsJson(const int stream_id) {
    LOG(INFO) << "SessionInfo::GetAsJson stream_id : " << stream_id;
    std::lock_guard<std::mutex> guard(session_mutex_);
    std::string return_string = 
      StringPrintf("{ session_id : %d , room_id : \"%s\" , room_type : \"%s\" , status : \"%s\" , create_time : \"%s\", destory_time : \"%s\", stream_infos: [",
        session_id_,
        room_id_.c_str(),
        room_type_.c_str(),
        status_.c_str(),
        GetDateTime(create_time_).c_str(),
        (status_ == SESSION_STATUS_DESTORYED ? GetDateTime(destroyed_time_).c_str() : ""));

    StreamInfoPtr stream_info_ptr = _GetStreamInfoById(stream_id);
    if (stream_info_ptr) {
      return_string += stream_info_ptr->GetAsJson(true) + ",";
    }
    if (',' == return_string.back()) {
      return_string.pop_back();
    }

    return_string += "]}";
    return return_string;
  }

  void SessionInfoManager::AddSession(const int session_id) {
    std::lock_guard<std::mutex> guard(session_manager_mutex_);
    if(_GetSessionInfoById(session_id) != NULL) {
      VLOG(2) << "session id " << session_id << " have added to map,ignored";
      return;
    }
    current_session_count_++;
    total_session_count_++;
    SessionInfoPtr session_info = std::make_shared<SessionInfo> (session_id);
    while(session_vector_.size() >= MAX_SAVE_SESSION_COUNT) {
      session_vector_.pop_back();
    }
    session_vector_.insert(session_vector_.begin(),session_info);
  }

  bool SessionInfoManager::RemoveSession(const int session_id) {
    std::lock_guard<std::mutex> guard(session_manager_mutex_);
    SessionInfoPtr delete_session = _GetSessionInfoById(session_id);
    if(delete_session != NULL) {
      if(delete_session->GetStatus() == SESSION_STATUS_LIVE) {
        current_session_count_--;
      }
      delete_session->_ChangeStateToDestoryed();
      //std::sort(session_vector_.begin(),session_vector_.end(),SessionSortFunction);
      std::sort(session_vector_.begin(),session_vector_.end(),comparator);
      return true;
    }
    return false;
  }



  SessionInfoPtr SessionInfoManager::GetSessionInfoById(const int session_id) {
    std::lock_guard<std::mutex> guard(session_manager_mutex_);
    return _GetSessionInfoById(session_id);
  }

  string SessionInfoManager::GetAsJson(const std::string room_id) {
    std::lock_guard<std::mutex> guard(session_manager_mutex_);
    std::vector<SessionInfoPtr>::iterator it;
    std::string return_string = "[";
    for(it = session_vector_.begin(); it != session_vector_.end(); it++) {
      std::string session_str = (*it)->GetAsJson(room_id);
      if (session_str == "") {
        continue;
      }
      return_string += session_str + ",";
    }
    if(',' == return_string.back()) {
      return_string.pop_back();
    }
    return_string += "]";
    return return_string;
  }

  string SessionInfoManager::GetAsJson(const int session_id, const int stream_id) {
    std::lock_guard<std::mutex> guard(session_manager_mutex_);
    std::string return_string = "[";
    SessionInfoPtr session_info_ptr = _GetSessionInfoById(session_id);
    if (session_info_ptr) {
      return_string += session_info_ptr->GetAsJson(stream_id) + ",";
    }

    if (',' == return_string.back()) {
      return_string.pop_back();
    }

    return_string += "]";
    return return_string;
  }

  //private method
  StreamInfoPtr SessionInfo::_GetStreamInfoById(const int stream_id) {
    auto search = stream_map_.find(stream_id);
    if (search != stream_map_.end()) {
      return search->second;
    }
    return NULL;
  }

  SessionInfoPtr SessionInfoManager::_GetSessionInfoById(const int session_id) {
    std::vector<SessionInfoPtr>::iterator it;
    for(it = session_vector_.begin(); it != session_vector_.end(); it++) {
      if( (*it)->GetSessionId() == session_id) {
        return *it;
      }
    }
    return NULL;
  }

}  // namespace orbit
