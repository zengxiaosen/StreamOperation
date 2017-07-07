# WebRTC implementation

package(default_visibility = ["//visibility:public"])

cc_library(
  name = "webrtc_impl",
  visibility = ["//visibility:public"],
  srcs = [
          "fec_receiver_impl.cc",
          "forward_error_correction.cc",
          "forward_error_correction_internal.cc",
          "rtp_utility.cc"
         ],
  hdrs = [
    "fec_private_tables_bursty.h",
    "fec_private_tables_random.h",
    "fec_receiver_impl.h",      
    "forward_error_correction.h",
    "forward_error_correction_internal.h",
    "rtp_utility.h"],
  deps = [
          "//third_party/glog"
         ],
)



