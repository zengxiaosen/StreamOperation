# The BUILD rules for Olive stream client.

package(default_visibility = ["//visibility:public"])

cc_library(
  name = "grpc_client",
  srcs = [
    "grpc_client.cc",
  ],
  hdrs = [
    "grpc_client.h",
  ],
  deps = [
    "//third_party/glog",
    "//stream_service/proto:stream_service_proto",
  ],
)

cc_library(
  name = "client",
  srcs = [
    "client.cc",
  ],
  hdrs = [
    "client.h",
  ],
  deps = [
    ":grpc_client",
    "//third_party/glog",
    "//stream_service/orbit:plugins",
    "//stream_service/orbit:transport_delegate",
    "//stream_service/orbit:sdp_info",
  ],
)

cc_binary(
  name = "grpc_client_test",
  srcs = [
    "grpc_client_test.cc",
  ],
  deps = [
    ":grpc_client",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

cc_binary(
  name = "grpc_stream_test",
  srcs = [
    "grpc_stream_test.cc",
  ],
  deps = [
    ":grpc_client",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

cc_binary(
  name = "client_test",
  srcs = [
    "client_test.cc",
  ],
  deps = [
    ":client",
    "//stream_service/orbit/debug_server:rtp_capture",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)
