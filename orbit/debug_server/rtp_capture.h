/*
 * Copyright 2016 (C) Orange Labs Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * rtp_capture.h
 * ---------------------------------------------------------------------------
 * Defines the interface and functions to a util class which is used to capture
 * and replay the RTP packets.
 * ---------------------------------------------------------------------------
 */

#pragma once

#include "stream_service/orbit/debug_server/stored_rtp.pb.h"
#include "stream_service/orbit/base/recordio.h"
#include "stream_service/orbit/media_definitions.h"
#include <mutex>

namespace orbit {

class RtpCapture {
public:
  RtpCapture();
  RtpCapture(const string& export_file);
  ~RtpCapture();
  void SetExportFile(const string& export_file);
  void CapturePacket(int transport_id, const dataPacket& packet);
  void SetFolderPath(const string& folder) {
    folder_path_ = folder;
  }

  const string& GetFolderPath() {
    return folder_path_;
  }

  const string& GetFilePath() {
    return file_path_;
  }

  bool IsReady() {
    return file_ != NULL;
  }
private:  
  void Destroy();

  RecordWriter* writer_;
  File* file_ = NULL;
  mutable std::mutex mutex_;

  string file_path_;
  string folder_path_;
};

class RtpReplay {
public:
  RtpReplay() {
  }
  ~RtpReplay() {
    reader_->Close();
    delete reader_;
    delete file_;
  }
  void Init(const string& file);
  std::shared_ptr<StoredPacket> Next();
private:
  RecordReader* reader_;
  File* file_;
};

 
}  // namespace orbit
