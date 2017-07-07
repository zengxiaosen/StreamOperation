# BUILD file for webrtc common libraries

package(default_visibility = ["//visibility:public"])

cc_library(
  name = "rbe_components",
  hdrs = [
    "aimd_rate_control.h",
    "inter_arrival.h",
    "overuse_detector.h",
    "overuse_estimator.h",
    "remote_estimator_proxy.h",
    "transport_feedback_adapter.h",
    "rate_statistics.h",
  ],
  srcs = [ 
    "aimd_rate_control.cc",
    "inter_arrival.cc",
    "overuse_detector.cc",
    "overuse_estimator.cc",
    "remote_bitrate_estimator_abs_send_time.cc",
    "remote_bitrate_estimator_single_stream.cc",
    "remote_estimator_proxy.cc",
    "send_time_history.cc",
    "transport_feedback_adapter.cc",
    "rate_statistics.cc"
],

  deps = [
    "//stream_service/orbit/webrtc:webrtc_common",
    "//stream_service/orbit:webrtc_includes",
    "//stream_service/orbit/webrtc/modules/rtp_rtcp:rtcp_packet",
    "//stream_service/orbit/webrtc/system_wrappers:system_wrappers",
  ],
)


cc_library(
  name = "remote_bitrate_estimator",
  hdrs = [
         ],
  srcs = [
         ],      
  deps = [
          ":rbe_components",
          "//stream_service/orbit/webrtc/modules/remote_bitrate_estimator/include:include",
          "//stream_service/orbit/webrtc:webrtc_common",
          "//stream_service/orbit:webrtc_includes",
         ],
)

cc_test(
 name = "rate_statistics_unittest",
 srcs = [
  "rate_statistics_unittest.cc",
 ],
 deps = [
   ":remote_bitrate_estimator",
   "//stream_service/orbit:webrtc_includes",
   "//third_party/gtest:gtest_main",
 ],
)


cc_test(
  name = "inter_arrival_unittest",
  srcs = [
           "inter_arrival_unittest.cc"
         ],
  deps = [
           ":remote_bitrate_estimator",
           "//third_party/gtest:gtest_main",
           "//third_party/glog",
         ],
)