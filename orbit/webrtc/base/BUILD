# BUILD file for webrtc common libraries

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "logging",
    hdrs = [
             "logging.h"
           ],
    srcs = [
             "logging.cc"
           ],
    deps = [
             "//stream_service/orbit:webrtc_includes",
             "//third_party/glog",
           ],
)

cc_library(
    name="rtc_base_approved",
    srcs = [
              "bitbuffer.cc",
              "buffer.cc",
              "bufferqueue.cc",
              "bytebuffer.cc",
              "checks.cc",
              "criticalsection.cc",
              "event.cc",
              "event_tracer.cc",
              "exp_filter.cc",
              "md5.cc",
              "logging.cc",
              "md5digest.cc",
              "platform_file.cc",
              "platform_thread.cc",
              "random.cc",
              "stringencode.cc",
              "stringutils.cc",
              "systeminfo.cc",
              "thread_checker_impl.cc",
              "timeutils.cc",
           ],
    hdrs = [
              "array_view.h",
              "atomicops.h",
              "bitbuffer.h",
              "buffer.h",
              "bufferqueue.h",
              "bytebuffer.h",
              "byteorder.h",
              "checks.h",
              "constructormagic.h",
              "criticalsection.h",
              "deprecation.h",
              "event_tracer.h",
              "exp_filter.h",
              "logging.h",
              "md5.h",
              "md5digest.h",
              "optional.h",
              "platform_file.h",
              "platform_thread.h",
              "systeminfo.h",
              "template_util.h",
              "thread_annotations.h",
              "thread_checker.h",
              "thread_checker_impl.h",
              "timeutils.h",
              "trace_event.h",
              "platform_thread_types.h",
              "random.h",
              "safe_conversions.h",
              "safe_conversions_impl.h",
              "scoped_ptr.h",
              "stringencode.h",
              "stringutils.h",
              "event.h",
           ],
    deps = [
             "//stream_service/orbit:webrtc_includes",
             "//third_party/glog",
           ],
)


