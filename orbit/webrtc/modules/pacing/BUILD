# BUILD file for webrtc/modules/rtp_rtcp

package(default_visibility = ["//visibility:public"])

cc_library(
  name = "pacing",
  hdrs = [
           "bitrate_prober.h",
           "paced_sender.h",
           "packet_router.h",
         ],
  srcs = [
           "bitrate_prober.cc",
           "paced_sender.cc",
           "packet_router.cc",
         ],
  deps = [
          "//stream_service/orbit:webrtc_includes",
          "//third_party/glog",
          "//stream_service/orbit/webrtc/base:rtc_base",
          "//stream_service/orbit/webrtc/system_wrappers",
          "//stream_service/orbit/webrtc/modules/rtp_rtcp:rtp_rtcp",
         ],
)

cc_test(
  name = "bitrate_prober_unittest",
  srcs = [
           "bitrate_prober_unittest.cc"
         ],
  deps = [
           ":pacing",
           "//third_party/gtest:gtest_main",
           "//third_party/glog",
         ],

)

cc_test(
  name = "paced_sender_unittest",
  srcs = [
           "paced_sender_unittest.cc"
         ],
  deps = [
           ":pacing",
           "//third_party/gmock",
           "//third_party/gtest:gtest_main",
           "//third_party/glog",
         ],

)

cc_test(
  name = "packet_router_unittest",
  srcs = [
           "packet_router_unittest.cc"
         ],
  deps = [
           ":pacing",
           "//third_party/gmock",
           "//third_party/gtest:gtest_main",
           "//third_party/glog",
         ],

)
