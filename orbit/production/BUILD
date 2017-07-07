#  The BUILD rules for ORBIT machine db.

package(default_visibility = ["//visibility:public"])

load("/third_party/grpc/grpc_proto", "proto_library")

proto_library(
  name = "machine_db_proto",
  src = "machine_db.proto"
)

cc_library(
  name = "machine_db",
  srcs = [
          "machine_db.cc",
         ],
  hdrs = ["machine_db.h",
         ],
  deps = [
          ":machine_db_proto",
          "//stream_service/orbit/base:file",
          "//stream_service/orbit/base:singleton",
          "//third_party/glog",
          "//third_party/gflags",
         ],
)

cc_test(
 name = "machine_db_test",
 srcs = [
  "machine_db_test.cc",
 ],
 deps = [
   ":machine_db",
   "//third_party/gtest:gtest_main",
 ],
  data = [
      "//stream_service/orbit/olivedata/production:machine_db.pb.txt",
  ],  
)

cc_binary(
  name = "machine_db_updater",
  srcs = [
    "machine_db_updater_main.cc"
  ],
  deps = [
    ":machine_db",
    "//stream_service/orbit/base:file",
    "//stream_service/orbit:stun_server_prober",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)
