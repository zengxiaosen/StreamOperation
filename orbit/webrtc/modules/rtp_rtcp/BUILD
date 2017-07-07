# BUILD file for webrtc/modules/rtp_rtcp

package(default_visibility = ["//visibility:public"])

cc_library(
  name = "rtcp_packet",
  hdrs = [
    "source/rtcp_packet.h",
    "source/rtcp_packet/app.h",
    "source/rtcp_packet/bye.h",
    "source/rtcp_packet/compound_packet.h",
    "source/rtcp_packet/extended_jitter_report.h",
    "source/rtcp_packet/nack.h",
    "source/rtcp_packet/pli.h",
    "source/rtcp_packet/psfb.h",
    "source/rtcp_packet/receiver_report.h",
    "source/rtcp_packet/report_block.h",
    "source/rtcp_packet/rrtr.h",
    "source/rtcp_packet/rtpfb.h",
    "source/rtcp_packet/sli.h",
    "source/rtcp_packet/tmmbn.h",
    "source/rtcp_packet/tmmbr.h",
    "source/rtcp_packet/transport_feedback.h",
    "source/rtcp_packet/voip_metric.h",
         ],
  srcs = [

    "source/rtcp_packet.cc",
    "source/rtcp_packet/app.cc",
    "source/rtcp_packet/bye.cc",
    "source/rtcp_packet/compound_packet.cc",
    "source/rtcp_packet/dlrr.cc",
    "source/rtcp_packet/dlrr.h",
    "source/rtcp_packet/extended_jitter_report.cc",
    "source/rtcp_packet/nack.cc",
    "source/rtcp_packet/pli.cc",
    "source/rtcp_packet/psfb.cc",
    "source/rtcp_packet/receiver_report.cc",
    "source/rtcp_packet/report_block.cc",
    "source/rtcp_packet/rrtr.cc",
    "source/rtcp_packet/rtpfb.cc",
    "source/rtcp_packet/sli.cc",
    "source/rtcp_packet/tmmbn.cc",
    "source/rtcp_packet/tmmbr.cc",
    "source/rtcp_packet/transport_feedback.cc",
    "source/rtcp_packet/voip_metric.cc",
    "source/rtcp_utility.cc",
         ],
  deps = [
          "//third_party/glog",
          "//stream_service/orbit:webrtc_includes",
          "//stream_service/orbit/webrtc/base:rtc_base",
          "//stream_service/orbit/webrtc/system_wrappers:system_wrappers"
         ],
)

cc_library(
  name = "rtp_rtcp_defines",
  hdrs = [
           "include/rtp_rtcp_defines.h",

         ],
  srcs = [
         ],
  deps = [
          "rtcp_packet",
          "//third_party/glog",
          "//stream_service/orbit:webrtc_includes",
          "//stream_service/orbit/webrtc/base:rtc_base",
          "//stream_service/orbit/webrtc/system_wrappers:system_wrappers"
         ],
)


