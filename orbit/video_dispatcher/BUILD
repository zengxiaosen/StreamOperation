# Orbit Gstreamer modules
# The BUILD rules for creating Gstreamer modules

package(default_visibility = ["//visibility:public"])

cc_library(
  name = "video_dispatcher",
  srcs = [
           "video_dispatcher_plugin.cc",
           "video_dispatcher_room.cc",
         ],
  hdrs = [
           "video_dispatcher_plugin.h",
           "video_dispatcher_room.h",
         ],
  copts = [
           "-I/usr/include/gstreamer-1.5",
           "-I/usr/lib/x86_64-linux-gnu/gstreamer-1.5/include",
           "-I/usr/include/glib-2.0",
           "-I/usr/lib/x86_64-linux-gnu/glib-2.0/include",
         ],
  linkopts = [
     "-lgobject-2.0 -lglib-2.0" 
  ],
  deps = [
          "//third_party/glog",
          "//stream_service/orbit/audio_processing:audio_energy",
          "//stream_service/orbit/rtp:rtp_packet_queue",
          "//stream_service/orbit/base:timeutil",
          "//stream_service/orbit:common_def",
          "//stream_service/orbit:webrtc_includes",
          "//stream_service/orbit/webrtc:webrtc_common",
          "//stream_service/orbit/live_stream:live_stream_processor"
         ],
)
