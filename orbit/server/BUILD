# The BUILD rules for Olive stream server.

package(default_visibility = ["//visibility:public"])

cc_library(
  name = "orbit_server_util",
  hdrs = ["orbit_server_util.h"],
  srcs = ["orbit_server_util.cc"],
  deps = [
  	"//stream_service/proto:stream_service_proto",
  	"//third_party/glog"
  ],
)

cc_library(
  name = "gst_util",
  srcs = [
    "gst_util.cc",
  ],
  hdrs = [
    "gst_util.h",
  ],
   copts = [
          "-I/usr/include/glib-2.0",
          "-I/usr/lib/x86_64-linux-gnu/glib-2.0/include",
          "-I/usr/include/gstreamer-1.5",
          "-I/usr/lib/x86_64-linux-gnu/gstreamer-1.5/include",
         ],
  linkopts = [
        "-lgstreamer-1.5 -lgstsdp-1.5 -lgstcheck-1.5 -lgobject-2.0 -lglib-2.0 -lgstapp-1.5 -lgstbase-1.5"
    ],
)
cc_library(
  name = "orbit_media_pipelines",
  srcs = [
          "orbit_media_pipelines.cc",
         ],
  hdrs = [
          "orbit_media_pipelines.h",
         ],
  deps = [
          ":gst_util",
          ":orbit_server_util",
          "//stream_service/orbit/video_mixer:video_mixer",
          "//stream_service/orbit/video_dispatcher:video_dispatcher",
          "//stream_service/orbit:plugins",
          "//stream_service/orbit:network_status_common",
          "//stream_service/orbit:orbit_webrtc_endpoint",
          "//stream_service/orbit/http_server:rpc_call_stats",
          "//stream_service/proto:stream_service_proto",
          "//third_party/glog"
         ],
 copts = [
          "-I/usr/include/glib-2.0",
          "-I/usr/lib/x86_64-linux-gnu/glib-2.0/include",
          "-I/usr/include/gstreamer-1.5",
          "-I/usr/lib/x86_64-linux-gnu/gstreamer-1.5/include",
         ],
  linkopts = [
                "-lgstreamer-1.5 -lgstsdp-1.5 -lgstcheck-1.5 -lgobject-2.0 -lglib-2.0 -lgstapp-1.5 -lgstbase-1.5"
             ],

)

cc_library(
  name = "orbit_stream_service_impl",
  srcs = [
          "orbit_stream_service_impl.cc",
         ],
  hdrs = ["orbit_stream_service_impl.h"],
  deps = [
          ":orbit_media_pipelines",
          "//stream_service/orbit:network_status_common",
          "//stream_service/orbit/base:session_info",
          "//stream_service/orbit/http_server:rpc_call_stats",
          "//stream_service/orbit/rtp:rtp_headers",
          "//stream_service/proto:stream_service_proto",
          "//third_party/glog",
          "//third_party/gflags",
         ],
)

cc_library(
  name = "orbit_zk_client",
  srcs = [
          "orbit_zk_client.cc",
         ],
  hdrs = ["orbit_zk_client.h"],
  deps = [
          "//stream_service/third_party/zookeeper_client:zookeeper_client",
          "//stream_service/orbit/base:strutil",
          "//stream_service/proto:registry_proto",
          "//stream_service/orbit/base:sys_info",
          "//stream_service/orbit/production:machine_db",
          "//third_party/glog",
          "//third_party/gflags",
          "//stream_service/orbit/base:singleton",
         ],
)

cc_binary(
  name = "orbit_stream_server",
  srcs = [
    "orbit_stream_server.cc"
  ],
  deps = [
    ":orbit_zk_client",
    ":orbit_stream_service_impl",
    "//stream_service/proto:registry_proto",
    "//stream_service/orbit/base:sys_info",
    "//stream_service/orbit/production:machine_db",
    "//stream_service/orbit/http_server:zk_status_handler",
    "//stream_service/orbit/http_server:statusz_handler",
    "//stream_service/orbit/http_server:varz_handler",
    "//stream_service/orbit/http_server:pretty_signin_handler",
    "//stream_service/orbit/http_server:rpcz_handler",
    "//stream_service/orbit/http_server:http_server",
    "//stream_service/orbit/http_server:port_checker",
    "//third_party/glog",
    "//third_party/gflags",
    # "//third_party/gperftools:tcmalloc",
  ],
  stamp = 1,
)

cc_binary(
  name = "orbit_zk_client_main",
  srcs = [
          "orbit_zk_client_main.cc"
         ],
  deps = [
          ":orbit_zk_client",
          "//stream_service/third_party/zookeeper_client:zookeeper_client",
          "//stream_service/orbit/base:strutil",
          "//third_party/glog",
          "//third_party/gflags",
         ],
)

