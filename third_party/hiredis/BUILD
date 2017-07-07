#  BUILD rules for hiredis: Minimalistic C client for Redis >= 1.2
#  The original git repository: https://github.com/redis/hiredis

package(default_visibility = ["//visibility:public"])

cc_library(
  name = "hiredis",
  hdrs = ["hiredis.h",
         ],
  srcs = [
    "read.h",
    "read.c",
    "hiredis.c",
    "async.c",
    "async.h",
    "fmacros.h",
    "sds.h",
    "sds.c",
    "net.h",
    "net.c",
    "dict.h",
    "dict.c",
  ],
  deps = [
          "//third_party/glog"
         ],
  includes = [
    ".",
  ],
)

cc_binary(
  name = "example",
  srcs = [
    "examples/example.c",
  ],
  deps = [
    ":hiredis",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

cc_binary(
  name = "example_glib",
  srcs = [
    "examples/example-glib.c",
    "adapters/glib.h",
  ],
  deps = [
    ":hiredis",
    "//third_party/glog",
    "//third_party/gflags",
  ],
  copts = [
         "-I/usr/include/glib-2.0",
         "-I/usr/lib/x86_64-linux-gnu/glib-2.0/include",
         ],
  linkopts = [
         "-lgobject-2.0 -lglib-2.0 -lgio-2.0"
         ],
)
