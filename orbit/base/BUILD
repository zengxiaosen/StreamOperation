#  The BUILD rules for ORBIT base classes.

package(default_visibility = ["//visibility:public"])

load("/third_party/grpc/grpc_proto", "proto_library")

cc_library(
  name = "base",
  hdrs = ["base.h",
         ],
  deps = [
          "//third_party/glog"
         ],
)

cc_library(
  name = "singleton",
  hdrs = ["singleton.h",
         ],
  deps = [
          "//third_party/glog"
         ],
  copts = [
           "-I/usr/include/glib-2.0",
           "-I/usr/lib/x86_64-linux-gnu/glib-2.0/include",
         ],
  linkopts = [
     "-lboost_thread -lboost_system"
  ],
)

cc_library(
  name = "sys_info",
  hdrs = [
          "sys_info.h",
         ],
  srcs = [
          "sys_info.cc"
         ],
  deps = [
          ":singleton",
          ":strutil",
          ":timeutil",
          "//stream_service/orbit/base/monitor:network_traffic_stats_proto",
          "//stream_service/orbit/base/monitor:system_info_proto",
          "//stream_service/third_party/tsar:tsar",
          "//third_party/glog"
         ],
)

cc_library(
  name = "session_info",
  hdrs = [
          "session_info.h",
         ],
  srcs = [
          "session_info.cc"
         ],
  deps = [
          ":singleton",
          "//third_party/glog",
          "//stream_service/orbit:common_def",
          "//stream_service/orbit/base:strutil",
          "//third_party/folly:folly"
         ],
)

cc_binary(
  name = "sys_info_main",
  srcs = [
    "sys_info_main.cc"
  ],
  deps = [
    ":sys_info",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

cc_binary(
  name = "session_info_main",
  srcs = [
    "session_info_main.cc"
  ],
  deps = [
    ":session_info",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

cc_library(
  name = "strutil",
  hdrs = ["strutil.h",
         ],
  srcs = [
          "strutil.cc"
         ],
  deps = [
          "//third_party/glog"
         ],
)

cc_test(
 name = "strutil_unittest",
 srcs = [
  "strutil_unittest.cc",
 ],
 deps = [
   ":strutil",
   "//third_party/gtest:gtest_main",
 ],
)

cc_library(
  name = "file",
  hdrs = ["file.h",
          "status.h",
         ],
  srcs = [
          "file.cc"
         ],
  deps = [
          ":base",
          ":strutil",
          "//third_party/protobuf/upstream:protobuf",
          "//third_party/glog"
         ],
)

cc_library(
  name = "thread_util",
  hdrs = ["thread_util.h"],
  deps = [":timeutil"],
)

cc_library(
  name = "timeutil",
  hdrs = ["timeutil.h",
         ],
  srcs = [
          "timeutil.cc"
         ],
  deps = [
          ":base",
          "//third_party/glog"
         ],
)

cc_test(
 name = "timeutil_test",
 srcs = [
  "timeutil_unittest.cc",
 ],
 deps = [
   ":timeutil",
   "//third_party/gtest:gtest_main",
 ],
)

cc_library(
  name = "zlib_util",
  hdrs = ["zlib_util.h",
         ],
  srcs = [
          "zlib_util.cc"
         ],
  deps = [
          ":file",
          "//third_party/zlib",
          "//third_party/glog"
         ],
)


cc_library(
  name = "recordio",
  hdrs = ["recordio.h",
         ],
  srcs = [
          "recordio.cc"
         ],
  deps = [
          ":file",
          ":zlib_util",
          "//third_party/glog"
         ],
)

proto_library(
  name = "example_test_proto",
  src = "example_test.proto"
)

cc_test(
 name = "file_test",
 srcs = [
  "file_test.cc",
 ],
 deps = [
   ":file",
   ":example_test_proto",
   "//third_party/gtest:gtest_main",
 ],
 data = [
      "//stream_service/orbit/base:testdata/test_example.pb.txt"
  ],  
)

cc_test(
 name = "recordio_test",
 srcs = [
  "recordio_test.cc",
 ],
 deps = [
   ":recordio",
   ":example_test_proto",
   "//third_party/gtest:gtest_main",
 ],
)

