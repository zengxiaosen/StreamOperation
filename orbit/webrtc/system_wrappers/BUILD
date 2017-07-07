# The BUILD rules for webrtc chrome implementation and related module.

package(default_visibility = ["//visibility:public"])
 	 	
cc_library(
  name = "system_wrappers",
  srcs = [
    "source/aligned_malloc.cc",
    "source/clock.cc",
    "source/condition_variable.cc",
    "source/condition_variable_posix.cc",
    "source/cpu_features.cc",
    "source/cpu_info.cc",
    "source/critical_section.cc",
    "source/critical_section_posix.cc",
    "source/data_log_c.cc",
    "source/data_log.cc",
    "source/event.cc",
    "source/event_timer_posix.cc",
    "source/file_impl.cc",
    "source/logging.cc",
    "source/rtp_to_ntp.cc",
    "source/rw_lock.cc",
    "source/rw_lock_generic.cc",
    "source/rw_lock_posix.cc",
    "source/sleep.cc",
    "source/sort.cc",
    "source/tick_util.cc",
    "source/timestamp_extrapolator.cc",
    "source/trace_impl.cc",
    "source/trace_posix.cc",
    "source/field_trial_default.cc",
    "source/metrics_default.cc",
         ],
  hdrs = [
    "include/aligned_array.h",
    "include/aligned_malloc.h",
    "include/atomic32.h",
    "include/clock.h",
    "include/condition_variable_wrapper.h",
    "include/cpu_features_wrapper.h",
    "include/cpu_info.h",
    "include/critical_section_wrapper.h",
    "include/data_log.h",
    "include/data_log_c.h",
    "include/data_log_impl.h",
    "include/event_wrapper.h",
    "include/field_trial.h",
    "include/file_wrapper.h",
    "include/logging.h",
    "include/ref_count.h",
    "include/rtp_to_ntp.h",
    "include/metrics.h",
    "include/rw_lock_wrapper.h",
    "include/scoped_vector.h",
    "include/sleep.h",
    "include/sort.h",
    "include/static_instance.h",
    "include/stl_util.h",
    "include/stringize_macros.h",
    "include/tick_util.h",
    "include/timestamp_extrapolator.h",
    "include/trace.h",
    "source/trace_posix.h",
    "source/rw_lock_posix.h",
    "source/rw_lock_generic.h",
    "source/file_impl.h",
    "source/event_timer_posix.h",
    "source/critical_section_posix.h",
    "source/condition_variable_posix.h",
    "source/trace_impl.h",
         ],
  deps = [
          "//third_party/glog",
          "//stream_service/orbit:webrtc_includes",
          "//stream_service/orbit/webrtc/base:rtc_base",
          "//stream_service/orbit/webrtc/base:rtc_base_approved",
          "//stream_service/orbit/webrtc:webrtc_common"
         ],
)

#cc_test(
# name = "data_log_unittest",
# srcs = [
#  "source/data_log_unittest.cc",
#  "source/data_log_c_helpers_unittest.c",
# ],
# deps = [
#   ":system_wrappers",
#   "//third_party/gtest:gtest_main",
# ],
#)

cc_test(
 name = "logging_unittest",
 srcs = [
  "source/logging_unittest.cc",
 ],
 deps = [
   ":system_wrappers",
   "//third_party/gtest:gtest_main",
 ],
)


cc_test(
 name = "aligned_array_unittest.cc",
 srcs = [
  "source/aligned_array_unittest.cc",
 ],
 deps = [
   ":system_wrappers",
   "//third_party/gtest:gtest_main",
 ],
)


cc_test(
 name = "rtp_to_ntp_unittest",
 srcs = [
  "source/rtp_to_ntp_unittest.cc",
 ],
 deps = [
   ":system_wrappers",
   "//third_party/gtest:gtest_main",
 ],
)


cc_test(
 name = "aligned_malloc_unittest",
 srcs = [
  "source/aligned_malloc_unittest.cc",
 ],
 deps = [
   ":system_wrappers",
   "//third_party/gtest:gtest_main",
 ],
)

cc_test(
 name = "scoped_vector_unittest",
 srcs = [
  "source/scoped_vector_unittest.cc",
 ],
 deps = [
   ":system_wrappers",
   "//third_party/gtest:gtest_main",
 ],
)

cc_test(
 name = "clock_unittest",
 srcs = [
  "source/clock_unittest.cc",
 ],
 deps = [
   ":system_wrappers",
   "//third_party/gtest:gtest_main",
 ],
)

cc_test(
 name = "stl_util_unittest",
 srcs = [
  "source/stl_util_unittest.cc",
 ],
 deps = [
   ":system_wrappers",
   "//third_party/gtest:gtest_main",
 ],
)

cc_test(
 name = "condition_variable_unittest",
 srcs = [
  "source/condition_variable_unittest.cc",
 ],
 deps = [
   ":system_wrappers",
   "//third_party/gtest:gtest_main",
 ],
)

cc_test(
 name = "metrics_unittest",
 srcs = [
  "source/metrics_unittest.cc",
 ],
 deps = [
   "//stream_service/orbit/webrtc/test:histogram",
   ":system_wrappers",
   "//third_party/gtest:gtest_main",
 ],
)

cc_test(
 name = "stringize_macros_unittest",
 srcs = [
  "source/stringize_macros_unittest.cc",
 ],
 deps = [
   ":system_wrappers",
   "//third_party/gtest:gtest_main",
 ],
)

cc_test(
 name = "critical_section_unittest",
 srcs = [
  "source/critical_section_unittest.cc",
 ],
 deps = [
   ":system_wrappers",
   "//third_party/gtest:gtest_main",
 ],
)

cc_test(
 name = "ntp_time_unittest",
 srcs = [
  "source/ntp_time_unittest.cc",
 ],
 deps = [
   ":system_wrappers",
   "//third_party/gtest:gtest_main",
 ],
)
