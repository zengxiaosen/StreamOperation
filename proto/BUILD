# The BUILD rule for stream server related proto.
package(default_visibility = ["//visibility:public"])

load("/third_party/grpc/grpc_proto", "proto_library")

proto_library(
  name = "stream_service_proto",
  src = "stream_service.proto",
  has_service = True,
)

proto_library(
  name = "registry_proto",
  src = "registry.proto",
  deps = ["//stream_service/orbit/base/monitor:system_info_proto"],
  has_service = True,
)
