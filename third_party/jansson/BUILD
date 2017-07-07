#  The BUILD rules for library Jansson
package(default_visibility = ["//visibility:public"])

cc_library(
  name = "jansson",
  hdrs = ["jansson.h",
          "jansson_config.h"],
  srcs = [
	  "dump.c",
   	"error.c",
    "hashtable.c",
    "hashtable.h",
    "hashtable_seed.c",
  	"jansson_private.h",
  	"load.c",
  	"lookup3.h",
	  "memory.c",
	  "pack_unpack.c",
  	"strbuffer.c",
  	"strbuffer.h",
	  "strconv.c",
  	"utf.c",
	  "utf.h",
    "value.c",
  ],
  deps = [
          "//third_party/glog"
         ],
  includes = [
    ".",
  ],
 defines = ["HAVE_STDINT_H", ],
)

cc_binary(
  name = "json_process",
  srcs = [
    "test/json_process.cc",
  ],
  deps = [
    ":jansson",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)
