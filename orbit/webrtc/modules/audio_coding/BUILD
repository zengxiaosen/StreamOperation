# The BUILD rules for webrtc chrome implementation and related module.

package(default_visibility = ["//visibility:public"])

exports_files(["testdata/testfile32kHz.pcm"])
exports_files(["testdata/teststereo32kHz.pcm"])

cc_library(
  name = "rent_a_codec",
  srcs = [
            "acm2/rent_a_codec.cc",
            "acm2/acm_codec_database.cc",
         ],
  hdrs = [
            "acm2/acm_codec_database.h",
            "acm2/rent_a_codec.h",
         ],
  deps = [
           "//stream_service/orbit/webrtc:webrtc_common",
           "//stream_service/orbit/webrtc/modules/audio_coding/codecs/opus:opus",
           "//stream_service/orbit/webrtc/modules/audio_coding/codecs/cng",
           "//stream_service/orbit/webrtc/modules/audio_coding/codecs/g711",
           "//stream_service/orbit/webrtc/modules/audio_coding/codecs/pcm16b",
         ],
)


cc_library(
  name = "audio_coding",
  hdrs = [
    "acm2/acm_common_defs.h",
    "acm2/acm_receiver.h",
    "acm2/acm_resampler.h",
    "acm2/audio_coding_module_impl.h",
    "acm2/call_statistics.h",
    "acm2/codec_manager.h",
    "acm2/initial_delay_manager.h",
    "include/audio_coding_module.h",
    "include/audio_coding_module_typedefs.h",
  ],
  srcs = [
    "acm2/acm_receiver.cc",
    "acm2/acm_resampler.cc",
    "acm2/audio_coding_module.cc",
    "acm2/audio_coding_module_impl.cc",
    "acm2/call_statistics.cc",
    "acm2/codec_manager.cc",
    "acm2/initial_delay_manager.cc",
  ],
  deps = [
     "//stream_service/orbit/webrtc/modules/audio_coding:rent_a_codec",
      #  ":ilbc",
      #":red",
     #  ":cng",
    "//stream_service/orbit/webrtc:rtc_event_log",
    "//stream_service/orbit/webrtc:webrtc_common",
    "//stream_service/orbit/webrtc/common_audio:common_audio",
    "//stream_service/orbit/webrtc/system_wrappers:system_wrappers",
    "//stream_service/orbit/webrtc/modules/audio_coding/neteq",
    "//stream_service/orbit/webrtc/modules/audio_coding/codecs/opus:opus",
    "//stream_service/orbit/webrtc/modules/audio_coding/codecs/pcm16b",
    "//stream_service/orbit/webrtc/modules/audio_coding/codecs/g711",
  ],
  defines = [
      "WEBRTC_CODEC_OPUS",
      "WEBRTC_CODEC_RED",
  ],
)
