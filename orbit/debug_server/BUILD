# The BUILD rules for Orbit debug server
# -------------------------------------------------
# Orbit debug server - a server is dedicated to run adebug server for capturing
# traffic and replaying them to simulate the real network environment.

package(default_visibility = ["//visibility:public"])

load("/third_party/grpc/grpc_proto", "proto_library")

proto_library(
  name = "stored_rtp_proto",
  src = "stored_rtp.proto"
)

cc_library(
  name = "replay_transport_delegate",
  srcs = [
          "replay_transport_delegate.cc",
         ],
  hdrs = ["replay_transport_delegate.h"],
  deps = [
          "//stream_service/orbit:orbit_webrtc_endpoint",
          "//third_party/glog"
         ],
)

cc_library(
  name = "rtp_capture",
  srcs = [
          "rtp_capture.cc",
         ],
  hdrs = ["rtp_capture.h"],
  deps = [
          ":stored_rtp_proto",
          "//stream_service/orbit:media_definitions",
          "//stream_service/orbit/base:singleton",
          "//stream_service/orbit/base:recordio",
          "//stream_service/orbit/base:file",
          "//stream_service/orbit/base:timeutil",
          "//third_party/glog"
         ],
)

cc_test(
  name = "rtp_capture_test",
  srcs = [
          "rtp_capture_test.cc",
         ],
  deps = [
          ":rtp_capture",
          "//third_party/gtest:gtest_main",
         ],
)

cc_library(
  name = "packet_replay_driver",
  hdrs = ["packet_replay_driver.h"],
  srcs = [
    "packet_replay_driver.cc",
  ],
  deps = [
    ":rtp_capture",
    ":replay_transport_delegate",
    "//stream_service/orbit/base:session_info",
    "//stream_service/orbit/video_mixer:video_mixer",
    "//stream_service/orbit/video_dispatcher:video_dispatcher",
    "//stream_service/orbit/server:orbit_media_pipelines",
    "//stream_service/orbit:transport_delegate",
    "//stream_service/orbit:plugins",
    "//stream_service/orbit/base:timeutil",
    "//stream_service/orbit/base:strutil",
    "//third_party/glog",
    "//third_party/gflags",
  ],
  copts = [
    "-I/usr/include/gstreamer-1.5",
    "-I/usr/lib/x86_64-linux-gnu/gstreamer-1.5/include",
    "-I/usr/include/glib-2.0",
    "-I/usr/lib/x86_64-linux-gnu/glib-2.0/include",
  ],
)


cc_binary(
  name = "rtp_replay_main",
  srcs = [
    "rtp_replay_main.cc",
  ],
  deps = [
    ":packet_replay_driver",
    "//third_party/glog",
    "//third_party/gflags",
    #"//third_party/gperftools:tcmalloc_heapcheck",
    #"//third_party/gperftools:tcmalloc",
  ],
)


