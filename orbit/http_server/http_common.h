/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * http_common.h
 * ---------------------------------------------------------------------------
 * Defines the http codes constants and the request/response model.
 * ---------------------------------------------------------------------------
 */
#ifndef ORBIT_HTTP_COMMON_H__
#define ORBIT_HTTP_COMMON_H__

#include <map>
#include <string>
#include <vector>
#include <mutex>

#include <string.h>
#include "glog/logging.h"

namespace orbit {

//////////////////////////////////////////////////////////////////////
// Constants
//////////////////////////////////////////////////////////////////////

// HTTP status codes.
// Taken from RFC 2616 Section 10.
enum HttpStatusCode {
  // Informational 1xx
  HTTP_CONTINUE = 100,
  HTTP_SWITCHING_PROTOCOLS = 101,

  // Successful 2xx
  HTTP_OK = 200,
  HTTP_CREATED = 201,
  HTTP_ACCEPTED = 202,
  HTTP_NON_AUTHORITATIVE_INFORMATION = 203,
  HTTP_NO_CONTENT = 204,
  HTTP_RESET_CONTENT = 205,
  HTTP_PARTIAL_CONTENT = 206,

  // Redirection 3xx
  HTTP_MULTIPLE_CHOICES = 300,
  HTTP_MOVED_PERMANENTLY = 301,
  HTTP_FOUND = 302,
  HTTP_SEE_OTHER = 303,
  HTTP_NOT_MODIFIED = 304,
  HTTP_USE_PROXY = 305,
  // 306 is no longer used.
  HTTP_TEMPORARY_REDIRECT = 307,

  // Client error 4xx
  HTTP_BAD_REQUEST = 400,
  HTTP_UNAUTHORIZED = 401,
  HTTP_PAYMENT_REQUIRED = 402,
  HTTP_FORBIDDEN = 403,
  HTTP_NOT_FOUND = 404,
  HTTP_METHOD_NOT_ALLOWED = 405,
  HTTP_NOT_ACCEPTABLE = 406,
  HTTP_PROXY_AUTHENTICATION_REQUIRED = 407,
  HTTP_REQUEST_TIMEOUT = 408,
  HTTP_CONFLICT = 409,
  HTTP_GONE = 410,
  HTTP_LENGTH_REQUIRED = 411,
  HTTP_PRECONDITION_FAILED = 412,
  HTTP_REQUEST_ENTITY_TOO_LARGE = 413,
  HTTP_REQUEST_URI_TOO_LONG = 414,
  HTTP_UNSUPPORTED_MEDIA_TYPE = 415,
  HTTP_REQUESTED_RANGE_NOT_SATISFIABLE = 416,
  HTTP_EXPECTATION_FAILED = 417,

  // Server error 5xx
  HTTP_INTERNAL_SERVER_ERROR = 500,
  HTTP_NOT_IMPLEMENTED = 501,
  HTTP_BAD_GATEWAY = 502,
  HTTP_SERVICE_UNAVAILABLE = 503,
  HTTP_GATEWAY_TIMEOUT = 504,
  HTTP_VERSION_NOT_SUPPORTED = 505,
};

// Methods of HTTP requests supported by the test HTTP server.
enum HttpMethod {
  METHOD_UNKNOWN,
  METHOD_GET,
  METHOD_HEAD,
  METHOD_POST,
  METHOD_PUT,
  METHOD_DELETE,
  METHOD_PATCH,
  METHOD_CONNECT,
  METHOD_OPTIONS,
};

// functional for insensitive std::string compare
struct iless {
  bool operator()(const std::string& lhs, const std::string& rhs) const {
    return (strcasecmp(lhs.c_str(), rhs.c_str()) < 0);
  }
};

//////////////////////////////////////////////////////////////////////
// HttpSession
//////////////////////////////////////////////////////////////////////
// Represents a HTTP session using cookie and in-memory session data. 

class HttpSession {
  typedef std::map<std::string, std::string, iless> SessionKeyValueMap;
  static const int kSessionSize = 33;
 public:
  HttpSession() {
    gen_random(sid_, kSessionSize);
  }
  void SetSessionId(const char* sid) {
    memcpy(sid_, sid, kSessionSize);
  }
  std::string GetSessionId() const {
    return sid_;
  }
  long GetLastActiveTime() {
    return last_active_;
  }

