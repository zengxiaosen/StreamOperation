/*
 * Copyright 2016 All Rights Reserved.
 * Author: caixinyu@orangelab.cn (Cai Xinyu)
 *
 * client_test.cc
 * -----------------------------------------------------------------------------
 * A test send captured packet to server and print received packet from server.
 * Sample usage:
 * bazel-bin/stream_service/orbit/client/client_test --logtostderr
 *    --session_id={id}  \
 *    --data_file=orbit_data/data.pb \
 * -----------------------------------------------------------------------------
 */

#include <stdio.h>

#include <vector>
#include <thread>
#include <atomic>
#include <unistd.h>

// For Gflags and Glog
#include "gflags/gflags.h"
#include "glog/logging.h"

#include "stream_service/orbit/debug_server/rtp_capture.h"

#include "client.h"

using namespace olive;
using namespace olive::client;

DEFINE_bool(only_public_ip, false, "use public ip only on candidate.");
DEFINE_string(stun_server_ip, "101.200.238.173", "The stun server's ip");
DEFINE_int32(stun_server_port, 3478, "The stun server's port");

DEFINE_string(turn_server_ip, "", "The turn server's ip");
DEFINE_int32(turn_server_port, 3478, "The turn server's port");
DEFINE_string(turn_server_username, "", "The turn server's username");
DEFINE_string(turn_server_pass, "", "The turn server's password");
DEFINE_bool(enable_red_fec, true, "EnablR Video RED/FEC codecs");
DEFINE_string(packet_capture_directory, "", "Specify the directory to store the capture file. e.g. /tmp/");
DEFINE_string(packet_capture_filename, "", "If any filename is specified, "
              "the rtp packet capture will be enabled and stored into the file."
              " e.g. video_rtp.pb");
DEFINE_string(data_file, "data.pb", "");

DEFINE_int32(session_id, -1, "The session id of the room that you want to entry.");
DEFINE_string(server_host, "127.0.0.1", "The ip/hostname of test server.");
DEFINE_int32(server_port, 10000, "The port of the test server.");
DEFINE_string(test_case, "entry_session", "If test_case is setted entry_session, indicate you can entry a room that you want go in, or create_session indicate you can create a new session.");
DEFINE_int32(max_iterations, 1, "Defines the max iteration numbers for the server testing.");
DEFINE_int32(leave_time, 600000, "After the leave_time(ms), the stream has no data coming in, then close the stream and update the statusz");

int main(int argc, char *argv[]) {
  google::InstallFailureSignalHandler();
  google::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);
  if (FLAGS_data_file.empty()) {
    LOG(ERROR) << "Filename is required!";
    LOG(ERROR) << argv[0]
               << " --logtostderr --session_id={id}"
               << " --data_file=orbit_data/data.pb";
    return 1;
  }

  for (int i = 0 ; i < FLAGS_max_iterations; i ++){
    LOG(INFO) << "restart ...";
    GrpcClient grpc_client(FLAGS_server_host, FLAGS_server_port);

    session_id_t session_id;
    if (FLAGS_test_case == "entry_session") {
      session_id = FLAGS_session_id;
    } else if (FLAGS_test_case == "create_session") {
      session_id = grpc_client.CreateSession(CreateSessionRequest::VIDEO_BRIDGE, "");
    }

    CreateStreamOption option;
    stream_id_t stream_id = grpc_client.CreateStream(session_id, option);
    if (stream_id == -1) {
      LOG(ERROR) << "stream_id is -1";
      exit(1);
    }

    grpc_client.SendSessionMessage(session_id, stream_id);

    orbit::client::OrbitClientStream stream(grpc_client,
                                            session_id,
                                            stream_id);

    stream.AddRecvRtpListener([&stream](const orbit::dataPacket &packet){
        uint proto_type = packet.type;
        stream.AddRecvRtpPacket(packet);
      });

    if (!stream.Connect()) {
      LOG(ERROR) << "stream cannot connect";
      return 1;
    }

    orbit::RtpReplay rtp_relay;
    rtp_relay.Init(FLAGS_data_file);
    std::shared_ptr<orbit::StoredPacket> packet;

    long ts = 0;
    int n = 0;
    while (true) {
      packet = rtp_relay.Next();
      if (!packet) {
        break;
      }

      int recv_len = stream.GetRecvPacketLength();
      if (recv_len > 20) {
        LOG(INFO) << "recv_len = " << recv_len;
        break;
      }
    
      if (ts != 0) {
        long long now = packet->ts();
        int sleep_time = (int)(now - ts);
        if (sleep_time != 0) {
          usleep(sleep_time * 1000);
        }
      }

      std::string type_str;
      ts = packet->ts();
      orbit::packetType type;
      auto proto_type = packet->type();
      if (proto_type == orbit::AUDIO) {
        type = orbit::AUDIO_PACKET;
      } else if (proto_type == orbit::VIDEO) {
        type = orbit::VIDEO_PACKET;
      } else if (proto_type == orbit::VIDEO_RTX) {
        type = orbit::VIDEO_RTX_PACKET;
      } else {
        type = orbit::OTHER_PACKET;
      }

      if (n % 1000 == 0) {
        LOG(INFO) << "send RTP packet ..." << n;
      }
      n++;
      stream.SendPacket(packet->data().c_str(), packet->packet_length(), type);
      stream.AddSendRtpPacket(packet);
    }
    LOG_IF(FATAL, grpc_client.CloseStream(session_id, stream_id) != 0) 
      << "Close stream:" << session_id << ":" << stream_id;
    LOG_IF(FATAL, grpc_client.CloseSession(session_id) != 0) 
      << "Close stream:" << session_id << ":" << stream_id;
    LOG(INFO) << "Iteration:" << i << " finish.";
  }
  return 0;
}

