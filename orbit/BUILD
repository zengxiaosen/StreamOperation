# Olive/ORBIT module
# The BUILD rules for ORBIT module in Olive server.

package(default_visibility = ["//visibility:public"])

cc_library(
  name = "common_def",
  hdrs = ["common_def.h",
         ],
  deps = [
          "//stream_service/proto:stream_service_proto",
          "//third_party/glog"
         ],
)

cc_library(
  name = "media_definitions",
  hdrs = ["media_definitions.h",
         ],
  deps = [
          "//third_party/glog"
         ],
)

cc_library(
  name = "sdp_info",
  srcs = [
          "sdp_info.cc",
         ],
  hdrs = ["sdp_info.h",
         ],
  deps = [
          ":common_def",
          "//stream_service/orbit/base:strutil",
          "//stream_service/orbit/rtp:rtp_headers",
          "//third_party/glog"
         ],
  linkopts = [
              "-lboost_regex",
             ],
)

cc_test(
 name = "sdp_info_test",
 srcs = [
  "sdp_info_test.cc",
 ],
 deps = [
   ":sdp_info",
   "//third_party/gtest:gtest_main",
 ],
 data =  glob(["testdata/**"]),
)

cc_library(
  name = "stun_server_prober",
  srcs = [
          "stun_server_prober.cc",
         ],
  hdrs = ["stun_server_prober.h",
         ],
  copts = [
         "-I/usr/include/glib-2.0",
         "-I/usr/lib/x86_64-linux-gnu/glib-2.0/include",
         ],
  linkopts = [
	 "-lgobject-2.0 -lglib-2.0 -lnice "
         ],
  deps = [
          "//stream_service/orbit/base:singleton",
          "//third_party/glog",
         ],
)

cc_binary(
  name = "stun_server_prober_client",
  srcs = [
    "stun_server_prober_client.cc"
  ],
  deps = [
    ":stun_server_prober",
    "//stream_service/orbit/base:timeutil",
    "//third_party/glog",
    "//third_party/gflags"
  ]
)

cc_library(
  name = "nice_lib",
  visibility = ["//visibility:public"],
  srcs = [
          "nice_connection.cc",
         ],
  hdrs = ["nice_connection.h",
         ],
  copts = [
           "-I/usr/include/glib-2.0",
           "-I/usr/lib/x86_64-linux-gnu/glib-2.0/include",
         ],
  deps = [
          ":media_definitions",
          ":sdp_info",
          "//stream_service/orbit/base:singleton",
          "//stream_service/orbit/base:strutil",
          "//stream_service/orbit/base:timeutil",
          "//stream_service/orbit/base:session_info",
          "//third_party/glog",
          "//third_party/gflags"
         ],
)

cc_library(
  name = "transport",
  visibility = ["//visibility:public"],
  srcs = [
          "dtls_transport.cc",
          "srtp_channel.cc"
         ],
  hdrs = ["dtls_transport.h",
          "transport.h",
          "srtp_channel.h"
         ],
  copts = [
           "-I/usr/include/glib-2.0",
           "-I/usr/lib/x86_64-linux-gnu/glib-2.0/include",
         ],
  deps = [
          ":nice_lib",
          "//stream_service/orbit/dtls:dtls",
          "//third_party/glog"
         ]
)

