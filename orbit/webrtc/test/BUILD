# The BUILD rules.

package(default_visibility = ["//visibility:public"])

cc_library(
  name = "histogram",
  hdrs = [
           "histogram.h",
         ],
  srcs = [
           "histogram.cc",
         ],
  deps = [
          "//stream_service/orbit/webrtc/base:rtc_base",
          "//third_party/glog",
         ],
)

cc_library(
  name = "rtp_file_reader",
  hdrs = [
           "rtp_file_reader.h",
         ],
  srcs = [
           "rtp_file_reader.cc",
         ],
  deps = [
          "//stream_service/orbit/webrtc/base:rtc_base",
          "//stream_service/orbit/webrtc/modules/rtp_rtcp",
          "//third_party/glog",
         ],
)

cc_library(
  name = "rtcp_packet_parser",
  hdrs = [
           "rtcp_packet_parser.h",
  ],
  srcs = [
           "rtcp_packet_parser.cc",
         ],
  deps = [
          "//stream_service/orbit/webrtc/base:rtc_base",
          "//third_party/glog",
          "//third_party/gtest",
         ],
)

cc_library(
  name = "null_transport",
  hdrs = [
           "null_transport.h",
  ],
  srcs = [
           "null_transport.cc",
         ],
  deps = [
          "//stream_service/orbit/webrtc/base:rtc_base",
          "//third_party/glog",
          "//third_party/gtest",
         ],
)
