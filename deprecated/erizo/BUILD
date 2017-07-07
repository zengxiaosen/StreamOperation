# MCU module
# The BUILD rules for MCU module in Olive server.

package(default_visibility = ["//visibility:public"])

cc_library(
  name = "logger",
  visibility = ["//visibility:public"],
  hdrs = ["logger.h",
         ],
  copts = [
           "-fpermissive",
           "-I/usr/include/glib-2.0",
           "-I/usr/lib/x86_64-linux-gnu/glib-2.0/include",
           "-I/usr/include/log4cxx/",
         ],
  linkopts = ["-llog4cxx -lapr-1 -laprutil-1 -lexpat"],
  deps = [
          "//third_party/glog"
         ],
)

cc_library(
  name = "nice_lib",
  visibility = ["//visibility:public"],
  srcs = [
          "NiceConnection.cpp",
         ],
  hdrs = ["logger.h",
          "NiceConnection.h",
         ],
  copts = [
           "-I/usr/include/glib-2.0",
           "-I/usr/lib/x86_64-linux-gnu/glib-2.0/include",
         ],
  deps = [
          "//third_party/glog"
         ],
)

cc_library(
  name = "erizo_lib",
  visibility = ["//visibility:public"],
  srcs = [
          "WebRtcConnection.cpp",
          "SdpInfo.cpp",
          "StringUtil.cpp",
          "DtlsTransport.cpp",
          "Stats.cpp",
          "SrtpChannel.cpp",
          "OneToManyProcessor.cpp",
         ],
  hdrs = ["WebRtcConnection.h",
          "MediaDefinitions.h",
          "DtlsTransport.h",
          "SdpInfo.h",
          "StringUtil.h",
          "Stats.h",
          "SrtpChannel.h",
          "OneToManyProcessor.h",
         ],
  copts = [
           "-I/usr/include/glib-2.0",
           "-I/usr/lib/x86_64-linux-gnu/glib-2.0/include",
         ],
  linkopts = [
     "-lgobject-2.0 -lglib-2.0 -lnice -lboost_thread -lboost_system -lsrtp -lcrypto -lz -ldl"
  ],
  deps = [
          ":logger",
          ":nice_lib",
          "//stream_service/erizo/rtp/webrtc:webrtc_impl",
          "//stream_service/erizo/rtp:rtp_processor",
          "//stream_service/erizo/dtls:dtls",
          "//third_party/glog"
         ],
)

cc_binary(
  name = "erizo_lib_test_client",
  srcs = [
    "erizo_lib_test_client.cc"
  ],
  deps = [
    ":erizo_lib",
    "//third_party/glog",
    "//third_party/gflags"
  ]
)

cc_test(
 name = "sdp_info_test",
 srcs = [
  "sdp_info_test.cc",
 ],
 deps = [
   ":erizo_lib",
   "//third_party/gtest:gtest_main",
 ],
 data =  glob(["testdata/**"]),
)
