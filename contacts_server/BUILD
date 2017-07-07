# The BUILD rule for contacts server.

package(default_visibility = ["//visibility:public"])

load("/third_party/grpc/grpc_proto", "proto_library")

proto_library(
  name = "contacts_storage_proto",
  src = "contacts_storage.proto"
)

proto_library(
  name = "contacts_service_proto",
  src = "contacts_service.proto",
  has_service = True,
  deps = [
          ":contacts_storage_proto"
         ]
)

cc_library(
  name = "contacts_db",
  srcs = [
    "contacts_db.cc"
  ],
  deps = [
    ":contacts_service_proto",
    "//stream_service/third_party/leveldb",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

cc_library(
  name = "contacts_service_impl",
  srcs = [
          "contacts_service_impl.cc",
         ],
  hdrs = [
          "contacts_service_impl.h"
          ],
  deps = [
          ":contacts_service_proto",
          ":contacts_db",
          "//third_party/glog"
         ],
)

cc_binary(
  name = "contacts_server",
  srcs = [
    "contacts_server.cc"
  ],
  deps = [
    ":contacts_service_proto",
    ":contacts_service_impl",
    "//stream_service/orbit/http_server:http_handler",
    "//stream_service/orbit/http_server:http_server",
    "//stream_service/third_party/leveldb",
    "//stream_service/third_party/json2pb",
    "//third_party/glog",
    "//third_party/gflags"
  ]
)

cc_binary(
  name = "contacts_test_client",
  srcs = [
    "contacts_test_client.cc"
  ],
  deps = [
    ":contacts_service_proto",
    ":contacts_service_impl",
    "//stream_service/third_party/leveldb",
    "//stream_service/orbit/base:timeutil",
    "//third_party/glog",
    "//third_party/gflags"
  ]
)
