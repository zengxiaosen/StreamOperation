#  The BUILD rules for Zookeeper C binding and client
#  Strip the code from Zookeeper client src/c binding.
package(default_visibility = ["//visibility:public"])

cc_library(
  name = "zookeeper_client",
  hdrs = ["zk.h",
          "include/zookeeper.h",
          "include/zookeeper_version.h",
          "include/zookeeper_log.h",
          "include/recordio.h",
          "include/proto.h"
         ],
  srcs = [
          "src/zookeeper.c",
          "src/recordio.c",
          "src/zk_adaptor.h",
          "src/mt_adaptor.c",
          "generated/zookeeper.jute.c",
          "generated/zookeeper.jute.h",
          "src/zk_log.c",
          "src/zk_hashtable.h",
          "src/zk_hashtable.c",
          "src/hashtable/hashtable.h",
          "src/hashtable/hashtable.c",
          "src/hashtable/hashtable_itr.c",
          "src/hashtable/hashtable_itr.h",
          "src/hashtable/hashtable_private.h"
         ],
  deps = [
          "//third_party/glog"
         ],
  includes = [
    ".",
    "generated/",
    "include/",
    "src/",
  ],
  linkopts = [
     "-lm -pthread"
  ],
  defines = ["THREADED"],
)

cc_binary(
  name = "zk_example",
  srcs = [
    "examples/zk_example.cc",
  ],
  deps = [
    ":zookeeper_client",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)
