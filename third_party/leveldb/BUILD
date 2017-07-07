#  The BUILD rules for level_db.

package(default_visibility = ["//visibility:public"])

# load("/third_party/grpc/grpc_proto", "proto_library")

cc_library(
  name = "leveldb",
  hdrs = glob(["include/leveldb/*.h"]),
  srcs = [
  "table/block.cc",
  "table/block_builder.h",
  "table/iterator.cc",
  "table/merger.h",
  "table/block.h",
  "table/filter_block.cc",
  "table/format.cc",
  "table/iterator_wrapper.h",
  "table/table.cc",
  "table/two_level_iterator.cc",
  "table/block_builder.cc",
  "table/filter_block.h",
  "table/format.h",
  "table/merger.cc",
  "table/table_builder.cc",
  "table/two_level_iterator.h",
  "db/db_impl.cc",
  "db/log_format.h",
  "db/memtable.h",
  "db/table_cache.h",
  "db/write_batch.cc",
  "db/builder.cc",
  "db/db_impl.h",
  "db/dumpfile.cc",
  "db/log_reader.cc",
  "db/version_edit.cc",
  "db/write_batch_internal.h",
  "db/builder.h",
  "db/db_iter.cc",
  "db/log_reader.h",
  "db/repair.cc",
  "db/version_edit.h",
  "db/c.cc",
  "db/db_iter.h",
  "db/filename.cc",
  "db/skiplist.h",
  "db/filename.h",
  "db/log_writer.cc",
  "db/version_set.cc",
  "db/dbformat.cc",
  "db/log_writer.h",
  "db/snapshot.h",
  "db/version_set.h",
  "db/db_bench.cc",
  "db/dbformat.h",
  "db/memtable.cc",
  "db/table_cache.cc",
  "port/port.h",
	"port/port_posix.cc",
	"port/thread_annotations.h",
  "port/atomic_pointer.h",
	"port/port_example.h",
	"port/port_posix.h",
  "util/arena.cc",
  "util/cache.cc",
  "util/comparator.cc",
  "util/env_posix.cc",
  "util/mutexlock.h",
  "util/arena.h",
  "util/crc32c.cc",
  "util/options.cc",
  "util/coding.cc",
  "util/crc32c.h",
  "util/filter_policy.cc",
  "util/posix_logger.h",
  "util/bloom.cc",
  "util/coding.h",
  "util/hash.cc",
  "util/logging.cc",
  "util/random.h",
  "util/env.cc",
  "util/hash.h",
  "util/logging.h",
  "util/status.cc",
  "helpers/memenv/memenv.h",
  "helpers/memenv/memenv.cc"
  ],
  deps = [
          "//third_party/glog"
         ],
  includes = [
    "include",
    ".",
  ],
  defines = ["LEVELDB_PLATFORM_POSIX"],
)

cc_library(
  name = "leveldb_testbase",
  srcs = [
          "util/testutil.cc",
          "util/testharness.cc",
         ],
  hdrs = [
          "util/testharness.h",
          "util/testutil.h"
         ],
  deps = [
          ":leveldb",
          "//third_party/glog"
         ],
)


