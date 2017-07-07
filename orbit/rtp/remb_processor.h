/*
 * remb_processor.h
 *
 *  Created on: May 30, 2016
 *      Author: chenteng
 */

#pragma once
#include <mutex>

namespace orbit {

class TransportDelegate;

class RembProcessor {
public:
  RembProcessor(TransportDelegate *trans_delegate);
  ~RembProcessor();

  void SetBitrate(uint64_t bitrate);
  void SlowLinkUpdate(unsigned int nacks, int video, int uplink, uint64_t now);
  void SendRembPacket(uint64_t bitrate);
private:
  void VideoCallSlowLink(int uplink, int video);

  TransportDelegate *trans_delegate_ = NULL;

  uint64_t bitrate_;
  uint64_t peer_bitrate_;
  unsigned int nack_period_ts_;
  unsigned int last_slowlink_time_;
  unsigned int nack_recent_cnt_;   // the number of packet lost.

};

} /* namespace orbit */
