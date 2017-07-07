# rtp_processor BUILD file

package(default_visibility = ["//visibility:public"])

cc_library(
  name = "rtp_headers",
  hdrs = [
          "rtp_headers.h",
         ],
  deps = [
          "//third_party/glog"
         ],
)

cc_library(
  name = "rtp_format_util",
  srcs = [
          "rtp_format_util.cc",
         ],
  hdrs = [
          "rtp_format_util.h",
         ],
  deps = [
          ":rtp_headers",
          "//stream_service/orbit:media_definitions",
          "//stream_service/orbit/webrtc:webrtc_format_util",
          "//third_party/glog"
         ],
)

cc_library(
  name = "rtcp_sender",
  srcs = [
          "rtcp_sender.cc",
         ],
  hdrs = [
          "rtcp_sender.h",
         ],
  copts = [
           "-I/usr/include/glib-2.0",
           "-I/usr/lib/x86_64-linux-gnu/glib-2.0/include",
         ],
  linkopts = [
         "-lgobject-2.0 -lglib-2.0 -lboost_thread -lboost_system ",
         ],
  deps = [
          ":rtp_headers",
          "//stream_service/orbit:sdp_info",
          "//stream_service/orbit:webrtc_includes",
          "//third_party/glog"
         ],
)

#cc_test(
# name = "rtcp_sender_test",
# srcs = [
#        "rtcp_sender_test.cc",
# ],
# copts = [
#         "-I/usr/include/glib-2.0",
#         "-I/usr/lib/x86_64-linux-gnu/glib-2.0/include",
#         ],
# linkopts = [
#         "-lgobject-2.0 -lglib-2.0 -lboost_thread -lboost_system "],
# deps = [
#   ":rtcp_sender",
  # "//stream_service/orbit:sdp_info",
  # "//stream_service/orbit:transport_delegate",
#   "//stream_service/orbit:orbit_webrtc_endpoint",
#   "//third_party/gtest:gtest_main",
# ],
#)

cc_library(
  name = "rtcp_receiver",
  srcs = [
          "rtcp_receiver.cc",
         ],
  hdrs = [
          "rtcp_receiver.h",
         ],
  copts = [
           "-I/usr/include/glib-2.0",
           "-I/usr/lib/x86_64-linux-gnu/glib-2.0/include",
         ],
  linkopts = [
           "-lgobject-2.0 -lglib-2.0 -lboost_thread -lboost_system "],
  deps = [
          "//stream_service/orbit/rtp:janus_rtcp_processor",
          "//stream_service/orbit:media_definitions",
          "//third_party/glog"  
         ],
)

cc_library(
  name = "remb_processor",
  srcs = [
         "remb_processor.cc"
  ],
  hdrs = ["remb_processor.h",
  ],
  copts = [
           "-I/usr/include/glib-2.0",
           "-I/usr/lib/x86_64-linux-gnu/glib-2.0/include",
         ],
  linkopts = [
           "-lgobject-2.0 -lglib-2.0 -lboost_thread -lboost_system "],
  deps = [
          "//stream_service/orbit:webrtc_includes",
          "//stream_service/orbit:common_def",
          "//third_party/glog",
  ],
)

cc_library(
  name = "rtcp_processor",
  srcs = [
          "rtcp_processor.cc",
         ],
  hdrs = ["rtcp_processor.h",
         ],
  copts = [
           "-I/usr/include/glib-2.0",
           "-I/usr/lib/x86_64-linux-gnu/glib-2.0/include",
         ],
  linkopts = [
           "-lgobject-2.0 -lglib-2.0 -lboost_thread -lboost_system "],
  deps = [
          ":rtp_headers",
          ":rtp_format_util",
          ":rtcp_sender",
          ":rtcp_receiver",
          ":nack_processor",
          ":remb_processor",
          ":janus_rtcp_processor",
          "//stream_service/orbit:media_definitions",
          "//stream_service/orbit/webrtc/modules/rtp_rtcp:rtp_rtcp",
          "//stream_service/orbit/base:timeutil",
          "//third_party/glog"
         ],
)

cc_library(
  name = "janus_rtcp_processor",
  srcs = [
          "janus_rtcp_processor.cc",
         ],
  hdrs = ["janus_rtcp_processor.h",
          "janus_rtcp_defines.h",
         ],
  copts = [
           "-I/usr/include/glib-2.0",
           "-I/usr/lib/x86_64-linux-gnu/glib-2.0/include",
         ],
  linkopts = [
           "-lgobject-2.0 -lglib-2.0 -lboost_thread -lboost_system",
         ],
  deps = [
          ":rtp_headers",
          "//stream_service/orbit:nice_lib",
          "//third_party/glog"
         ],
)

cc_library(
  name = "nack_processor",
  srcs = [
         "nack_processor.cc"
  ],
  hdrs = ["nack_processor.h",
  ],
  copts = [
         "-I/usr/include/glib-2.0",
         "-I/usr/lib/x86_64-linux-gnu/glib-2.0/include",
  ],
  linkopts = [
         "-lgobject-2.0 -lglib-2.0 -lboost_thread -lboost_system",
  ],
  deps = [
          ":rtp_headers",
          "//stream_service/orbit:media_definitions",
          "//stream_service/orbit/rtp:janus_rtcp_processor",
          "//stream_service/orbit:webrtc_includes",
          "//stream_service/orbit/base:session_info",
          "//third_party/glog",
  ],
)


cc_library(
  name = "rtp_packet_queue",
  srcs = [
          "rtp_packet_queue.cc",
         ],
  hdrs = [
          "rtp_packet_queue.h",
         ],
  linkopts = [
         "-lboost_thread -lboost_system",
  ],
  deps = [
          "//stream_service/orbit:media_definitions",
          "//third_party/glog"
         ],
)


cc_test(
 name = "rtp_packet_queue_test",
 srcs = [
  "rtp_packet_queue_test.cc",
 ],
 deps = [
   ":rtp_packet_queue",
   "//third_party/gtest:gtest_main",
 ],
)
