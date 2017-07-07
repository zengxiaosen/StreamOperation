# rtp_processor BUILD file
cc_library(
  name = "rtp_processor",
  visibility = ["//visibility:public"],
  srcs = [
          "RtcpProcessor.cpp",
          "RtpPacketQueue.cpp",
          "RtpSink.cpp",
          "RtpSource.cpp",
          "RtpVP8Fragmenter.cpp",
          "RtpVP8Parser.cpp"
         ],
  hdrs = ["RtcpProcessor.h",
          "RtpHeaders.h",
          "RtpPacketQueue.h",
          "RtpSink.h",
          "RtpSource.h",
          "RtpVP8Fragmenter.h",
          "RtpVP8Parser.h"
         ],
  copts = [
           "-fpermissive",
         ],
  linkopts = [
     "-lgobject-2.0 -lglib-2.0 -lnice -lboost_thread -lboost_system"
  ],
  deps = [
          "//stream_service/erizo:logger",
          "//third_party/glog"
         ],
)

cc_test(
 name = "rtp_packet_queue_test",
 srcs = [
  "rtp_packet_queue_test.cc",
 ],
 deps = [
   ":rtp_processor",
   "//third_party/gtest:gtest_main",
 ],
)