cc_library(
  name = "rtp_rtcp",
  hdrs = [
    "include/fec_receiver.h",
    "include/receive_statistics.h",
    "include/remote_ntp_time_estimator.h",
    "include/rtp_header_parser.h",
    "include/rtp_payload_registry.h",
    "include/rtp_receiver.h",
    "include/rtp_rtcp.h",
    "include/rtp_rtcp_defines.h",
    "mocks/mock_rtp_rtcp.h",
    "source/bitrate.h",
    "source/byte_io.h",
    "source/dtmf_queue.h",
    "source/fec_private_tables_bursty.h",
    "source/fec_private_tables_random.h",
    "source/fec_receiver_impl.h",
    "source/forward_error_correction.h",
    "source/forward_error_correction_internal.h",
    "source/h264_bitstream_parser.h",
    "source/h264_sps_parser.h",
    "source/mock/mock_rtp_payload_strategy.h",
    "source/packet_loss_stats.h",
    "source/producer_fec.h",
    "source/receive_statistics_impl.h",
    "source/rtcp_packet.h",
    "source/rtcp_packet/app.h",
    "source/rtcp_packet/bye.h",
    "source/rtcp_packet/compound_packet.h",
    "source/rtcp_packet/extended_jitter_report.h",
    "source/rtcp_packet/dlrr.h",
    "source/rtcp_packet/nack.h",
    "source/rtcp_packet/pli.h",
    "source/rtcp_packet/psfb.h",
    "source/rtcp_packet/report_block.h",
    "source/rtcp_packet/rrtr.h",
    "source/rtcp_packet/rtpfb.h",
    "source/rtcp_packet/sli.h",
    "source/rtp_format_h264.h",
    "source/rtp_format_video_generic.h",
    "source/rtp_format_vp8.h",
    "source/rtp_format_vp9.h",
    "source/rtp_header_extension.h",
    "source/rtp_packet_history.h",
    "source/rtp_receiver_audio.h",
    "source/rtp_receiver_impl.h",
    "source/rtp_receiver_strategy.h",
    "source/rtp_rtcp_config.h",
    "source/rtp_rtcp_impl.h",
    "source/rtp_sender.h",
    "source/rtp_sender_audio.h",
    "source/rtp_sender_video.h",
    "source/rtp_utility.h",
    "source/ssrc_database.h",
    "source/tmmbr_help.h",
    "source/video_codec_information.h",
    "source/vp8_partition_aggregator.h",
    "source/rtcp_packet/receiver_report.h",
    "source/rtcp_packet/tmmbn.h",
    "source/rtcp_packet/tmmbr.h",
    "source/rtcp_packet/transport_feedback.h",
    "source/rtcp_packet/voip_metric.h",
    "source/rtcp_receiver.h",
    "source/rtcp_receiver_help.h",
    "source/rtcp_sender.h",
    "source/rtcp_utility.h",
    "source/rtp_format.h",
    "source/rtp_receiver_video.h",
  ],
  srcs = [
      "source/bitrate.cc",
    "source/dtmf_queue.cc",
    "source/fec_receiver_impl.cc",
    "source/forward_error_correction.cc",
    "source/forward_error_correction_internal.cc",
    "source/h264_bitstream_parser.cc",
    "source/h264_sps_parser.cc",
    "source/packet_loss_stats.cc",
    "source/producer_fec.cc",
    "source/receive_statistics_impl.cc",
    "source/remote_ntp_time_estimator.cc",
    "source/rtcp_packet.cc",
    "source/rtcp_packet/app.cc",
    "source/rtcp_packet/bye.cc",
    "source/rtcp_packet/compound_packet.cc",
    "source/rtcp_packet/dlrr.cc",
    "source/rtcp_packet/extended_jitter_report.cc",
    "source/rtcp_packet/nack.cc",
    "source/rtcp_packet/pli.cc",
    "source/rtcp_packet/psfb.cc",
    "source/rtcp_packet/receiver_report.cc",
    "source/rtcp_packet/report_block.cc",
    "source/rtcp_packet/rrtr.cc",
    "source/rtcp_packet/rtpfb.cc",
    "source/rtcp_packet/sli.cc",
    "source/rtcp_packet/tmmbn.cc",
    "source/rtcp_packet/tmmbr.cc",
    "source/rtcp_packet/transport_feedback.cc",
    "source/rtcp_packet/voip_metric.cc",
    "source/rtcp_receiver.cc",
    "source/rtcp_receiver_help.cc",
    "source/rtcp_sender.cc",
    "source/rtcp_utility.cc",
    "source/rtp_format.cc",
    "source/rtp_format_h264.cc",
    "source/rtp_format_video_generic.cc",
    "source/rtp_format_vp8.cc",
    "source/rtp_format_vp9.cc",
    "source/rtp_header_extension.cc",
    "source/rtp_header_parser.cc",
    "source/rtp_packet_history.cc",
    "source/rtp_payload_registry.cc",
    "source/rtp_receiver_audio.cc",
    "source/rtp_receiver_impl.cc",
    "source/rtp_receiver_strategy.cc",
    "source/rtp_receiver_video.cc",
    "source/rtp_rtcp_impl.cc",
    "source/rtp_sender.cc",
    "source/rtp_sender_audio.cc",
    "source/rtp_sender_video.cc",
    "source/rtp_utility.cc",
    "source/ssrc_database.cc",
    "source/tmmbr_help.cc",
    "source/vp8_partition_aggregator.cc",
  ],
  deps = [
          "//third_party/glog",
          "//stream_service/orbit:webrtc_includes",
          "//stream_service/orbit/webrtc/base:rtc_base",
          "//stream_service/orbit/webrtc/system_wrappers:system_wrappers",
          "//stream_service/orbit/webrtc/modules/remote_bitrate_estimator:remote_bitrate_estimator"
         ],
  linkopts = ["-lm"],
)

