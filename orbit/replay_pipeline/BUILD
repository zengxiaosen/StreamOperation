# The BUILD rules for Orbit replay pipeline
# -------------------------------------------------
# Orbit replay pipeline

package(default_visibility = ["//visibility:public"])

load("/third_party/grpc/grpc_proto", "proto_library")

proto_library(
  name = "session_timer_proto",
  src = "session_timer.proto"
)

cc_library(
  name = "replay_pipeline",
  hdrs = ["replay_pipeline.h",
         ],
  srcs = [
          "replay_pipeline.cc",
         ],
  deps = [
    "//stream_service/proto:stream_service_proto",
    "//stream_service/orbit/debug_server:rtp_capture",
    "//stream_service/orbit/base:timeutil",
    "//stream_service/orbit/base:strutil",
    "//stream_service/orbit/base:file",
    "//stream_service/orbit/webrtc/base:rtc_base",
    "//stream_service/orbit/modules:stream_recorder_element",
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

cc_library(
  name = "time_recorder",
  srcs = [
          "time_recorder.cc",
         ],
  hdrs = ["time_recorder.h"],
  deps = [
          ":session_timer_proto",
          "//stream_service/orbit:media_definitions",
          "//stream_service/orbit/base:recordio",
          "//stream_service/orbit/base:file",
          "//stream_service/orbit/base:timeutil",
          "//stream_service/third_party/json2pb:json2pb",
          "//stream_service/orbit/base:session_info",
          "//third_party/glog"
         ],
)

cc_library(
  name = "replay_exector",
  srcs = [
          "replay_exector.cc",
         ],
  hdrs = ["replay_exector.h"],
  deps = [
            ":replay_pipeline",
            "//third_party/glog",
            "//third_party/gflags",
            "//stream_service/orbit/base:singleton",
            "//stream_service/orbit/base:thread_util"
         ],
)

cc_binary(
  name = "replay_pipeline_main",
  srcs = [
    "replay_pipeline_main.cc",
  ],
  deps = [
    ":replay_pipeline",
    ":time_recorder",
    "//stream_service/orbit/server:gst_util",
    "//third_party/glog",
    "//third_party/gflags"
  ],
  copts = [
          "-I/usr/include/glib-2.0",
          "-I/usr/lib/x86_64-linux-gnu/glib-2.0/include",
          "-I/usr/include/gstreamer-1.5",
          "-I/usr/lib/x86_64-linux-gnu/gstreamer-1.5/include",
         ],
  linkopts = [
                "-lgstreamer-1.5 -lgstsdp-1.5 -lgstcheck-1.5 -lgobject-2.0 -lglib-2.0 -lgstapp-1.5 -lgstbase-1.5"
             ],
)