cc_binary(
  name = "leveldbutil",
  srcs = [
    "db/leveldbutil.cc",
  ],
  deps = [
    ":leveldb",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

cc_binary(
  name = "db_bench",
  srcs = [
    "db/db_bench.cc",
    "util/histogram.h",
    "util/histogram.cc",
  ],
  deps = [
    ":leveldb_testbase",
    "//third_party/glog",
    "//third_party/gflags",
  ],
  linkopts = [
    "-lm"
  ],

)

# Notice: since the leveldb test are not written in gTest framework.
# (they are using their own testing framework), we will have to make them
# as the cc_binary as a single test code.
cc_binary(
  name = "autocompact_test",
  srcs = [
    "db/autocompact_test.cc"
  ],
  deps = [
    ":leveldb_testbase",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

cc_binary(
  name = "c_test",
  srcs = [
    "db/c_test.c"
  ],
  deps = [
    ":leveldb_testbase",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

cc_binary(
  name = "corruption_test",
  srcs = [
    "db/corruption_test.cc"
  ],
  deps = [
    ":leveldb_testbase",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

cc_binary(
  name = "db_test",
  srcs = [
    "db/db_test.cc"
  ],
  deps = [
    ":leveldb_testbase",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

cc_binary(
  name = "dbformat_test",
  srcs = [
    "db/dbformat_test.cc"
  ],
  deps = [
    ":leveldb_testbase",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

cc_binary(
  name = "fault_injection_test",
  srcs = [
    "db/fault_injection_test.cc"
  ],
  deps = [
    ":leveldb_testbase",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

cc_binary(
  name = "filename_test",
  srcs = [
    "db/filename_test.cc"
  ],
  deps = [
    ":leveldb_testbase",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

cc_binary(
  name = "log_test",
  srcs = [
    "db/log_test.cc"
  ],
  deps = [
    ":leveldb_testbase",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

cc_binary(
  name = "recovery_test",
  srcs = [
    "db/recovery_test.cc"
  ],
  deps = [
    ":leveldb_testbase",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

cc_binary(
  name = "skiplist_test",
  srcs = [
    "db/skiplist_test.cc"
  ],
  deps = [
    ":leveldb_testbase",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

cc_binary(
  name = "version_set_test",
  srcs = [
    "db/version_set_test.cc"
  ],
  deps = [
    ":leveldb_testbase",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

cc_binary(
  name = "version_edit_test",
  srcs = [
    "db/version_edit_test.cc"
  ],
  deps = [
    ":leveldb_testbase",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

cc_binary(
  name = "write_batch_test",
  srcs = [
    "db/write_batch_test.cc"
  ],
  deps = [
    ":leveldb_testbase",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

cc_binary(
  name = "helpers/memenv/memenv_test",
  srcs = [
    "helpers/memenv/memenv_test.cc"
  ],
  deps = [
    ":leveldb_testbase",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

cc_binary(
  name = "issues/issue178_test",
  srcs = [
    "issues/issue178_test.cc"
  ],
  deps = [
    ":leveldb_testbase",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

cc_binary(
  name = "issues/issue200_test",
  srcs = [
    "issues/issue200_test.cc"
  ],
  deps = [
    ":leveldb_testbase",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

cc_binary(
  name = "table/filter_block_test",
  srcs = [
    "table/filter_block_test.cc"
  ],
  deps = [
    ":leveldb_testbase",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

cc_binary(
  name = "table/table_test",
  srcs = [
    "table/table_test.cc"
  ],
  deps = [
    ":leveldb_testbase",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

cc_binary(
  name = "util/arena_test",
  srcs = [
    "util/arena_test.cc"
  ],
  deps = [
    ":leveldb_testbase",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

cc_binary(
  name = "util/bloom_test",
  srcs = [
    "util/bloom_test.cc"
  ],
  deps = [
    ":leveldb_testbase",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

cc_binary(
  name = "util/cache_test",
  srcs = [
    "util/cache_test.cc"
  ],
  deps = [
    ":leveldb_testbase",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

cc_binary(
  name = "util/coding_test",
  srcs = [
    "util/coding_test.cc"
  ],
  deps = [
    ":leveldb_testbase",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

cc_binary(
  name = "util/crc32c_test",
  srcs = [
    "util/crc32c_test.cc"
  ],
  deps = [
    ":leveldb_testbase",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

cc_binary(
  name = "util/env_test",
  srcs = [
    "util/env_test.cc"
  ],
  deps = [
    ":leveldb_testbase",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)

cc_binary(
  name = "util/hash_test",
  srcs = [
    "util/hash_test.cc"
  ],
  deps = [
    ":leveldb_testbase",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)
