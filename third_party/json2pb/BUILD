# The BUILD rule for json2pb - A Json to Protocol Buffer and vice verse conversion library.

package(default_visibility = ["//visibility:public"])

cc_library(
  name = "json2pb",
  hdrs = [
    "json2pb.h"
  ],
  srcs = [
    "json2pb.cc",
    "bin2ascii.h"
  ],
  deps = [
    "//stream_service/third_party/jansson",
    "//third_party/protobuf/upstream:protobuf",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