cc_library(
  name = "plugins",
  srcs = [
          "transport_plugin.cc",
          "echo_plugin.cc",
          "video_bridge_plugin.cc",
          "audio_conference_plugin.cc",
          "classroom_plugin.cc",
         ],
  hdrs = [
          "transport_plugin.h",
          "echo_plugin.h",
          "video_bridge_plugin.h",
          "audio_conference_plugin.h",
          "classroom_plugin.h",
         ],
  copts = [
           "-I/usr/include/gstreamer-1.5",
           "-I/usr/lib/x86_64-linux-gnu/gstreamer-1.5/include",
           "-I/usr/include/glib-2.0",
           "-I/usr/lib/x86_64-linux-gnu/glib-2.0/include",
         ],
  linkopts = [
     "-lopus",
  ],
  deps = [
          ":transport_delegate",
          ":media_definitions",
          "//stream_service/orbit/base:strutil",
          "//stream_service/orbit/modules:audio_mixer_element",
          "//stream_service/orbit/modules:video_forward_element",
          "//stream_service/orbit/modules:stream_recorder_element",
          "//third_party/glog",
          "//stream_service/orbit/audio_processing:audio_energy",
          "//stream_service/orbit/rtp:rtp_packet_queue",
          "//stream_service/orbit/webrtc:webrtc_common",
          "//stream_service/orbit/webrtc:webrtc_format_util",
          "//stream_service/orbit/live_stream:live_stream_processor",
          "//stream_service/orbit/replay_pipeline:time_recorder",
          "//stream_service/orbit/replay_pipeline:replay_exector"
         ],
)

cc_library(
  name = "network_status_common",
  hdrs = [
          "network_status_common.h",
	 ],
)

cc_library(
  name = "network_status",
  srcs = [
          "network_status.cc",
         ],
  hdrs = [
          "network_status.h",
         ],
  deps = [
          ":network_status_common",
          ":common_def",
          "//stream_service/orbit/base:timeutil",
          "//stream_service/orbit/base:session_info",
          "//stream_service/proto:stream_service_proto",
          "//third_party/glog"
         ],
)

cc_test(
 name = "network_status_test",
 srcs = [
        "network_status_test.cc",
        ],
 deps = [
        ":network_status",
        "//stream_service/orbit/rtp:rtp_headers",
        "//third_party/gtest:gtest_main",
         ],
)

cc_library(
  name = "rtp_sender",
  srcs = ["rtp_sender.cc"],
  hdrs = ["rtp_sender.h"],
  deps = [
          ":webrtc_includes",
          ":transport",
          "//third_party/glog",
          "//stream_service/orbit/base:timeutil",
         ]
)

cc_library(
  name = "transport_delegate",
  visibility = ["//visibility:public"],
  srcs = [
          "transport_delegate.cc",
         ],
  hdrs = [
          "transport_delegate.h",
         ],
  copts = [
           "-I/usr/include/glib-2.0",
           "-I/usr/lib/x86_64-linux-gnu/glib-2.0/include",
         ],
  linkopts = [
     "-lgobject-2.0 -lglib-2.0 -lnice -lboost_thread -lboost_system -lsrtp -lcrypto -lz -ldl"
  ],
  deps = [
          ":sdp_info",
          ":transport",
          ":stun_server_prober",
          "//stream_service/orbit/base:timeutil",
          ":common_def",
          ":network_status",
          ":network_status_common",
          ":rtp_sender",
          "//stream_service/orbit/rtp:rtcp_processor",
          "//stream_service/orbit/rtp:janus_rtcp_processor",
          "//stream_service/orbit/rtp:nack_processor",
          "//stream_service/orbit/webrtc/modules/rtp_rtcp",
          "//stream_service/orbit/base:session_info",
          "//stream_service/orbit/debug_server:rtp_capture",
          "//third_party/glog"
        ],
  defines = ['NDEBUG'],
)

cc_library(
  name = "orbit_webrtc_endpoint",
  visibility = ["//visibility:public"],
  srcs = [
          "webrtc_endpoint.cc",
         ],
  hdrs = [
          "webrtc_endpoint.h",
         ],
  deps = [
          ":sdp_info",
          ":transport_delegate",
          ":network_status_common",
          "//third_party/glog"
         ],
)

cc_library(
  name = "webrtc_includes",
  hdrs = [
           "webrtc_includes.h"
         ],
  includes = ["."],
  defines = [
              "WEBRTC_POSIX",
              "WEBRTC_LINUX",
            ],
)