cc_library(
     name = "rtc_base",
     defines = [ "LOGGING=1" ],
     hdrs = [ 
              "arraysize.h",
              "asyncfile.h",
              "asyncinvoker-inl.h",
              "asyncinvoker.h",
              "asyncpacketsocket.h",
              "asyncresolverinterface.h",
              "asyncsocket.h",
              "asynctcpsocket.h",
              "asyncudpsocket.h",
              "autodetectproxy.h",
              "base64.h",
              "common.h",
              "crc32.h",
              "cryptstring.h",
              "diskcache.h",
              "filerotatingstream.h",
              "fileutils.h",
              "firewallsocketserver.h",
              "flags.h",
              "format_macros.h",
              "gunit_prod.h",
              "helpers.h",
              "httpbase.h",
              "httpclient.h",
              "httpcommon-inl.h",
              "httpcommon.h",
              #"httprequest.h",
              "ipaddress.h",
              "linked_ptr.h",
              "mathutils.h",
              "messagedigest.h",
              "messagehandler.h",
              "messagequeue.h",
              "nethelpers.h",
              "network.h",
              "networkmonitor.h",
              "nullsocketserver.h",
              "pathutils.h",
              "physicalsocketserver.h",
              "proxydetect.h",
              "proxyinfo.h",
              "ratelimiter.h",
              "ratetracker.h",
              "rtccertificate.h",
              "scoped_autorelease_pool.h",
              "sha1.h",
              "sha1digest.h",
              "signalthread.h",
              "sigslot.h",
              "sigslotrepeater.h",
              "socket.h",
              #"sslsocketfactory.h",
              "taskparent.h",
              "taskrunner.h",
              "thread.h",
              "timing.h",
              "urlencode.h",
              "worker.h",
              "socketadapters.h",
              "socketaddress.h",
              "socketaddresspair.h",
              "socketfactory.h",
              "socketpool.h",
              "socketserver.h",
              "socketstream.h",
              "stream.h",
              "task.h",
              "ifaddrs-android.h",
              "unixfilesystem.h",
              "bandwidthsmoother.h",
              "bind.h",
              "callback.h",
              "fileutils_mock.h",
              "httpserver.h",
              "proxyserver.h",
              "refcount.h",
              "referencecountedsingletonfactory.h",
              "rollingaccumulator.h",
              "scoped_ref_ptr.h",
              "scopedptrcollection.h",
              "sec_buffer.h",
              "logsinks.h",
              "multipart.h",
              "natserver.h",
              "natsocketfactory.h",
              "nattypes.h",
              "optionsfile.h",
              "sharedexclusivelock.h",
              "testclient.h",
              "transformadapter.h",
              "versionparsing.h",
              "virtualsocketserver.h",
              "posix.h",
              "dbus.h",
              "libdbusglibsymboltable.h",
              "linuxfdwalk.h",
              "linux.h",
            ],
  srcs = [
          "asyncfile.cc",
          "asyncinvoker.cc",
          "asyncpacketsocket.cc",
          "asyncresolverinterface.cc",
          "asyncsocket.cc",
          "asyncudpsocket.cc",
          "asynctcpsocket.cc",
          "autodetectproxy.cc",
          "base64.cc",
          "common.cc",
          "crc32.cc",
          "cryptstring.cc",
          "diskcache.cc",
          "filerotatingstream.cc",
          "fileutils.cc",
          "firewallsocketserver.cc",
          "flags.cc",
          "helpers.cc",
          "httpbase.cc",
          "httpclient.cc",
          "httpcommon.cc",
          #"httprequest.cc",
          "ipaddress.cc",
          "messagedigest.cc",
          "messagehandler.cc",
          "messagequeue.cc",
          "nethelpers.cc",
          "network.cc",
          "networkmonitor.cc",
          "pathutils.cc",
          "physicalsocketserver.cc",
          "proxydetect.cc",
          "proxyinfo.cc",
          "ratelimiter.cc",
          "ratetracker.cc",
          "rtccertificate.cc",
          "sha1.cc",
          "sha1digest.cc",
          "signalthread.cc",
          #"sslsocketfactory.cc",
          "sigslot.cc",
          "socketadapters.cc",
          "socketaddress.cc",
          "socketaddresspair.cc",
          "socketpool.cc",
          "socketstream.cc",
          "stream.cc",
          "task.cc",
          "taskparent.cc",
          "taskrunner.cc",
          "thread.cc",
          "timing.cc",
          "urlencode.cc",
          "worker.cc",
          "ifaddrs_converter.cc",
          "unixfilesystem.cc",
          "bandwidthsmoother.cc",
          "httpserver.cc",
          "logsinks.cc",
          "multipart.cc",
          "natserver.cc",
          "natsocketfactory.cc",
          "nattypes.cc",
          "optionsfile.cc",
          "profiler.cc",
          "profiler.h",
          "proxyserver.cc",
          
          "sharedexclusivelock.cc",
          "testclient.cc",
          "transformadapter.cc",
          "versionparsing.cc",
          "virtualsocketserver.cc",
          "posix.cc",
          "dbus.cc",
          "libdbusglibsymboltable.cc",
          "linuxfdwalk.c",
          "linux.cc",
  ],
       deps = [
           ":rtc_base_approved",
           "//stream_service/orbit/webrtc:webrtc_common",
           "//stream_service/orbit:webrtc_includes",
           "//third_party/glog",
         ],
)



cc_library(
  name = "stringutils",
  hdrs = [
           "stringutils.h",
         ],
  srcs = [
           "stringutils.cc",
         ],
  deps = [
          ":rtc_base",
          "//third_party/glog",
         ],
)


cc_test(
 name = "stringutils_unittest",
 srcs = [
  "stringutils_unittest.cc",
 ],
 deps = [
   ":stringutils",
   "//stream_service/orbit:webrtc_includes",
   "//third_party/gtest:gtest_main",
 ],
)

cc_library(
  name = "stringencode",
  hdrs = [
           "stringencode.h",
         ],
  srcs = [
           "stringencode.cc",
         ],      
  deps = [
          ":rtc_base",
          ":stringutils",
          "//stream_service/orbit:webrtc_includes",
          "//third_party/glog",
         ],
)



cc_library(
  name = "httpcommon",
  hdrs = [
           "httpcommon.h",
           "httpcommon-inl.h"
         ],
  srcs = [
           "httpcommon.cc",
         ],
  deps = [
          ":rtc_base",
          ":stringencode",
          "//third_party/glog",
         ],
)

cc_library(
    name = "thread_checker",
    hdrs = [
             "thread_checker.h",
             "thread_checker_impl.h"
           ],
    srcs = [
             "thread_checker_impl.cc"
           ],
    deps = [
             ":rtc_base",
             ":stringencode",
             "//third_party/glog",
           ],
)

