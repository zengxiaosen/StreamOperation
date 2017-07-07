# Orbit Gstreamer modules
# The BUILD rules for creating Gstreamer modules

package(default_visibility = ["//visibility:public"])

cc_library(
  name = "video_map",
  hdrs = [
          "video_map.h",
         ],
  srcs = [
          "video_map.cc",
          "//third_party/glog",
         ],
)

cc_binary(
  name = "video_map_test",
  srcs = [
          "video_map_test.cc",
         ],
  deps = [
          ":video_map",
          "//third_party/glog",
          "//third_party/gflags",
         ]
)

cc_library(
  name = "video_frame_queue",
  hdrs = [
          "video_frame_queue.h",
         ],
  srcs = [
          "video_frame_queue.cc",
         ],
  deps = [
          "//stream_service/orbit/webrtc:webrtc_format_util",
          "//third_party/glog",
         ],
)

cc_library(
  name = "speaker_estimator",
  hdrs = [
          "speaker_estimator.h"
         ],
 srcs = [
          "speaker_estimator.cc",
        ],
  deps = [
           "//third_party/glog",
         ],
)

cc_test(
 name = "speaker_estimator_test",
 srcs = [
  "speaker_estimator_test.cc",
 ],
 deps = [
   ":speaker_estimator",
   "//third_party/gtest:gtest_main",
 ],
)

cc_library(
  name = "audio_mixer_element",
  hdrs = [
           "media_packet.h",
           "audio_mixer_element.h",
           "audio_buffer_manager.h"
         ],
  srcs = [
           "audio_mixer_element.cc",
           "audio_buffer_manager.cc"
         ],
  deps = [
            ":speaker_estimator",
            "//stream_service/orbit:media_definitions",
            "//stream_service/orbit:network_status",
            "//stream_service/orbit/base:thread_util",
            "//stream_service/orbit/base:timeutil",
            "//stream_service/orbit/base:strutil",
            "//stream_service/orbit/rtp:rtp_headers",
            "//stream_service/orbit/rtp:rtp_packet_queue",
            "//stream_service/orbit/webrtc/modules/audio_coding/neteq",
            "//stream_service/orbit/webrtc/modules/rtp_rtcp:rtp_rtcp",
            "//stream_service/orbit/webrtc/modules/audio_coding/codecs/opus:opus"
         ],
)



cc_binary(
  name = "audio_mixer_test_main",
  srcs = [
    "audio_mixer_test_main.cc",
  ],
  deps = [
    "//third_party/glog",
    "//third_party/gflags",
    ":audio_mixer_test",
  ],
  linkopts = [
        "-lboost_system -lboost_thread "
  ]
)

cc_library(
  name = "audio_mixer_test",
  srcs = [
    "audio_mixer_test.h",
  ],
  deps = [
    ":audio_mixer_element",
    "//third_party/gtest:gtest_main",
    "//stream_service/orbit/audio_processing:audio_energy",
    "//stream_service/orbit/webrtc/modules/audio_coding/neteq/tools:audio_loop",
    "//stream_service/orbit/webrtc/modules/audio_coding/codecs:codecs",
    "//stream_service/orbit/webrtc/modules/audio_coding/codecs/opus:opus",
    "//stream_service/orbit/base:timeutil",
  ],
  linkopts = [
        "-lboost_system"
  ]
)

cc_library(
  name = "rtp_packet_buffer",
  hdrs = [
           "rtp_packet_buffer.h"
         ],
  srcs = [
           "rtp_packet_buffer.cc",
         ],
  deps = [
           "//stream_service/orbit/base:base",
           "//stream_service/orbit:media_definitions",
           "//stream_service/orbit/webrtc:webrtc_common",
           "//stream_service/orbit/webrtc:webrtc_format_util",
           "//third_party/gtest:gtest",
         ],
  linkopts = ["-lboost_system"],
)

cc_test(
 name = "rtp_packet_buffer_test",
 srcs = [
  "rtp_packet_buffer_test.cc",
 ],
 deps = [
   ":rtp_packet_buffer",
   "//third_party/gtest:gtest_main",
 ],
)


cc_library(
  name = "stream_recorder_element",
  hdrs = [
           "stream_recorder_element.h",
         ],
  srcs = [
           "stream_recorder_element.cc",
         ],
  deps = [
          ":rtp_packet_buffer",
          "//third_party/glog",
          "//stream_service/orbit:media_definitions",
          "//stream_service/orbit/rtp:rtp_packet_queue",
          "//stream_service/orbit/webrtc:webrtc_common",
          "//stream_service/orbit/base:timeutil",
          "//stream_service/orbit/rtp:rtp_format_util"
         ],
  copts = [
            "-I/usr/include/gstreamer-1.5",
            "-I/usr/lib/x86_64-linux-gnu/gstreamer-1.5/include",
            "-I/usr/include/glib-2.0",
            "-I/usr/lib/x86_64-linux-gnu/glib-2.0/include",
          ],
)

cc_library(
  name = "video_forward_element",
  hdrs = [
           "video_forward_element.h",
         ],
  srcs = [
           "video_forward_element.cc",
           "video_forward_element_new.cc",
         ],
  linkopts = [
                "-lglib-2.0"
             ],
  copts = [
            "-I/usr/include/glib-2.0",
            "-I/usr/lib/x86_64-linux-gnu/glib-2.0/include",
          ],
  deps = [
           ":video_map",
           ":video_frame_queue",
           ":speaker_estimator",
           ":rtp_packet_buffer",
           "//stream_service/orbit:media_definitions",
           "//stream_service/orbit/rtp:janus_rtcp_processor",
           "//stream_service/orbit/rtp:rtp_packet_queue",
           "//stream_service/orbit/rtp:rtp_format_util",
           "//stream_service/orbit/webrtc:webrtc_common",
           "//third_party/glog",
           "//third_party/gflags",
         ]
)

