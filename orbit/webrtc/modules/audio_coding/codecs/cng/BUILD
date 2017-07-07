# BUILD file for webrtc audio_coding module -  G711 codecs

package(default_visibility = ["//visibility:public"])

cc_library(
  name = "cng",
  hdrs = [
        'audio_encoder_cng.h',
         ],
  srcs = [
        'audio_encoder_cng.cc',
        'cng_helpfuns.c',
        'cng_helpfuns.h',
        'webrtc_cng.c',
        'webrtc_cng.h',
         ],      
  deps = [
          "//third_party/glog",
          "//stream_service/orbit/webrtc/modules/audio_coding/codecs",
          "//stream_service/orbit/webrtc/common_audio",
         ],
)

cc_test(
  name = "audio_encoder_cng_unittest",
  srcs = [
    "audio_encoder_cng_unittest.cc",
  ],
  deps = [
    ":cng",
    "//third_party/gmock",
    "//third_party/gtest:gtest_main",
  ],
  data = [
      "//stream_service/orbit/webrtc/modules/audio_coding:testdata/testfile32kHz.pcm",
      "//stream_service/orbit/webrtc/modules/audio_coding:testdata/teststereo32kHz.pcm"
  ],  
)

cc_test(
  name = "cng_unittest",
  srcs = [
    "cng_unittest.cc",
  ],
  deps = [
    ":cng",
    "//third_party/gmock",
    "//third_party/gtest:gtest_main",
  ],
  data = [
      "//stream_service/orbit/webrtc/modules/audio_coding:testdata/testfile32kHz.pcm",
      "//stream_service/orbit/webrtc/modules/audio_coding:testdata/teststereo32kHz.pcm"
  ],  
)
