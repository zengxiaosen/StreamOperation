# BUILD file for webrtc/audio_coding module/codecs

package(default_visibility = ["//visibility:public"])

cc_library(
  name = "codecs",
  hdrs = [
          "audio_decoder.h",
          "audio_encoder.h",
         ],
  srcs = [
          "audio_decoder.cc",
          "audio_encoder.cc",
         ],      
  deps = [
          "//stream_service/orbit:webrtc_includes",
          "//third_party/glog",
          "//stream_service/orbit/webrtc/base:rtc_base"
         ],
)


cc_library(
  name = "audio_encoder_interface",
  hdrs = [
    "audio_encoder.h"
  ],
  srcs = [
    "audio_encoder.cc"
  ],
  deps = [
    "//stream_service/orbit:webrtc_includes",
    "//stream_service/orbit/webrtc:webrtc_common",
  ],
)

cc_library(
  name = "audio_decoder_interface",
  hdrs = [
    "audio_decoder.h",
  ],
  srcs = [
    "audio_decoder.cc",
  ],
  deps = [
    "//stream_service/orbit/webrtc:webrtc_common",
    "//stream_service/orbit:webrtc_includes",
  ],
)