cc_test(
 name = "httpcommon_unittest",
 srcs = [
  "httpcommon_unittest.cc",
 ],
 deps = [
   ":httpcommon",
   "//third_party/gtest:gtest_main",
 ],
)


cc_test(
 name = "array_view_unittest",
 srcs = [
  "array_view_unittest.cc",
 ],
 deps = [
   ":rtc_base",
   "//third_party/gtest:gtest_main",
 ],
)

#cc_test(
# name = "httpserver_unittest",
# srcs = [
#  "httpserver_unittest.cc",
# ],
# deps = [
#   ":rtc_base",
#   "//third_party/gtest:gtest_main",
# ],
#)

cc_test(
 name = "rollingaccumulator_unittest",
 srcs = [
  "rollingaccumulator_unittest.cc",
 ],
 deps = [
   ":rtc_base",
   "//third_party/gtest:gtest_main",
 ],
)


cc_test(
 name = "asynctcpsocket_unittest",
 srcs = [
  "asynctcpsocket_unittest.cc",
 ],
 deps = [
   ":rtc_base",
   "//third_party/gtest:gtest_main",
 ],
)


cc_test(
 name = "ipaddress_unittest",
 srcs = [
  "ipaddress_unittest.cc",
 ],
 deps = [
   ":rtc_base",
   "//third_party/gtest:gtest_main",
 ],
)


cc_test(
 name = "scopedptrcollection_unittest",
 srcs = [
  "scopedptrcollection_unittest.cc",
 ],
 deps = [
   ":rtc_base",
   "//third_party/gtest:gtest_main",
 ],
)

cc_test(
 name = "asyncudpsocket_unittest",
 srcs = [
  "asyncudpsocket_unittest.cc",
 ],
 deps = [
   ":rtc_base",
   "//third_party/gtest:gtest_main",
 ],
)

# JSON is not installed currently.
#
#cc_test(
# name = "json_unittest",
# srcs = [
#  "json_unittest.cc",
# ],
# deps = [
#   ":rtc_base",
#   "//third_party/gtest:gtest_main",
# ],
#)


cc_test(
 name = "sha1digest_unittest",
 srcs = [
  "sha1digest_unittest.cc",
 ],
 deps = [
   ":rtc_base",
   "//third_party/gtest:gtest_main",
 ],
)


cc_test(
 name = "atomicops_unittest",
 srcs = [
  "atomicops_unittest.cc",
 ],
 deps = [
   ":rtc_base",
   "//third_party/gtest:gtest_main",
 ],
)



cc_test(
 name = "sharedexclusivelock_unittest",
 srcs = [
  "sharedexclusivelock_unittest.cc",
 ],
 deps = [
   ":rtc_base",
   "//third_party/gtest:gtest_main",
 ],
)

cc_test(
 name = "autodetectproxy_unittest",
 srcs = [
  "autodetectproxy_unittest.cc",
 ],
 deps = [
   ":rtc_base",
   "//third_party/gtest:gtest_main",
 ],
)

cc_test(
 name = "linux_unittest",
 srcs = [
  "linux_unittest.cc",
 ],
 deps = [
   ":rtc_base",
   "//third_party/gtest:gtest_main",
 ],
)


cc_test(
 name = "bandwidthsmoother_unittest",
 srcs = [
  "bandwidthsmoother_unittest.cc",
 ],
 deps = [
   ":rtc_base",
   "//third_party/gtest:gtest_main",
 ],
)

cc_test(
 name = "signalthread_unittest",
 srcs = [
  "signalthread_unittest.cc",
 ],
 deps = [
   ":rtc_base",
   "//third_party/gtest:gtest_main",
 ],
)


cc_test(
 name = "linuxfdwalk_unittest",
 srcs = [
  "linuxfdwalk_unittest.cc",
 ],
 deps = [
   ":rtc_base",
   "//third_party/gtest:gtest_main",
 ],
)


cc_test(
 name = "sigslot_unittest",
 srcs = [
  "sigslot_unittest.cc",
 ],
 deps = [
   ":rtc_base",
   "//third_party/gtest:gtest_main",
 ],
)

cc_test(
 name = "base64_unittest",
 srcs = [
  "base64_unittest.cc",
 ],
 deps = [
   ":rtc_base",
   "//third_party/gtest:gtest_main",
 ],
)
