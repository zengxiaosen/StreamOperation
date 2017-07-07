/*
 * common_defines.h
 *
 *  Created on: Mar 7, 2016
 *      Author: vellee
 */

#ifndef GSTREAMER_COMMON_DEFINES_H_
#define GSTREAMER_COMMON_DEFINES_H_

#include <gstreamer-1.5/gst/gst.h>
#include "string.h"
#include "stream_service/orbit/media_definitions.h"
namespace orbit{
/**
 * Check rtmp location is valid or not
 */
  bool IsValidRtmpLocation(const char* rtmp_location);
  GstPad* RequestElementPad(GstElement *element, char* templete);
}
#endif /* GSTREAMER_COMMON_DEFINES_H_ */
