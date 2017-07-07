/*
 * common_defines.cc
 *
 *  Created on: Mar 7, 2016
 *      Author: vellee
 */

#include "string.h"
#include "gst/gst.h"
#include "stream_service/orbit/live_stream/common_defines.h"
#define RTMP_SCHEME "rtmp://"
#define RTMP_SCHEME_LENGTH strlen(RTMP_SCHEME)

namespace orbit{

  bool IsValidRtmpLocation(const char* rtmp_location){
    bool is_valid_rtmp_location = false;
    if(rtmp_location) {
      if(strncmp(rtmp_location,RTMP_SCHEME, RTMP_SCHEME_LENGTH) == 0){
        is_valid_rtmp_location = true;
      }
    }
    return is_valid_rtmp_location;
  }
  GstPad* RequestElementPad(GstElement *element, char* templete) {
    GstPadTemplate *pad_templete = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(element), templete);
    GstPad *mixer_sink_pad = gst_element_request_pad(element, pad_templete, NULL, NULL);
    gst_object_unref(pad_templete);
    return mixer_sink_pad;
  }
}
