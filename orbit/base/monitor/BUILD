#  The BUILD rules for ORBIT monitoring and related proto.

package(default_visibility = ["//visibility:public"])

load("/third_party/grpc/grpc_proto", "proto_library")

proto_library(
  name = "network_traffic_stats_proto",
  src = "network_traffic_stats.proto"
)

proto_library(
  name = "system_info_proto",
  src = "system_info.proto",
  deps = [":network_traffic_stats_proto"]
)
