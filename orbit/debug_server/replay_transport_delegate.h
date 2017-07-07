/*
 * Copyright 2016 (C) Orange Labs Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * replay_transport_delegate.h
 * ---------------------------------------------------------------------------
 * Defines the interface and functions for a fake transport delegate for
 * replaying the RTP packets.
 * ---------------------------------------------------------------------------
 */

#pragma once

#include "stream_service/orbit/transport_delegate.h"

namespace orbit {
 
class ReplayTransportDelegate : public TransportDelegate {
 public:
  ReplayTransportDelegate(bool audio_enabled, bool video_enabled,
                          bool trickle_enabled, const IceConfig& ice_config, int session_id, int stream_id) :
  TransportDelegate(audio_enabled, video_enabled, trickle_enabled, ice_config, session_id, stream_id) {
  }

};

}  // namespace orbit