cc_test(
  name = "packet_loss_stats_test",
  srcs = [
           "source/packet_loss_stats_unittest.cc"
         ],
  deps = [
           ":rtp_rtcp",
           "//third_party/gtest:gtest_main",
           "//third_party/glog",
           "//stream_service/orbit/webrtc/base:logging",
         ],
)


cc_test(
  name = "byte_io_unittest",
  srcs = [
           "source/byte_io_unittest.cc"
         ],
  deps = [
           ":rtp_rtcp",
           "//third_party/gtest:gtest_main",
           "//third_party/glog",
           "//stream_service/orbit/webrtc/base:logging",
         ],
)
cc_test(
  name = "packet_loss_stats_unittest",
  srcs = [
           "source/packet_loss_stats_unittest.cc"
         ],
  deps = [
           ":rtp_rtcp",
           "//third_party/gtest:gtest_main",
           "//third_party/glog",
           "//stream_service/orbit/webrtc/base:logging",
         ],
)
cc_test(
  name = "rtcp_packet_unittest",
  srcs = [
           "source/rtcp_packet_unittest.cc"
         ],
  deps = [
           ":rtp_rtcp",
           "//third_party/gmock:gmock_main",
           "//third_party/gtest:gtest_main",
           "//third_party/glog",
           "//stream_service/orbit/webrtc/base:logging",
           "//stream_service/orbit/webrtc/test:rtcp_packet_parser",
         ],
)
cc_test(
  name = "rtp_format_h264_unittest",
  srcs = [
           "source/rtp_format_h264_unittest.cc"
         ],
  deps = [
           ":rtp_rtcp",
           "//third_party/gmock:gmock_main",
           "//third_party/gtest:gtest_main",
           "//third_party/glog",
           "//stream_service/orbit/webrtc/base:logging",
         ],
)
cc_test(
  name = "rtp_payload_registry_unittest",
  srcs = [
           "source/rtp_payload_registry_unittest.cc"
         ],
  deps = [
           ":rtp_rtcp",
           "//third_party/gmock:gmock_main",
           "//third_party/gtest:gtest_main",
           "//third_party/glog",
           "//stream_service/orbit/webrtc/base:logging",
         ],
)
cc_test(
  name = "fec_receiver_unittest",
  srcs = [
           "source/fec_receiver_unittest.cc",
           "source/fec_test_helper.cc"
         ],
  deps = [
           ":rtp_rtcp",
           "//third_party/gmock:gmock_main",
           "//third_party/gtest:gtest_main",
           "//third_party/glog",
           "//stream_service/orbit/webrtc/base:logging",
         ],
)
cc_test(
  name = "producer_fec_unittest",
  srcs = [
           "source/producer_fec_unittest.cc",
           "source/fec_test_helper.cc"
         ],
  deps = [
           ":rtp_rtcp",
           "//third_party/gtest:gtest_main",
           "//third_party/gmock:gmock_main",
           "//third_party/glog",
           "//stream_service/orbit/webrtc/base:logging",
         ],
)   
cc_test(
  name = "rtcp_receiver_unittest",
  srcs = [
           "source/rtcp_receiver_unittest.cc",
           "source/fec_test_helper.cc"
         ],
  deps = [
           ":rtp_rtcp",
           "//third_party/gmock:gmock_main",
           "//third_party/gtest:gtest_main",
           "//third_party/glog",
           "//stream_service/orbit/webrtc/base:logging",
         ],
) 
cc_test(
  name = "rtp_format_vp8_unittest",
  srcs = [
           "source/rtp_format_vp8_unittest.cc",
           "source/rtp_format_vp8_test_helper.cc"
],
  deps = [
           ":rtp_rtcp",
           "//third_party/gmock:gmock_main",
           "//third_party/gtest:gtest_main",
           "//third_party/glog",
           "//stream_service/orbit/webrtc/base:logging",
         ],
)    
cc_test(
  name = "rtp_rtcp_impl_unittest",
  srcs = [
           "source/rtp_rtcp_impl_unittest.cc"
         ],
  deps = [
           ":rtp_rtcp",
           "//third_party/gtest:gtest_main",
           "//third_party/gmock:gmock_main",
           "//third_party/glog",
           "//stream_service/orbit/webrtc/base:logging",
           "//stream_service/orbit/webrtc/test:rtcp_packet_parser",
         ],
)
cc_test(
  name = "h264_bitstream_parser_unittest",
  srcs = [
           "source/h264_bitstream_parser_unittest.cc"
         ],
  deps = [
           ":rtp_rtcp",
           "//third_party/gtest:gtest_main",
           "//third_party/glog",
           "//third_party/gmock:gmock_main",
           "//stream_service/orbit/webrtc/base:logging",
         ],
) 

