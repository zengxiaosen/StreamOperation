/*
 * nack_processor.h
 *
 *  Created on: Apr 7, 2016
 *      Author: chenteng
 */

#pragma once

#include <atomic>
#include <glib.h>
#include <boost/scoped_ptr.hpp>
#include "glog/logging.h"
#include "stream_service/orbit/rtp/janus_rtcp_processor.h"
#include "stream_service/orbit/media_definitions.h"

namespace orbit {

class RtcpProcessor;
class TransportDelegate;

struct RtpRetransmitPacket {
  uint16_t seqn;
  unsigned char* buf;
  int buf_size;
  long ts;
  int retries;
  RtpRetransmitPacket() {
    buf = NULL;
  }
  RtpRetransmitPacket(unsigned char* buf_, int buf_size_) {
    buf = buf_;
    buf_size = buf_size_;
  }
  ~RtpRetransmitPacket() {
    if (buf != NULL) {
      free(buf);
      buf = NULL;
    }  
  }
};
typedef std::shared_ptr<RtpRetransmitPacket> RtpRetransmitPacketPtr;

// Keep track seqnumber to generate nack message (chenteng)
// Deal with nack message (chenteng)
class NackProcessor {
 public:
  NackProcessor();

  NackProcessor(TransportDelegate *trans_delegate, RtcpProcessor *rtcp_processor);

  virtual ~NackProcessor();

  /*
   * Determine the input RTCP packet (buf) whether an NACK message ?
   * If it's an NACK message, the return value is the count of packet lost. 
   * Otherwise the return value is 0.
   *
   * @return the count of packet lost (> 0) , otherwise return 0
   */
  int GetRtcpNackPackets(char* buf, int buflen);

  /*
   * Parse received NACK message and retransmit the lost packets to client.
   *
   * @param[in] buf : received NACK packet
   * @param[in] buflen : the length of received NACK packet
   *
   * @return the number of lost packets that not include duplicate packets.  
   */
  int ProcessRtcpNackPackets(char* buf, int buflen);

  /*
   * Keep track the sequence number of the input packets to determine if any 
   * packets are missing ? If there are packet lost, the function will generate
   * an NACK message and send it to remote.
   *
   * @param[in] seqnumber : the sequence number of incoming packet 
   * @param[in] media_type: rtp packet type(VIDEO_TYPE or AUDIO_TYPE)
   * @return NACK packet size (if > 0), or 0 for Not NACK message create and send.
   */
  int SendNackByCondition(uint16_t seqnumber, packetType media_type);

  void PushPacket(char* buf, int len);

  unsigned int GetRetransmitListSize();

  void SendRtcpNackList(const std::vector<uint16_t>& nack_list, 
                        packetType packet_type);

  void SendFirPacket();
 
  inline void set_last_fir_request(long long now) { last_fir_request_ = now; } 
  
  inline long long last_fir_request() const { return last_fir_request_; }

  inline void set_get_fir() { get_fir_ = true; }
  
  inline long long get_fir() { return get_fir_; }

private:
  /*
   * Method to keep track the incoming rtp packet's sequence number to find lost
   * packets and generate NACK message by condition.
   *
   * @param[in] new_seqn : the sequence number of packet received.
   * @param[out] nackbuf : NACK message.
   *
   * @return The Nack buffer's length (ie. the size of nackbuf)
   */
  int KeepTrackRtpSequenceNumber(/* IN */ uint16_t new_seqn,
                                 /* OUT */ char *nackbuf);
  /*
   * To create an NACK rtcp message according to the lost packets.
   *
   * @param[in] nacks : the lost packets list
   * @param[out] nackbuf : created NACK packet
   * @param[out] nackbuf_size : the length of NACK packet.
   */
  void RtcpNackCreate(/* IN */GSList *nacks, 
                      /* OUT */char* nackbuf, int* nackbuf_size);

  TransportDelegate *trans_delegate_;
  RtcpProcessor *rtcp_processor_;

  // retransmit container
  std::list<RtpRetransmitPacketPtr> retransmit_list_;
  boost::mutex retransmit_mutex_;

  // seqnumber container
  seq_info_t* last_seqs_video_;
  std::mutex seqn_mtx_;

  /* key frame request */
  int fir_seq_{0};
    // Indicate sent a FIR/PLI message or not, the variable will be set to false
    // if we sent a key frame request and still not receive any one.
  bool get_fir_{1}; 
  long long last_fir_request_{0};
};

} /* namespace orbit */
