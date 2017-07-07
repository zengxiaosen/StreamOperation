# Orbit Gstreamer modules
# The BUILD rules for creating Gstreamer modules

package(default_visibility = ["//visibility:public"])

cc_library(
  name = "live_stream_processor",
  srcs = [
          "live_stream_processor.cc",
          "common_defines.cc",
         ],
  hdrs = [
          "live_stream_processor.h",
          "live_stream_processor_impl.h",
          "common_defines.h",
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
  deps = [
          "//third_party/glog",
          "//stream_service/orbit/modules:rtp_packet_buffer",
          "//stream_service/orbit:media_definitions",
          "//stream_service/orbit/rtp:rtp_packet_queue",
          "//stream_service/orbit/webrtc:webrtc_common",
         ],
)
