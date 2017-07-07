/*
 * Copyright 2016 (C) Orange Labs Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * rtp_capture.h
 * ---------------------------------------------------------------------------
 * Impletes util class which is used to capture and replay the RTP packets.
 * ---------------------------------------------------------------------------
 */

#include "rtp_capture.h"
#include "glog/logging.h"

#include "stream_service/orbit/rtp/rtp_headers.h"
#include "stream_service/orbit/base/timeutil.h"
#include "stream_service/orbit/debug_server/stored_rtp.pb.h"

namespace orbit {
  RtpCapture::RtpCapture() {
  }
  RtpCapture::RtpCapture(const string& export_file) {
    SetExportFile(export_file);
  }

  void RtpCapture::SetExportFile(const string& export_file){
    if (!file::Open(export_file, "wb", &file_, file::Defaults()).ok()) {
      LOG(FATAL) << "Cannot open " << export_file;
    }
    file_path_ = export_file;
    writer_ = new RecordWriter(file_);
  }

  RtpCapture::~RtpCapture() {
    Destroy();
  }

  void RtpCapture::Destroy() {
    writer_->Close();
  }

  void RtpCapture::CapturePacket(int transport_id, const dataPacket& packet) {
    std::lock_guard<std::mutex> lock(mutex_);
    StoredPacket packet_proto;
    if (packet.type == AUDIO_PACKET) {
      packet_proto.set_type(AUDIO);
    } else if (packet.type == VIDEO_PACKET) {
      packet_proto.set_type(VIDEO);
    } else if (packet.type == VIDEO_RTX_PACKET) {
      packet_proto.set_type(VIDEO_RTX);
    }

    RtcpHeader* chead = (RtcpHeader*)(&(packet.data[0]));
    if (chead->isRtcp()) {
      packet_proto.set_packet_type(RTCP_PACKET);
    } else {
      packet_proto.set_packet_type(RTP_PACKET);
    }
    packet_proto.set_packet_length(packet.length);
    packet_proto.set_header_length(12);
    packet_proto.set_data(packet.data, packet.length);

    long long ts = getTimeMS();
    packet_proto.set_ts(ts);

    packet_proto.set_remote_ntp_time_ms(packet.remote_ntp_time_ms);
    packet_proto.set_rtp_timestamp(packet.rtp_timestamp);

    // Set the transport_id.
    packet_proto.set_transport_id(transport_id);

    writer_->WriteProtocolMessage(packet_proto);
  }

  void RtpReplay::Init(const string& file) {
    if (!file::Open(file, "rb", &file_, file::Defaults()).ok()) {
      LOG(FATAL) << "Cannot open " << file;
    }
    reader_ = new RecordReader(file_);
  }

  std::shared_ptr<StoredPacket> RtpReplay::Next() {
    std::shared_ptr<StoredPacket> packet(new StoredPacket);
    if (reader_->ReadProtocolMessage(packet.get())) {
      return packet;
    }
    return NULL;
  }
  
}  // namespace orbit
