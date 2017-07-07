# The BUILD rules for webrtc chrome implementation and related module.

package(default_visibility = ["//visibility:public"])
 	 	
cc_library(
  name = "webrtc_common",
  srcs = [
          "common_types.cc",
         ],
  hdrs = [
          "common_types.h",
          "typedefs.h",
          "video_frame.h",
          "engine_configurations.h"
         ],
  deps = [
          "//third_party/glog",
          "//stream_service/orbit:webrtc_includes",
         ],
)

cc_library(
  name = "webrtc_format_util",
  srcs = [
          "webrtc_format_util.cc",
         ],
  hdrs = [
          "webrtc_format_util.h",
         ],
  deps = [
          "//third_party/glog",
          "//stream_service/orbit:webrtc_includes",
          "//stream_service/orbit/webrtc/modules/rtp_rtcp:rtp_rtcp",
         ],
)


cc_library(
  name = "rtc_event_log",
  hdrs = [
    "call/rtc_event_log.h",
  ],
  srcs = [
    "call/rtc_event_log.cc",
  ],
  deps = [
    "//stream_service/orbit:webrtc_includes",
    ":webrtc_common",
  ]
)