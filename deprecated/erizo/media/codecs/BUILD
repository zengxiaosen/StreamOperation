# The BUILD rules for codecs

package(default_visibility = ["//visibility:public"])

cc_library(
  name = "codecs",
  srcs = [
          "VideoCodec.cpp",
          "AudioCodec.cpp"
         ],
  hdrs = ["VideoCodec.h",
          "AudioCodec.h"],
  deps = [
          "//stream_service:media_processor",
          "//third_party/glog"
         ],
  linkopts = [
     "-lavformat-ffmpeg -lavcodec-ffmpeg -lavutil-ffmpeg"
  ],
)
