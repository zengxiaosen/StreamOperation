#  BUILD rules for redis c++ client.
#  Fork from the git repository: https://github.com/shawn246/redis_client.git

package(default_visibility = ["//visibility:public"])

cc_library(
  name = "redis_client",
  hdrs = ["src/RedisClient.hpp",
         ],
  srcs = [
    "src/RedisClient.cpp"
  ],
  deps = [
          "//stream_service/third_party/hiredis",
          "//third_party/glog"
         ],
  includes = [
    "src/",
  ],
)

cc_binary(
  name = "test_client",
  srcs = [
    "test/TestClient.cpp",
    "test/TestBase.cpp",
    "test/TestGeneric.cpp",
    "test/TestString.cpp",
    "test/TestList.cpp",
    "test/TestSet.cpp",
    "test/TestHash.cpp",
    "test/TestZset.cpp",
    "test/TestConcur.cpp",
    "test/main.cpp",
  ],
  deps = [
    ":redis_client",
    "//third_party/glog",
    "//third_party/gflags",
  ],
)
