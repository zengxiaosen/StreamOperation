/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * http_common.cc
 * ---------------------------------------------------------------------------
 * Implements the http common utils and requests/response model.
 * ---------------------------------------------------------------------------
 */
#include "http_common.h"

namespace orbit {

HttpRequest::HttpRequest() : method_(METHOD_UNKNOWN), method_string_(""), url_(""), temp_data_("") {
}

HttpRequest::HttpRequest(const std::string& uri) : method_(METHOD_UNKNOWN), url_(uri) {
}

HttpRequest::HttpRequest(const HttpRequest& other) = default;

HttpRequest::~HttpRequest() {
}

HttpResponse::HttpResponse() : code_(HTTP_OK), content_type_("text/plain") {
}

HttpResponse::~HttpResponse() {
}

}  // namespace orbit
