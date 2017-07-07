# Orbit Gstreamer modules
# The BUILD rules for creating Gstreamer modules

package(default_visibility = ["//visibility:public"])

cc_library(
  name = "video_mixer",
  srcs = [
          "video_mixer_position_manager.cc",
          "video_mixer_room.cc",
          "video_mixer_plugin.cc",
         ],
  hdrs = [
          "video_mixer_plugin.h",
          "video_mixer_room.h",
          "video_mixer_position.h",
          "video_mixer_position_manager.h",
          "position_update_listener.h"
         ],
  copts = [
           "-I/usr/include/gstreamer-1.5",
           "-I/usr/lib/x86_64-linux-gnu/gstreamer-1.5/include",
           "-I/usr/include/glib-2.0",
           "-I/usr/lib/x86_64-linux-gnu/glib-2.0/include",
         ],
  linkopts = [
     "-lopus -lgobject-2.0 -lglib-2.0" 
  ],
  deps = [
          "//third_party/glog",
          "//stream_service/orbit:common_def",
          "//stream_service/orbit/audio_processing:audio_energy",
          "//stream_service/orbit/modules:audio_mixer_element",
          "//stream_service/orbit/rtp:rtp_packet_queue",
          "//stream_service/orbit/webrtc:webrtc_common",
          "//stream_service/orbit/live_stream:live_stream_processor",
          "//stream_service/orbit/modules:stream_recorder_element",
         ],
)
