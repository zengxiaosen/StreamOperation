# The BUILD rule for stream server.

package(default_visibility = ["//visibility:public"])

load("/third_party/grpc/grpc_proto", "proto_library")

proto_library(
  name = "stream_service_proto",
  src = "stream_service.proto",
  has_service = True,
)

proto_library(
  name = "registry_proto",
  src = "registry.proto",
  deps = ["//stream_service/orbit/base/monitor:system_info_proto"],
  has_service = True,
)

cc_library(
  name = "media_processor",
  srcs = [
          "gst_util.cc",
          "videocall_pipeline.cc",
          "videomixer_pipeline.cc",
          "videolive_pipeline.cc"
         ],
  hdrs = [ "gst_util.h",
           "videocall_pipeline.h",
           "videomixer_pipeline.h",
           "videolive_pipeline.h",
           "media_pipeline.h"
         ],
  deps = [
          ":stream_service_proto",
          "//third_party/glog"
         ],
  copts = ["-pthread",
        "-fpermissive",
        "-I/usr/include/gstreamer-1.5",
        "-I/usr/include/glib-2.0",
        "-I/usr/lib/x86_64-linux-gnu/gstreamer-1.5/include",
        "-I/usr/lib/x86_64-linux-gnu/glib-2.0/include",
        "-I/usr/include/kurento/modules/core", 
        "-I/usr/include/kurento"],
#In docker webrtcendpoint version is 6.1.2
  linkopts = ["-lgstreamer-1.5 -lgstsdp-1.5  -lgobject-2.0 -lglib-2.0 -lgstcheck-1.5 -lnice -lkmswebrtcendpointlib"],
#In docker webrtcendpoint version is 6.1.1
  #linkopts = ["-lgstreamer-1.5 -lgstsdp-1.5  -lgobject-2.0 -lglib-2.0 -lgstcheck-1.5 -lnice -lkmsicecandidate"],
)

cc_library(
  name = "stream_service_impl",
  srcs = [
          "stream_service_impl.cc",
         ],
  hdrs = ["stream_service_impl.h"],
  deps = [
          ":stream_service_proto",
          ":media_processor",
          "//third_party/glog"
         ],
)

cc_binary(
  name = "stream_server",
  srcs = [
    "stream_server.cc"
  ],
  deps = [
    ":stream_service_impl",
    "//third_party/glog",
    "//third_party/gflags"
  ]
)

cc_binary(
  name = "stream_test_client",
  srcs = [
    "stream_test_client.cc"
  ],
  deps = [
    ":stream_service_proto",
    "//third_party/glog",
    "//third_party/gflags"
  ]
)
