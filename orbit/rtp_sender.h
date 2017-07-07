/*
 * Copyright 2016 (C) Orange Labs Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * rtp_sender.h
 * ---------------------------------------------------------------------------
 * Defines a class to implement a module to send the RTP packets.
 * The class is refactored from transport_delegate.h
 * ---------------------------------------------------------------------------
 */

#pragma once
#include <boost/thread/mutex.hpp>
#include <boost/scoped_ptr.hpp>

#include "transport.h"
namespace orbit {
// Forward declartion
class TransportDelegate;

  enum SendPacketPriority {
    RETRANSMIT_PRIORITY = 5,
    VIDEO_RTX_PRIORITY = 2,
    RTCP_PRIORITY = 5,
    AUDIO_PRIORITY = 6,
    REMB_PRIORITY = 8,
    DEFAULT_PRIORITY = 10, // VIDEO is also in this priority.
  };

 // A packet to store the outgoing packets.
 struct RtpSendPacket {
   int priority;
   int seqn;
   long queue_ts;
   Transport* transport;
   unsigned char* buf;
   int buf_size;
   RtpSendPacket() {
   }
 };

 // A functor use to packets' ordering and sorting in sender queue.
class RtpSendPacketComparator {
public:
  bool operator() (RtpSendPacket a, RtpSendPacket b)
  {
    if (a.priority > b.priority) {
      return true;
    } else {
      if (a.seqn > b.seqn) {
        return true;
      }
      return false;
    }
  }
};

class RtpSender {
 public:
  RtpSender(TransportDelegate *transport_delegate);
  ~RtpSender();
 private:
  // The transport_delegate maintains a RTP/RTCP sender in the class
  void WriteAndSend(Transport* transport, int priority, unsigned char* data, int len);
  // A thread maintained by this class to send the packets to other endpoint.
  void SendThreadLoop();

  // The reference to the tranport_delegate_
  TransportDelegate *transport_delegate_;
  // mutex to lock the sender queue.
  boost::mutex send_pq_mutex_;
  // seqn hash_set - the sender queue ordered by priority and sequence number.
  std::priority_queue<RtpSendPacket, std::vector<RtpSendPacket>, RtpSendPacketComparator> send_pq_;

  // the thread to 
  boost::scoped_ptr<boost::thread> sender_thread_;
  // The flag of the running status of the class/thread.
  bool running_ = false;

  friend class TransportDelegate;
};

}  // namespace orbit
