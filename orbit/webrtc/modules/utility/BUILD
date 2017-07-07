# The BUILD rules for webrtc chrome implementation and related module.

package(default_visibility = ["//visibility:public"])

cc_library(
  name = "utility",
  hdrs = [
            "include/audio_frame_operations.h",
            "include/file_player.h",
            "include/file_recorder.h",
            "include/process_thread.h",
            "source/coder.h",
            "source/file_player_impl.h",
            "source/file_recorder_impl.h",
            "source/process_thread_impl.h",
         ],
  srcs = [
            "source/audio_frame_operations.cc",
            "source/coder.cc",
            "source/file_player_impl.cc",
            "source/file_recorder_impl.cc",
            "source/process_thread_impl.cc",
         ],
  deps = [
    "//stream_service/orbit/webrtc:webrtc_common",
    "//stream_service/orbit/webrtc/common_audio",
    "//stream_service/orbit/webrtc/system_wrappers",
    "//stream_service/orbit/webrtc/modules/audio_coding:audio_coding",
    #"//stream_service/orbit/webrtc:media_file",
  ]
)