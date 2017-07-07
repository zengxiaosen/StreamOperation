# BUILD file for webrtc audio_coding module -  OPUS codecs

package(default_visibility = ["//visibility:public"])

cc_library(
  name = "audio_loop",
  hdrs = [
          "audio_loop.h",
         ],
  srcs = [
          "audio_loop.cc",
         ],      
  deps = [
          "//third_party/glog",
          "//stream_service/orbit:webrtc_includes",
          "//stream_service/orbit/webrtc:webrtc_common",
         ],
)

cc_library(
  name = "neteq_unittest_tools",
  hdrs = [
            'audio_loop.h',
            'audio_sink.h',
            'constant_pcm_packet_source.h',
            'input_audio_file.h',
            'output_audio_file.h',
            'packet.h',
            'packet_source.h',
            'resample_input_audio_file.h',
            'rtp_file_source.h',
         ],
  srcs = [
            'audio_loop.cc',
            'constant_pcm_packet_source.cc',
            'output_wav_file.h',
            'packet.cc',
            'input_audio_file.cc',
            'resample_input_audio_file.cc',
            'rtp_file_source.cc',
            'rtp_generator.cc',         ],      
  deps = [
          "//third_party/glog",
          "//stream_service/orbit/webrtc/test:rtp_file_reader",
          "//stream_service/orbit:webrtc_includes",
          "//stream_service/orbit/webrtc:webrtc_common",
         ],
)