  bool GetValue(const std::string& key, std::string* value) const {
    auto mvalue = session_data_.find(key);
    if (mvalue == session_data_.end()) {
      *value = "";
      return false;
    }
    *value = mvalue->second;
    return true;
  }

  bool SetValue(const std::string& key, const std::string& value) {
    session_data_.insert(std::make_pair(key, value));
    return true;
  }

  bool DeleteValue(const std::string& key) {
    auto it = session_data_.find(key);
    session_data_.erase(it);
    return true;
  }

  void DebugPrint() const {
    for (auto it : session_data_) {
      LOG(INFO) << "key=" << it.first << " Value:" << it.second;
    }
  }

 private:
   void gen_random(char *s, const int len) {
     static const char alphanum[] =
       "0123456789"
       "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
       "abcdefghijklmnopqrstuvwxyz";
     
     for (int i = 0; i < len; ++i) {
       s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
     }
     
     s[len] = 0;
   }

  /**
   * Unique ID for this session. 
   */
  char sid_[kSessionSize];

  /**
   * Time when this session was last active.
   */
  long last_active_;

  /**
   * Key-value map for each session.
   */
  SessionKeyValueMap session_data_;
};

// The manager class to manage all the session and the session-specific data.
 class HttpSessionManager {
   typedef std::map<std::string, HttpSession> HttpSessionMap;
  public:
   HttpSessionManager() {
     srand(time(NULL));
   }
   std::string CreateSession(HttpSession* session_value) {
     std::lock_guard<std::mutex> guard(var_mutex_);
     std::string cookie_value = session_value->GetSessionId();
     var_map_.insert(make_pair(cookie_value, *session_value));
     return cookie_value;
   }

   bool FindSessionByCookie(std::string cookie_value, HttpSession* session_value) {
     std::lock_guard<std::mutex> guard(var_mutex_);
     auto search = var_map_.find(cookie_value);
     if (search != var_map_.end()) {
       *session_value = search->second;
       return true;
     }
     return false;
   }

   void DebugHttpSessions() {
     for (auto it : var_map_) {
       VLOG(3) << "cookie_name=" << it.first;
       it.second.DebugPrint();
     }
   }

   bool UpdateSession(std::string cookie_value, const HttpSession& session_value) {
     std::lock_guard<std::mutex> guard(var_mutex_);
     var_map_[cookie_value] = session_value;
     return true;
   }
  private:
   std::mutex var_mutex_;
   HttpSessionMap var_map_;
   //DEFINE_AS_SINGLETON(HttpSessionManager);
 };


//////////////////////////////////////////////////////////////////////
// HttpRequest
//////////////////////////////////////////////////////////////////////

// Represents a HTTP request. 
// Example usage:
/*
  std::shared_ptr<HttpResponse> MyHander::HandleRequest(
      const HttpRequest& request) {
   for (auto query : request.GetQueryList()) {
    LOG(INFO) << "key=" << query.first << " value=" << query.second;
   }

   string value;
   if (request.GetQueryValue("query", &value)) {
     LOG(INFO) << "value=" << value;
   }

   for (auto header : request.GetHeaderList()) {
     LOG(INFO) << "header_key=" << header.first << " header_value=" << header.second;
   }
 }
*/
class HttpRequest {
  typedef std::map<std::string, std::string, iless> HeaderMap;
  typedef std::map<std::string, std::string, iless> QueryMap;
 public:
  HttpRequest();
  HttpRequest(const std::string& query);
  HttpRequest(const HttpRequest& other);
  ~HttpRequest();

  void AddHeader(const char* key, const char* value) {
    std::string key_str(key);
    std::string value_str(value);
    AddHeader(key_str, value_str);
  }

  void AddHeader(const std::string& key, const std::string& value) {
    header_map_.insert(std::make_pair(key, value));
  }

  void AppendHeader(const std::string& key, const std::string& value) {
    std::string old_value;
    std::string new_value = value;
    if (GetHeaderValue(key, &old_value)) {
      new_value = old_value + value;
      header_map_[key] = new_value;
      return;
    } else {
      header_map_.insert(std::make_pair(key, value));
      return;
    }
  }

  void AddQueryParam(const char* key, const char* value) {
    std::string key_str(key);
    std::string value_str(value);
    AddQueryParam(key_str, value_str);
  }

  void AddQueryParam(const std::string& key, const std::string& value) {
    query_map_.insert(std::make_pair(key, value));
  }

