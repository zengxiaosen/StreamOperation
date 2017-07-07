# The BUILD rules for Olive stream server.

package(default_visibility = ["//visibility:public"])

cc_library(
  name = "erizo_media_processor",
  srcs = [
          "erizo_media_processor.cc",
         ],
  hdrs = ["erizo_media_processor.h"],
  deps = [
          "//stream_service:media_processor",
          "//stream_service/erizo/media:media_output",
          "//stream_service/erizo/media:external_input",
          "//stream_service/erizo:erizo_lib",
          "//third_party/glog"
         ],
)

cc_library(
  name = "erizo_stream_service_impl",
  srcs = [
          "erizo_stream_service_impl.cc",
         ],
  hdrs = ["erizo_stream_service_impl.h"],
  deps = [
          ":erizo_media_processor",
          "//stream_service:stream_service_proto",
          "//third_party/glog"
         ],
)

cc_binary(
  name = "erizo_stream_server",
  srcs = [
    "erizo_stream_server.cc"
  ],
  deps = [
    ":erizo_stream_service_impl",
    "//third_party/glog",
    "//third_party/gflags"
  ]
)
