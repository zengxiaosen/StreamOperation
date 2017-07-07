# The BUILD rules for orbit-simple SSTable
# -------------------------------------------------
# OS-SSTable is an simple implementation of SStable, a sorted
# immutable data structure usually as a index file.

load("/third_party/grpc/grpc_proto", "proto_library")

proto_library(
  name = "sstable_proto",
  src = "sstable.proto"
)

cc_library(
  name = "sstable",
  srcs = [
          "sstable_builder.cc",
         ],
  hdrs = ["sstable_builder.h"],
  deps = [
          ":sstable_proto",
          "//stream_service/orbit/base:base",
          "//stream_service/orbit/base:zlib_util",
          "//third_party/glog"
         ],
)

cc_test(
  name = "sstable_builder_test",
  srcs = [
          "sstable_builder_test.cc",
         ],
  deps = [
          ":sstable",
          "//stream_service/orbit/base:example_test_proto",
          "//stream_service/orbit/base:file",
          "//stream_service/orbit/base:timeutil",
          "//third_party/gtest:gtest_main",
         ],
)
