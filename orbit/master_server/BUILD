# The BUILD rules for master/slave server.

package(default_visibility = ["//visibility:public"])

cc_library(
  name = "registry_service_impl",
  hdrs = [
          "registry_service_impl.h",
         ],
  srcs = [
          "registry_service_impl.cc"
         ],
  deps = [
    ":slavedata_collector",
    "//stream_service/proto:registry_proto",
    "//third_party/glog",
   ],
)

cc_library(
  name = "orbit_master_server_manager",
  hdrs = [
          "orbit_master_server_manager.h",
         ],
  srcs = [
          "orbit_master_server_manager.cc"
         ],
  deps = [
    ":registry_service_impl",
    "//stream_service/orbit/base:singleton",
    "//third_party/glog",
    "//third_party/gflags",
   ],
)

cc_library(
  name = "forward_stream_service_impl",
  hdrs = [
          "forward_stream_service_impl.h",
         ],
  srcs = [
          "forward_stream_service_impl.cc"
         ],
  deps = [
    ":orbit_master_server_manager",
    "//stream_service/proto:stream_service_proto",
    "//stream_service/orbit/base:base",
    "//stream_service/orbit/http_server:rpc_call_stats",
    "//third_party/glog",
   ],
)

cc_library(
  name = "master_status_handler",
  srcs = [
          "master_status_handler.cc",
         ],
  hdrs = ["master_status_handler.h"],
  deps = [
          ":slavedata_collector",
          ":orbit_master_server_manager",
          "//stream_service/proto:registry_proto",
          "//stream_service/orbit/base/monitor:system_info_proto",
          "//stream_service/orbit/base:base",
          "//stream_service/orbit/base:strutil",
          "//stream_service/orbit/http_server:http_handler",
          "//stream_service/orbit/http_server:html_writer",
          "//stream_service/orbit/http_server:build_info",
          "//third_party/glog"
         ],
)

cc_library(
  name = "slavedata_collector",
  hdrs = [
          "slavedata_collector.h",
          "server_stat.h",
         ],
  srcs = [
          "slavedata_collector.cc"
         ],
  deps = [
    "//stream_service/orbit/base:strutil",
    "//stream_service/orbit/base:singleton",
    "//stream_service/orbit/base:timeutil",
    "//third_party/glog",
    "//third_party/gflags",
    "//stream_service/third_party/leveldb",
    "//stream_service/proto:registry_proto"
   ],
)

cc_binary(
  name = "orbit_master_server",
  srcs = [
           "orbit_master_server.cc"
         ],
  deps = [
    ":master_status_handler",
    ":slavedata_collector",
    ":forward_stream_service_impl",
    "//stream_service/orbit/server:orbit_stream_service_impl",
    "//stream_service/proto:registry_proto",
    "//stream_service/orbit/base:sys_info",
    "//stream_service/orbit/base:timeutil",
    "//stream_service/orbit/base:strutil",
    "//stream_service/orbit/http_server:statusz_handler",
    "//stream_service/orbit/http_server:varz_handler",
    "//stream_service/orbit/http_server:rpcz_handler",
    "//stream_service/orbit/http_server:pretty_signin_handler",
    "//stream_service/orbit/http_server:http_server",
    "//stream_service/orbit/http_server:port_checker",
    "//third_party/glog",
    "//third_party/gflags"
  ],
  stamp = 1,
)

cc_binary(
  name = "registry_test_client",
  srcs = [
    "registry_test_client.cc"
  ],
  deps = [
    "//stream_service/proto:registry_proto",
    "//stream_service/orbit/base:timeutil",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)
