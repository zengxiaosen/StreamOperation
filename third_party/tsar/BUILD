# TSAR system activity reporter
# The BUILD rules for integrating Tsar system.

package(default_visibility = ["//visibility:public"])

cc_library(
  name = "tsar",
  hdrs = [
           "tsar_wrapper.h"
         ],
  srcs = [
           "tsar_wrapper.cc",
         ],
  deps = [
          "//third_party/glog"
         ],
)

cc_binary(
  name = "tsar_main",
  srcs = [
    "tsar_main.cc"
  ],
  deps = [
    ":tsar",
    "//third_party/glog",
    "//third_party/gflags"
  ]
)

cc_binary(
  name = "onload",
  srcs = [
    "onload.cc"
  ],
  deps = [
    ":tsar",
    "//stream_service/orbit/base:timeutil",
    "//stream_service/orbit/base:strutil",
    "//third_party/glog",
    "//third_party/gflags"
  ]
)