  void AppendQueryParam(const std::string& key, const std::string& value) {
    std::string old_value;
    std::string new_value = value;
    if (GetQueryValue(key, &old_value)) {
      new_value = old_value + value;
      query_map_[key] = new_value;
      return;
    } else {
      query_map_.insert(std::make_pair(key, new_value));
      return;
    }
  }

  bool GetQueryValue(const std::string& key, std::string* value) const {
    auto mvalue = query_map_.find(key);
    if (mvalue == query_map_.end()) {
      *value = "";
      return false;
    }
    *value = mvalue->second;
    return true;
  }

  bool GetHeaderValue(const std::string& key, std::string* value) const {
    auto mvalue = header_map_.find(key);
    if (mvalue == header_map_.end()) {
      *value = "";
      return false;
    }
    *value = mvalue->second;
    return true;
  }

  const QueryMap& GetQueryList() const {
    return query_map_;
  }
  const HeaderMap& GetHeaderList() const {
    return header_map_;
  }

  HttpMethod method() const {
    return method_;
  }
  void set_method(std::string method_string) {
    method_string_ = method_string;
    if (strcasecmp(method_string_.c_str(), "GET") == 0) {
      method_ = METHOD_GET;
    } else if (strcasecmp(method_string_.c_str(), "HEAD") == 0) {
      method_ = METHOD_HEAD;
    } else if (strcasecmp(method_string_.c_str(), "GET") == 0) {
      method_ = METHOD_POST;
    } else if (strcasecmp(method_string_.c_str(), "PUT") == 0) {
      method_ = METHOD_PUT;
    } else if (strcasecmp(method_string_.c_str(), "OPTIONS") == 0) {
      method_ = METHOD_OPTIONS;
    } else {
      method_ = METHOD_UNKNOWN;
    }
  }
  void set_url(std::string url) {
    url_ = url;
  }
  std::string url() const {
    return url_;
  }

  void SetSessionData(HttpSession session_data) {
    session_data_ = session_data;
  }

  const HttpSession& GetSessionData() const {
    return session_data_;
  }

  void AppendTempData(const char* upload_data, int upload_data_size) {
    std::string upload_str = std::string(upload_data, upload_data_size);
    if (temp_data_.empty()) {
      temp_data_ = upload_str;
    } else {
      temp_data_ = temp_data_ + std::string(upload_data, upload_data_size);
    }
  }
  const char* GetTempData() {
    return temp_data_.c_str();
  }
  int GetTempDataSize() {
    return temp_data_.size();
  }

 private:
  QueryMap query_map_;
  HeaderMap header_map_;

  HttpMethod method_;
  std::string method_string_;
  std::string url_;

  std::string temp_data_;

  HttpSession session_data_;
};


//////////////////////////////////////////////////////////////////////
// HttpResponse
//////////////////////////////////////////////////////////////////////

// This class represents a basic HTTP responses with commonly used
// response headers such as "Content-Type".
class HttpResponse {
 public:
  HttpResponse();
  ~HttpResponse();

  // The response code.
  HttpStatusCode code() const { return code_; }
  void set_code(HttpStatusCode code) { code_ = code; }
  // The content of the response.
  const std::string& content() const { return content_; }
  void set_content(const std::string& content) { content_ = content; }
  // The content type.
  const std::string& content_type() const { return content_type_; }
  void set_content_type(const std::string& content_type) {
    content_type_ = content_type;
  }

  void SetSessionDataValue(const std::string& key, const std::string& value) {
    session_data_.SetValue(key, value);
  }
  std::string GetSessionDataValue(const std::string& key) {
    std::string value;
    if (session_data_.GetValue(key, &value)) {
      return value;
    }
    return "";
  }

  void SetSessionData(HttpSession session_data) {
    session_data_ = session_data;
  }

  const HttpSession& GetSessionData() const {
    return session_data_;
  }

  const std::string& location() {
    return location_;
  }
  void set_location(const std::string& location) {
    location_ = location;
  }

  void CopyFrom(const HttpResponse* response) {
    code_ = response->code_;
    content_ = response->content_;
    content_type_ = response->content_type_;
    session_data_ = response->session_data_;
    location_ = response->location_;
  }
 private:

  HttpStatusCode code_;
  std::string content_;
  std::string content_type_;
  std::string location_;

  HttpSession session_data_;
};

}  // namespace orbit

#endif  // ORBIT_HTTP_COMMON_H__
