# The BUILD rules for building a http server and the http handler.

package(default_visibility = ["//visibility:public"])

cc_library(
  name = "html_writer",
  srcs = [
          "html_writer.cc",
         ],
  hdrs = ["html_writer.h"],
  deps = [
          "//third_party/gflags",
          "//third_party/glog",
          "//stream_service/orbit/base:base",
          "//stream_service/orbit/base:strutil"
         ],
  linkopts = [
     "-lboost_regex"
  ],
)

cc_test(
  name = "html_writer_test",
  srcs = [
          "html_writer_test.cc",
         ],
  deps = [
          ":html_writer",
          "//third_party/gtest:gtest_main",
         ],
  data = [
      "//stream_service/orbit/http_server/html:statusz_include_test.tpl",
  ],  
)

cc_library(
  name = "http_common",
  srcs = [
          "http_common.cc",
         ],
  hdrs = ["http_common.h"],
  deps = [
          "//third_party/gflags",
          "//third_party/glog",
          "//stream_service/orbit/webrtc/base:httpcommon"
         ],
)

cc_library(
  name = "http_handler",
  srcs = [
          "http_handler.cc",
         ],
  hdrs = ["http_handler.h"],
  deps = [
          ":http_common",
          "//stream_service/orbit/base:strutil",
          "//third_party/glog"
         ],
)


cc_library(
  visibility = ["//visibility:public"],
  name = 'build_info',
  hdrs = [
    'build_info.h'
  ],
  linkstamp = 'build_info.cc'
)

cc_library(
  name = "statusz_handler",
  srcs = [
          "statusz_handler.cc",
         ],
  hdrs = ["statusz_handler.h"],
  deps = [
          ":http_handler",
          ":html_writer",
          "//stream_service/orbit/http_server:build_info",
          "//stream_service/orbit/base:base",
          "//stream_service/orbit/base:sys_info",
          "//stream_service/orbit/base:session_info",
          "//stream_service/orbit/base:singleton",
          "//stream_service/orbit/base:strutil",
          "//third_party/glog"
         ],
)

cc_library(
  name = "zk_status_handler",
  srcs = [
          "zk_status_handler.cc",
         ],
  hdrs = ["zk_status_handler.h"],
  deps = [
          "//stream_service/orbit/server:orbit_zk_client",
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
  name = "exported_var",
  srcs = [
          "exported_var.cc",
         ],
  hdrs = ["exported_var.h"],
  deps = [
          "//stream_service/orbit/base:base",
          "//stream_service/orbit/base:strutil",
          "//third_party/glog"
         ],
)

cc_library(
  name = "varz_handler",
  srcs = [
          "varz_handler.cc",
         ],
  hdrs = ["varz_handler.h"],
  deps = [
          ":http_handler",
          ":html_writer",
          ":exported_var",
          "//stream_service/orbit/base:base",
          "//stream_service/orbit/base:strutil",
          "//third_party/glog",
          "//third_party/gflags"
         ],
)

cc_library(
  name = "rpc_call_stats",
  srcs = [
          "rpc_call_stats.cc",
         ],
  hdrs = ["rpc_call_stats.h"],
  deps = [
          "//stream_service/orbit/base:base",
          "//stream_service/orbit/base:singleton",
          #"//third_party/folly:folly",
          "//third_party/glog"
         ],
)

cc_test(
  name = "rpc_call_stats_test",
  srcs = [
          "rpc_call_stats_test.cc",
         ],
  deps = [
          ":rpc_call_stats",
          "//third_party/gtest:gtest_main",
         ],
)

cc_library(
  name = "rpcz_handler",
  srcs = [
          "rpcz_handler.cc",
         ],
  hdrs = ["rpcz_handler.h"],
  deps = [
          ":http_handler",
          ":html_writer",
          ":rpc_call_stats",
          "//stream_service/orbit/base:base",
         ],
)

cc_library(
  name = "pretty_signin_handler",
  srcs = [
          "pretty_signin_handler.cc",
         ],
  hdrs = ["pretty_signin_handler.h"],
  deps = [
          ":http_handler",
          ":html_writer",
          "//stream_service/orbit/base:base",
         ],
)


cc_library(
  name = "http_server",
  srcs = [
          "http_server.cc",
         ],
  hdrs = ["http_server.h"],
  deps = [
          ":http_handler",
          ":http_common",
          "//stream_service/orbit/base:strutil",
          "//stream_service/orbit/base:zlib_util",
          "//third_party/gflags",
          "//third_party/glog",
         ],
  linkopts = [
            "-lmicrohttpd"
         ],
)

cc_binary(
  name = "simple_demo_http_server",
  srcs = [
    "simple_demo_http_server.cc"
  ],
  deps = [
    ":http_server",
    ":statusz_handler",
    ":pretty_signin_handler",
    ":varz_handler",
    ":exported_var",
    "//stream_service/orbit/base:sys_info",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

cc_library(
  name = "port_checker",
  srcs = [
          "port_checker.cc",
         ],
  hdrs = ["port_checker.h"],
  deps = [
          "//third_party/gflags",
          "//third_party/glog",
         ],
)

cc_binary(
  name = "port_checker_main",
  srcs = [
          "port_checker_main.cc"
         ],
  deps = [
          ":port_checker",
         ],
)