cc_test(
  name = "receive_statistics_unittest",
  srcs = [
           "source/receive_statistics_unittest.cc"
         ],
  deps = [
           ":rtp_rtcp",
           "//third_party/gmock:gmock_main",
           "//third_party/gtest:gtest_main",
           "//third_party/glog",
           "//stream_service/orbit/webrtc/base:logging",
         ],
)         
cc_test(
  name = "rtcp_sender_unittest",
  srcs = [
           "source/rtcp_sender_unittest.cc"
         ],
  deps = [
           ":rtp_rtcp",
           "//third_party/gmock:gmock_main",
           "//third_party/gtest:gtest_main",
           "//third_party/glog",
           "//stream_service/orbit/webrtc/base:logging",
           "//stream_service/orbit/webrtc/test:rtcp_packet_parser",
         ],
)    
cc_test(
  name = "rtp_format_vp9_unittest",
  srcs = [
           "source/rtp_format_vp9_unittest.cc"
         ],
  deps = [
           ":rtp_rtcp",
           "//third_party/gmock:gmock_main",
           "//third_party/gtest:gtest_main",
           "//third_party/glog",
           "//stream_service/orbit/webrtc/base:logging",
         ],
)        
cc_test(
  name = "rtp_sender_unittest",
  srcs = [
           "source/rtp_sender_unittest.cc"
         ],
  deps = [
           ":rtp_rtcp",
           "//third_party/gmock:gmock_main",
           "//third_party/gtest:gtest_main",
           "//third_party/glog",
           "//stream_service/orbit/webrtc/base:logging",
         ],
)
cc_test(
  name = "h264_sps_parser_unittest",
  srcs = [
           "source/h264_sps_parser_unittest.cc"
         ],
  deps = [
           ":rtp_rtcp",
           "//third_party/gmock:gmock_main",
           "//third_party/gtest:gtest_main",
           "//third_party/glog",
           "//stream_service/orbit/webrtc/base:logging",
         ],
)        
cc_test(
  name = "remote_ntp_time_estimator_unittest",
  srcs = [
           "source/remote_ntp_time_estimator_unittest.cc"
         ],
  deps = [
           ":rtp_rtcp",
           "//third_party/gtest:gtest_main",
           "//third_party/gmock:gmock_main",
           "//third_party/glog",
           "//stream_service/orbit/webrtc/base:logging",
         ],
)  
cc_test(
  name = "rtcp_utility_unittest",
  srcs = [
           "source/rtcp_utility_unittest.cc"
         ],
  deps = [
           ":rtp_rtcp",
           "//third_party/gmock:gmock_main",
           "//third_party/gtest:gtest_main",
           "//third_party/glog",
           "//stream_service/orbit/webrtc/base:logging",
         ],
)   
cc_test(
  name = "rtp_header_extension_unittest",
  srcs = [
           "source/rtp_header_extension_unittest.cc"
         ],
  deps = [
           ":rtp_rtcp",
           "//third_party/gmock:gmock_main",
           "//third_party/gtest:gtest_main",
           "//third_party/glog",
           "//stream_service/orbit/webrtc/base:logging",
         ],
)  
cc_test(
  name = "time_util_unittest",
  srcs = [
           "source/time_util_unittest.cc"
         ],
  deps = [
           ":rtp_rtcp",
           "//third_party/gmock:gmock_main",
           "//third_party/gtest:gtest_main",
           "//third_party/glog",
           "//stream_service/orbit/webrtc/base:logging",
         ],
)
cc_test(
  name = "nack_rtx_unittest",
  srcs = [
           "source/nack_rtx_unittest.cc"
         ],
  deps = [
           ":rtp_rtcp",
           "//third_party/gmock:gmock_main",
           "//third_party/gtest:gtest_main",
           "//third_party/glog",
           "//stream_service/orbit/webrtc/base:logging",
         ],
)               
cc_test(
  name = "rtcp_format_remb_unittest",
  srcs = [
           "source/rtcp_format_remb_unittest.cc"
         ],
  deps = [
           ":rtp_rtcp",
           "//third_party/gmock:gmock_main",
           "//third_party/gtest:gtest_main",
           "//third_party/glog",
           "//stream_service/orbit/webrtc/base:logging",
           "//stream_service/orbit/webrtc/test:null_transport",
           "//stream_service/orbit/webrtc/modules/remote_bitrate_estimator:remote_bitrate_estimator",
         ],
)           
cc_test(
  name = "rtp_fec_unittest",
  srcs = [
           "source/rtp_fec_unittest.cc"
         ],
  deps = [
           ":rtp_rtcp",
           "//third_party/gmock:gmock_main",
           "//third_party/gtest:gtest_main",
           "//third_party/glog",
           "//stream_service/orbit/webrtc/base:logging",
         ],
)        
cc_test(
  name = "rtp_packet_history_unittest",
  srcs = [
           "source/rtp_packet_history_unittest.cc"
         ],
  deps = [
           ":rtp_rtcp",
           "//third_party/gmock:gmock_main",
           "//third_party/gtest:gtest_main",
           "//third_party/glog",
           "//stream_service/orbit/webrtc/base:logging",
         ],
)    

cc_test(
  name = "vp8_partition_aggregator_unittest",
  srcs = [
           "source/vp8_partition_aggregator_unittest.cc"
         ],
  deps = [
           ":rtp_rtcp",
           "//third_party/gmock:gmock_main",
           "//third_party/gtest:gtest_main",
           "//third_party/glog",
           "//stream_service/orbit/webrtc/base:logging",
         ],
)


cc_test(
  name = "test_fec",
  srcs = [
           "test/testFec/test_fec.cc",
           "test/testFec/average_residual_loss_xor_codes.h",
         ],
  deps = [
           ":rtp_rtcp",
           "//third_party/gmock:gmock_main",
           "//third_party/gtest:gtest_main",
           "//third_party/glog",
           "//stream_service/orbit/webrtc/base:logging",
         ],
)
