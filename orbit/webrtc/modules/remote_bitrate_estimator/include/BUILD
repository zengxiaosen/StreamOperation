# The BUILD rules for webrtc chrome implementation and related module.

package(default_visibility = ["//visibility:public"])
 	 	
cc_library(
  name = "include",
  hdrs = [
          "send_time_history.h",
          "remote_bitrate_estimator.h"
         ],
  deps = [
          "//stream_service/orbit:webrtc_includes",
          "//third_party/glog"
         ],
)