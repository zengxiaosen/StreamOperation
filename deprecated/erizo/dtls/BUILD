# DTLS implementation

package(default_visibility = ["//visibility:public"])

cc_library(
  name = "dtls",
  srcs = [
          "DtlsClient.cpp",
          "DtlsSocket.cpp",
          "OpenSSLInit.cpp",
          "DtlsTimer.cpp",
          "DtlsFactory.cpp",
          "bf_dwrap.c"
         ],
  hdrs = [ "DtlsFactory.h",
           "DtlsTimer.h",
           "OpenSSLInit.h",
           "DtlsSocket.h" ],
  copts = [
           "-I/usr/include/glib-2.0",
           "-I/usr/lib/x86_64-linux-gnu/glib-2.0/include",
         ],
  linkopts = [
     "-lgobject-2.0 -lglib-2.0 -lnice -lssl -lcrypto"
  ],
  deps = [
          "//third_party/glog"
         ],
)
