# BUILD file for webrtc audio_coding module -  PCM16B codecs

package(default_visibility = ["//visibility:public"])

cc_library(
  name = "pcm16b",
  hdrs = [
        'audio_decoder_pcm16b.h',
        'audio_encoder_pcm16b.h',
         ],
  srcs = [
        'audio_decoder_pcm16b.cc',
        'audio_encoder_pcm16b.cc',
        'pcm16b.c',
        'pcm16b.h',
         ],      
  deps = [
          "//third_party/glog",
          "//stream_service/orbit/webrtc/modules/audio_coding/codecs",
          "//stream_service/orbit/webrtc/modules/audio_coding/codecs/g711",
         ],
)

