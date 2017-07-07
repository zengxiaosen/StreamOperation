# The BUILD rules for webrtc chrome implementation and related module.

package(default_visibility = ["//visibility:public"])
 	 	
cc_library(
  name = "media_file",
  srcs = [
           "media_file_impl.cc",
           "media_file_utility.cc",
         ],
  hdrs = [
          "media_file_utility.h",
          "media_file_defines.h",
          "media_file_impl.h",
          "media_file.h"
         ],
  deps = [
          "//third_party/glog",
          "//stream_service/orbit/webrtc/base:rtc_base",
          "//stream_service/orbit/webrtc/common_audio:common_audio",
          "//stream_service/orbit/webrtc/common_video:common_video"
          
         ],
)

cc_test(
 name = "media_file_unittest",
 srcs = [
  "media_file_unittest.cc",
 ],
 deps = [
   ":media_file",
   "//third_party/gtest:gtest_main",
   "//stream_service/orbit/webrtc/test/testsupport:fileutils",
   "//stream_service/orbit/webrtc/system_wrappers:system_wrappers"
 ],
  data = [
      "//stream_service/orbit/webrtc/modules/media_file:data/audio_tiny48.wav",
  ],
)