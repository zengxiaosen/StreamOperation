# BUILD file for webrtc audio_coding module -  G711 codecs

package(default_visibility = ["//visibility:public"])

cc_library(
  name = "g711",
  hdrs = [
        'audio_decoder_pcm.h',
        'audio_encoder_pcm.h',
         ],
  srcs = [
        'audio_decoder_pcm.cc',
        'audio_encoder_pcm.cc',
        'g711_interface.c',
        'g711_interface.h',
        'g711.c',
        'g711.h',
         ],      
  deps = [
          "//third_party/glog",
          "//stream_service/orbit/webrtc/modules/audio_coding/codecs",
         ],
)

