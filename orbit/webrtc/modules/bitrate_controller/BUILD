# BUILD file for webrtc common libraries

package(default_visibility = ["//visibility:public"])

cc_library(
  name = "bitrate_controller",
  hdrs = [
          "include/bitrate_controller.h",
         ],
  srcs = [
          "bitrate_controller_impl.cc",
          "bitrate_controller_impl.h",
          "send_side_bandwidth_estimation.cc",
          "send_side_bandwidth_estimation.h",
         ],      
  deps = [
          "//stream_service/orbit/webrtc/system_wrappers:system_wrappers",
          "//stream_service/orbit/webrtc:webrtc_common",
          "//stream_service/orbit:webrtc_includes",
         ],
)

cc_test(
 name = "send_side_bandwidth_estimation_unittest",
 srcs = [
    "send_side_bandwidth_estimation_unittest.cc",
 ],
 deps = [
   ":bitrate_controller",
   "//stream_service/orbit:webrtc_includes",
   "//third_party/gtest:gtest_main",
 ],
)

cc_test(
 name = "bitrate_controller_unittest",
 srcs = [
    "bitrate_controller_unittest.cc",
 ],
 deps = [
   ":bitrate_controller",
   "//stream_service/orbit:webrtc_includes",
   "//third_party/gtest:gtest_main",
 ],
)