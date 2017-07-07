# The BUILD rules for Erizo media processing unit (note it is based on libav).

package(default_visibility = ["//visibility:public"])

cc_library(
  name = "media_output",
  srcs = [
          "ExternalOutput.cpp",
          "MediaProcessor.cpp",
         ],
  hdrs = ["ExternalOutput.h",
          "MediaProcessor.h"
          ],
  deps = [
          "//stream_service:media_processor",
          "//stream_service/erizo:erizo_lib",
          "//stream_service/erizo/media/codecs:codecs",
          "//third_party/glog"
         ],
  linkopts = [
     "-lavformat-ffmpeg -lavcodec-ffmpeg -lavutil-ffmpeg"
  ],
)

cc_library(
  name = "external_input",
  srcs = [
          "ExternalInput.cpp",
          "MediaProcessor.cpp",
         ],
  hdrs = ["ExternalInput.h",
          "MediaProcessor.h",
          ],
  deps = [
          "//stream_service:media_processor",
          "//stream_service/erizo:erizo_lib",
          "//stream_service/erizo/media/codecs:codecs",
          "//third_party/glog"
         ],
  linkopts = [
     "-lavformat-ffmpeg -lavcodec-ffmpeg -lavutil-ffmpeg"
  ],
)
