package(default_visibility = ["//visibility:public"])

cc_binary(
  name = "test_priority_queue",
  srcs = [
    "test_priority_queue.cc"
  ],
  deps = [
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

cc_binary(
  name = "libnice_example",
  srcs = [
    "libnice_example.cc"
  ],
  deps = [
    "//stream_service/third_party/libnice",
    "//third_party/glog",
    "//third_party/gflags",
  ],
 copts = ["-fpermissive",
         "-I/usr/include/glib-2.0",
         "-I/usr/lib/x86_64-linux-gnu/glib-2.0/include",
  ],
 linkopts = [
     "-lgobject-2.0 -lglib-2.0 -lnice"
  ],
)

cc_test(
 name = "janus_rtcp_processor_test",
 srcs = [
     "janus_rtcp_processor_test.cc",
 ],
 copts = [
         "-I/usr/include/glib-2.0",
         "-I/usr/lib/x86_64-linux-gnu/glib-2.0/include",
  ],
 linkopts = [
     "-lgobject-2.0 -lglib-2.0 "
  ],
 deps = [
   "//stream_service/orbit/rtp:janus_rtcp_processor",
   "//third_party/gtest:gtest_main",
   "//third_party/gperftools:tcmalloc_heapcheck",
 ],
)

cc_binary(
  name = "leveldb_example",
  srcs = [
    "leveldb_example.cc"
  ],
  deps = [
    "//stream_service/third_party/leveldb",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

cc_binary(
  name = "async_test",
  srcs = [
    "async_test.cc"
  ],
  deps = [
    "//stream_service/orbit/base:timeutil",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

cc_binary(
  name = "opus_performance_test",
  srcs = [
    "opus_performance_test.cc"
  ],
  deps = [
    "//stream_service/orbit/base",
    "//stream_service/orbit/base:timeutil",
    "//stream_service/orbit/debug_server:rtp_capture",
    "//stream_service/orbit/modules:audio_mixer_element",
  ],
)

cc_binary(
  name = "udp_example_server",
  srcs = [
    "udp_example_server.cc"
  ],
  deps = [
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

cc_binary(
  name = "udp_example_client",
  srcs = [
    "udp_example_client.cc"
  ],
  deps = [
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

cc_binary(
  name = "zk_leader_election",
  srcs = [
    "zk_leader_election.cc"
  ],
  deps = [
    "//stream_service/third_party/zookeeper_client:zookeeper_client",
    "//stream_service/orbit/base:strutil",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)
