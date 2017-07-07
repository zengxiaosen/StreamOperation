# The BUILD rules for webrtc chrome implementation and related module.

package(default_visibility = ["//visibility:public"])

cc_library(
  name = "fileutils",
  srcs = [
           "fileutils.cc",
         ],
  hdrs = [
           "fileutils.h"
         ],
  deps = [
          "//stream_service/orbit:webrtc_includes",
          "//stream_service/orbit/webrtc:webrtc_common"
         ],
)

cc_test(
  name = "fileutils_unittest",
  srcs = [
           "fileutils_unittest.cc",
         ],
  deps = [
          ":fileutils",
          "//stream_service/orbit:webrtc_includes",
          "//third_party/gtest:gtest_main",
          "//stream_service/orbit/webrtc:webrtc_common"
         ],
)
