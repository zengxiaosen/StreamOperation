# BUILD file for webrtc audio_coding module -  OPUS codecs

package(default_visibility = ["//visibility:public"])

cc_library(
  name = "opus",
  hdrs = [
          "audio_decoder_opus.h",
          "audio_encoder_opus.h",
          "opus_inst.h",
          "opus_interface.h",
         ],
  srcs = [
          "audio_decoder_opus.cc",
          "audio_encoder_opus.cc",
          "opus_interface.c",
         ],      
  deps = [
          "//stream_service/orbit:webrtc_includes",
          "//third_party/glog",
          "//stream_service/orbit/webrtc:webrtc_common",
          "//stream_service/orbit/webrtc/modules/audio_coding/codecs",
         ],
  copts = [
           "-I/usr/include/gstreamer-1.5",
           "-I/usr/lib/x86_64-linux-gnu/gstreamer-1.5/include",
           "-I/usr/include/glib-2.0",
           "-I/usr/lib/x86_64-linux-gnu/glib-2.0/include",
         ],
  linkopts = [
     "-lopus"
  ],
)

cc_test(
  name = "audio_encoder_opus_unittest",
  srcs = [
          "audio_encoder_opus_unittest.cc",
         ],    
  deps = [
    "//stream_service/orbit/webrtc/modules/audio_coding/codecs:codecs",
    "//stream_service/orbit/webrtc/modules/audio_coding/codecs/opus:opus",
    "//third_party/gtest:gtest_main",
  ],
)

cc_test(
  name = "opus_unittest",
  srcs = [
          "opus_unittest.cc",
         ],    
  deps = [
    "//stream_service/orbit/webrtc/modules/audio_coding/codecs:codecs",
    "//stream_service/orbit/webrtc/modules/audio_coding/codecs/opus:opus",
    "//stream_service/orbit/webrtc/modules/audio_coding/neteq/tools:audio_loop",
    "//third_party/gtest:gtest_main",
  ],
  data = [
      "//stream_service/orbit/webrtc/modules/audio_coding:testdata/testfile32kHz.pcm",
      "//stream_service/orbit/webrtc/modules/audio_coding:testdata/teststereo32kHz.pcm"
  ],  
)

cc_test(
  name = "opus_fec_test",
  srcs = [
    "opus_fec_test.cc",
  ],
  deps = [
    "//stream_service/orbit/webrtc/modules/audio_coding/codecs:codecs",
    "//stream_service/orbit/webrtc/modules/audio_coding/codecs/opus:opus",
    "//third_party/gtest:gtest_main",
  ],
  data = [
      "//stream_service/orbit/webrtc/modules/audio_coding:testdata/testfile32kHz.pcm",
      "//stream_service/orbit/webrtc/modules/audio_coding:testdata/teststereo32kHz.pcm"
  ],  
)


