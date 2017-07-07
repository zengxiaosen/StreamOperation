/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * http_server.cc
 * ---------------------------------------------------------------------------
 * Implements the simple embeded http server.
 * ---------------------------------------------------------------------------
 */

#include "http_server.h"
#include <set>
#include <map>
#include <memory>
#include <vector>
#include <utility>
#include <arpa/inet.h>

// For Gflags and Glog
#include "gflags/gflags.h"
#include "glog/logging.h"
#include "stream_service/orbit/logger_helper.h"
#include "stream_service/orbit/base/strutil.h"
#include "stream_service/orbit/base/zlib_util.h"

#include "assert.h"

using namespace std;

DEFINE_int32(default_http_port, 18081, "Default http server's port.");
DEFINE_bool(use_http_compress, true, "Use compression for http response.");
DEFINE_string(http_cache_control_policy, "max-age=3600", "Defines the policy"
              " for cache control field in the HttpResponse.");

#define ORBIT_COOKIE_MAGIC "orbit_cookie"

namespace orbit {
namespace {
static bool ReadCertificates(string server_pem, string server_key,
                             string* cert_pem_value, string* cert_key_value) {
  char* cert_pem_bytes = NULL;
  char* cert_key_bytes = NULL;

  /* Read certificate and key */
  FILE *pem = fopen(server_pem.c_str(), "rb");
  if(pem) {
    fseek(pem, 0L, SEEK_END);
    size_t size = ftell(pem);
    fseek(pem, 0L, SEEK_SET);
    cert_pem_bytes = (char*)malloc(size+1);
    char *index = cert_pem_bytes;
    int read = 0, tot = size;
    while((read = fread(index, sizeof(char), tot, pem)) > 0) {
      tot -= read;
      index += read;
    }
    fclose(pem);
  } else {
    LOG(FATAL) << "Error in reading server_pem. Aborted.";
  }
  if (cert_pem_bytes != NULL) {
    *cert_pem_value = cert_pem_bytes;
    free(cert_pem_bytes);
  }
  
  FILE *key = fopen(server_key.c_str(), "rb");
  if(key) {
    fseek(key, 0L, SEEK_END);
    size_t size = ftell(key);
    fseek(key, 0L, SEEK_SET);
    cert_key_bytes = (char*)malloc(size+1);
    char *index = cert_key_bytes;
    int read = 0, tot = size;
    while((read = fread(index, sizeof(char), tot, key)) > 0) {
      tot -= read;
      index += read;
    }
    fclose(key);
  } else {
    LOG(FATAL) << "Error in reading server_key. Aborted.";
  }
  if (cert_key_bytes != NULL) {
    *cert_key_value = cert_key_bytes;
    free(cert_key_bytes);
  }
  return true;
}

/* Connection notifiers */
static int HttpClientConnect(void *cls, const struct sockaddr *addr, socklen_t addrlen) {
	struct sockaddr_in *sin = (struct sockaddr_in *)addr;
	char *ip = inet_ntoa(sin->sin_addr);
  ELOG_INFO2("HTTP connect request from ip:%s", ip);
	return MHD_YES;
}

/* Connection notifiers */
static void HttpRequestCompleted(void *cls, struct MHD_Connection *connection, void **con_cls, enum MHD_RequestTerminationCode toe) {
  ELOG_INFO2("HTTP request completed.");
}

int KeyValueIterator(void *cls, enum MHD_ValueKind kind, const char *key, const char *value)
{
  if (key == NULL || value == NULL) {
    return MHD_NO;
  }

  HttpRequest *handler = reinterpret_cast<HttpRequest *>(cls);
    
  switch(kind) {
  case MHD_HEADER_KIND:
    handler->AddHeader(key, value);
    break;
  case MHD_COOKIE_KIND:
    handler->AddHeader(key, value);
    break;
  case MHD_GET_ARGUMENT_KIND:
    handler->AddQueryParam(key, value);
    break;
  default:
    break;
  }   
  return MHD_YES;
}

static int PostKeyValueIterator (void *cls,
                                 enum MHD_ValueKind kind,
                                 const char *key,
                                 const char *filename,
                                 const char *content_type,
                                 const char *transfer_encoding,
                                 const char *data, uint64_t off, size_t size) {
  // NOTE(chengxu): libmhd has a weird model for processing POST data.
  // If the POST_DATA is exceeding a certain size threshold, the DATA will not
  // be fed into this function for just one time. While, The PostKeyValueIterator
  // will be called for multiple time, and each time it will have such kind of data"
  //  key  -  same as last time.
  //  data -  the new data
  //  off  -  the offsite of the data in the original POST_DATA
  //  size -  the size of new data.
  // Example:
  //   a=1&&b=1234abcd5678efgh as example.
  //  Assume that the libmhd will call the PostKeyValueIterator in this sequence.
  //   1. PostKeyValueIterator key=a data=1 off=0 size=1
  //   2. PostKeyValueIterator key=b data=1234 off=0 size=4
  //   3. PostKeyValueIterator key=b data=abcd off=4 size=4
  //   3. PostKeyValueIterator key=b data=5678 off=8 size=4
  //   3. PostKeyValueIterator key=b data=efgh off=12 size=4
  if (size != 0) { 
    HttpRequest *handler = reinterpret_cast<HttpRequest *>(cls);
    // For off (offsite), we don't use right now.
    string value(data, size);
    switch(kind) {
    case MHD_COOKIE_KIND:
      VLOG(3) << "AddHeader(" << key << "," << value << ");";
      handler->AppendHeader(key, value);
      break;
    case MHD_HEADER_KIND:
      VLOG(3) << "AddHeader(" << key << "," << value << ");";
      handler->AppendHeader(key, value);
      break;
    case MHD_POSTDATA_KIND:
      VLOG(3) << "AddQueryParam(" << key << "," << value << ");";
      handler->AppendQueryParam(key, value);
      break;
    default:
      LOG(ERROR) << "Unsupported form key=" << key << " kind=" << kind;
      break;
    }   
  }
  return MHD_YES;
}

static bool MaybeCompressContent(struct MHD_Connection *connection,
                                 const std::string& buf,
                                 string* compressed_buf,
                                 string* content_encoding) {
    /* Did the client request compression? */
    const char *encodings = MHD_lookup_connection_value (connection,
                                                         MHD_HEADER_KIND,
                                                         MHD_HTTP_HEADER_ACCEPT_ENCODING);
    if (encodings == NULL) encodings = "";
    // NOTE(chengxu): so far, we support only the deflate (zlib) compression method.
    // Add gzip method in the future.
    if (strstr (encodings, "deflate") != NULL && FLAGS_use_http_compress) {
      LOG(INFO) << "response_page_size=" << buf.size();
      *compressed_buf = ZlibCompress(buf);
      LOG(INFO) << "compressed_response_page_size=" << compressed_buf->size();
      *content_encoding = "deflate";
    } else {
      *compressed_buf = buf;
      *content_encoding = "";
      return false;
    }
    return true;
}

// The common handler function to handle the request. 
static int HttpRequestHandler(void * cls,
                              struct MHD_Connection * connection,
                              const char * url,
                              const char * method,
                              const char * version,
                              const char * upload_data,
                              size_t * upload_data_size,
                              void ** ptr) {
  static int dummy;
  HttpServer* http_server = reinterpret_cast<HttpServer*>(cls);
  struct MHD_Response * response;
  int ret;
  if (0 != strcmp (method, MHD_HTTP_METHOD_POST) &&
      0 != strcmp (method, MHD_HTTP_METHOD_GET)) {
    LOG(ERROR) << "method OPTION...: Not implemented yet.";
    return MHD_NO; /* unexpected method */
  }

  /* Is this the first round? */
  if (*ptr == NULL) {
    /* The first time only the headers are valid,
       do not respond in the first round... */
    ELOG_INFO2("Got a HTTP %s request on %s...\n", method, url);
    VLOG(3) << "first time..." << method << "/" << url << "/" << *upload_data_size;
    *ptr = &dummy;
    return MHD_YES;
  }
   
  /*
    MHD_HEADER_KIND        --- HTTP header.
    MHD_COOKIE_KIND        --- Cookies. 
    MHD_POSTDATA_KIND      --- POST data. 
    MHD_GET_ARGUMENT_KIND  --- URL argument
   */
  HttpRequest* request;
  if (0 == strcmp (method, MHD_HTTP_METHOD_GET)) {
    if (0 != *upload_data_size) {
      LOG(ERROR) << "Upload data is not yet supported.";
      const char * data = "<html><body><p>Error!</p></body></html>";
      response = MHD_create_response_from_data(strlen(data),
                                               (void*)data,
                                               MHD_NO,
                                               MHD_NO);
      ret = MHD_queue_response(connection, MHD_HTTP_NOT_IMPLEMENTED, response);
      return ret; /* upload data in a GET!? */
    }
    
    *ptr = NULL; /* clear context pointer */

    request = new HttpRequest;
    *ptr = request;
    request->set_method(method);
    request->set_url(url);
    if(strncmp(method, MHD_HTTP_METHOD_GET, strlen(MHD_HTTP_METHOD_GET)) == 0) {
      // Get the GET argument.
      MHD_get_connection_values(connection, MHD_GET_ARGUMENT_KIND, KeyValueIterator,
                                reinterpret_cast<void *>(request));
      // Get the cookie.
      MHD_get_connection_values(connection, MHD_COOKIE_KIND, KeyValueIterator,
                                reinterpret_cast<void *>(request));
    }
  } else {
    // For POST, we will handle it later.
    VLOG(3) << "Handle POST.";
    VLOG(3) << "upload_data_size=" << *upload_data_size;
    // For POST, first time is the situation that upload_data_size is not equal to 0. 
    // This time we should not proceed with the HttpHandler, but we just parse the POST
    // data until the upload_data_size is equal to zero. At that time, we will go with
    // http handler to process it.
    if (0 != *upload_data_size) {
      if (*ptr == &dummy) {
        request = new HttpRequest;
        *ptr = request;
      }
      // Get the COOKIES
      MHD_get_connection_values(connection, MHD_COOKIE_KIND, KeyValueIterator,
                                reinterpret_cast<void *>(request));
      request->AppendTempData(upload_data, *upload_data_size);
      *upload_data_size = 0;
      return MHD_YES;
    } else {
      // *upload_data_size == 0
      request = static_cast<HttpRequest*>(*ptr);
      assert(request != NULL);
      if (request == NULL) {
        LOG(ERROR) << "request is NULL. Exit.";
        return MHD_NO; /* internal error */
      }
      struct MHD_PostProcessor *pp;
      // Get the POST_DATA 
      pp = MHD_create_post_processor(connection, 1024 * 16,
                                     &PostKeyValueIterator,
                                     reinterpret_cast<void *>(request));
      if (NULL == pp) {
        LOG(ERROR) << "Failed to setup post processor for " << url;
        delete request;
        return MHD_NO; /* internal error */
      } else {
        MHD_post_process (pp,
                          request->GetTempData(),
                          request->GetTempDataSize());
        /* done with POST data, serve response */
        MHD_destroy_post_processor(pp);
        *ptr = request;
        request->set_method(method);
        request->set_url(url);
      }
    }
  }
  http_server->DebugHttpSessions();

  // At this point, if this is HTTP_POST, the upload_data_size=0, thus we get the request from ptr.
  // If this is HTTP_GET, then it will be good to use the request
  request = (HttpRequest*)*ptr;
  VLOG(3) << "url=" << url;
  // Assume that the url is /create/123456, we should be able to extract /create as the root url.
  string url_str = url;
  string root_url = url;
  vector<string> result;
  SplitStringUsing(url_str, "/", &result);
  if (result.size() > 1) {
    root_url = "/" + result[0];
  }
  HttpHandler* handler = http_server->GetHttpHandler(root_url);

  // Check if there is a COOKIE, if not, we should set one.
  string cookie_value;
  HttpSession session_value;
  string should_create_cookie_value;
  if (!request->GetHeaderValue(ORBIT_COOKIE_MAGIC, &cookie_value)) {
    cookie_value = http_server->CreateSession(&session_value);
    should_create_cookie_value = cookie_value;
  } else {
    bool ret = http_server->FindSessionByCookie(cookie_value, &session_value);
    if (!ret) {
      session_value.SetSessionId(cookie_value.c_str());
      http_server->UpdateSession(cookie_value, session_value);
    }
  }

  string response_page;
  string content_type = "text/plain";
  string content_encoding = "";
  int http_code = MHD_HTTP_OK;
  string redirect_location;
  if (handler == NULL) {
    // Create a error response....
    response_page += "ERROR URL:";
    response_page += url;
    http_code = MHD_HTTP_NOT_FOUND;
  } else {
    std::shared_ptr<HttpResponse> response(new HttpResponse());
    request->SetSessionData(session_value);
    response->SetSessionData(session_value);

    handler->HandleRequestInternal(*request, response);
    http_code = response->code();
    if (!response->content_type().empty()) {
      content_type = response->content_type();
    }
    string raw_response_page = response->content();
    MaybeCompressContent(connection, raw_response_page,
                         &response_page, &content_encoding);
    const HttpSession& update_session_value = response->GetSessionData();
    http_server->UpdateSession(cookie_value, update_session_value);

    redirect_location = response->location();
  }

  const char* page = response_page.c_str();
  response = MHD_create_response_from_data(response_page.size(),
                                           (void*)page,
                                           MHD_NO,
                                           MHD_RESPMEM_MUST_COPY);
	MHD_add_response_header(response,
                          MHD_HTTP_HEADER_CONTENT_TYPE, // "Content-Type"
                          content_type.c_str());
  if (!content_encoding.empty()) {
    MHD_add_response_header(response,
                            MHD_HTTP_HEADER_CONTENT_ENCODING, // "Content-Encoding"
                            content_encoding.c_str());
    // If the file is served on the http server. Add a cache control to it.
    if (content_type == "text/javascript" ||
        content_type == "text/css") {
      MHD_add_response_header(response,MHD_HTTP_HEADER_CACHE_CONTROL,
                              FLAGS_http_cache_control_policy.c_str()); 
    }
  }

  if (!should_create_cookie_value.empty()) {
    char cstr[256];
    snprintf (cstr, sizeof (cstr),
              "%s=%s", ORBIT_COOKIE_MAGIC, should_create_cookie_value.c_str());
    if (MHD_NO == MHD_add_response_header(response,
                                          MHD_HTTP_HEADER_SET_COOKIE,
                                          cstr)) {
      LOG(ERROR) << "Failed to set session cookie header!";
    }
  }

  if (http_code == HTTP_MOVED_PERMANENTLY ||
      http_code == HTTP_TEMPORARY_REDIRECT) {
    LOG(INFO) << "HEADER_location" << redirect_location;
    MHD_add_response_header(response,
                            MHD_HTTP_HEADER_LOCATION,
                            redirect_location.c_str());
  }

  ret = MHD_queue_response(connection, http_code, response);
  MHD_destroy_response(response);
  delete request;
  return ret;
}

}  // annoymouse namespace

HttpServer::HttpServer() : HttpServer(FLAGS_default_http_port, TYPE_HTTP) {
}

HttpServer::HttpServer(int port, Type type) {
  started_ = false;
  SetPort(port);
  SetThreadPool(0);
  type_ = type;
  handlers_.reset(new HttpHandlerMap());
  session_manager_.reset(new HttpSessionManager());
}

HttpServer::~HttpServer() {
}

bool HttpServer::StartServer() {
  string cert_pem_value;
  string cert_key_value;

  if (type_ == TYPE_HTTPS) {
    // HTTPS connection
    ReadCertificates(server_pem_, server_key_, &cert_pem_value, &cert_key_value);
    daemon_ = MHD_start_daemon(MHD_USE_SSL | MHD_USE_SELECT_INTERNALLY,
                               port_,
                               &HttpClientConnect,
                               NULL,
                               &HttpRequestHandler,
                               this,  // Pass the http_server instance to handler.
                               MHD_OPTION_THREAD_POOL_SIZE, thread_number_,
                               MHD_OPTION_NOTIFY_COMPLETED, &HttpRequestCompleted, NULL,
                               MHD_OPTION_HTTPS_MEM_CERT, cert_pem_value.c_str(),
                               MHD_OPTION_HTTPS_MEM_KEY, cert_key_value.c_str(),
                               MHD_OPTION_END);
  } else {
    // HTTP connection
     daemon_ = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION,
                                port_,
                                &HttpClientConnect,
                                NULL,
                                &HttpRequestHandler,
                                this,  // Pass the http_server instance to handler.
                                MHD_OPTION_NOTIFY_COMPLETED, &HttpRequestCompleted, NULL,
                                (void*)MHD_OPTION_END);
  }
  if (daemon_ == NULL) {
    LOG(ERROR) << "Start http server daemon buf failed...";
    LOG(FATAL) << "The http port " << port_ << " may have been used.";
    return false;
  }
  started_ = true;
  return true;
}

bool HttpServer::Stop() {
  if (started_) {
    // Stop the server.
    MHD_stop_daemon(daemon_);
    started_ = false;
  }
  return true;
}

bool HttpServer::RegisterHandler(const string& uri,
                                 HttpHandler* handler) {
  if (handlers_->find(uri) != handlers_->end()) {
    LOG(ERROR) << "Register handler for same uri:" << uri;
    // TODO(chengxu): maybe we should make the handlers as a chain function?
    return false;
  }
  (*handlers_)[uri] = handler;
  return true;
}

HttpHandler* HttpServer::GetHttpHandler(const string& uri) {
  if (handlers_->find(uri) != handlers_->end()) {
    return (*handlers_)[uri];
  } else {
    return NULL;
  }
}

void HttpServer::SetThreadPool(int thread_number) {
  thread_number_ = thread_number;
}

}
