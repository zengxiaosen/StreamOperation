/*
 * Copyright 2016 (C) Orange Labs Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * rtp_sender.cc
 * ---------------------------------------------------------------------------
 * Implement a RTP sender module.
 * The class is refactored from transport_delegate.h
 * ---------------------------------------------------------------------------
 */
#include "rtp_sender.h"
#include "stream_service/orbit/base/timeutil.h"
#include "stream_service/orbit/transport_delegate.h"
#include "rtp/rtp_headers.h"
#include <sys/prctl.h>

#define SEND_QUEUE_SLEEP_TIME 1000 // in us, i.e. 1000us = 1ms

namespace orbit {
  RtpSender::RtpSender(TransportDelegate* transport_delegate) {
    transport_delegate_ = transport_delegate;

    running_ = true;
    sender_thread_.reset(
      new boost::thread(boost::bind(&RtpSender::SendThreadLoop, this)));
  }

  RtpSender::~RtpSender() {
    running_ = false;
    if (sender_thread_ != NULL) {
      sender_thread_->join();
    }
    // clear send_pq_
    {
      boost::mutex::scoped_lock lock(send_pq_mutex_);
      int queue_size = send_pq_.size();
      if (queue_size > 0) {
        LOG(INFO) << "RtpSender::~RtpSender free the send_pq_ which size = " << queue_size;
      }
      while(queue_size > 0) {
        RtpSendPacket p = send_pq_.top();
        if (p.buf) {
          free(p.buf);
          p.buf = NULL;
        }
        send_pq_.pop();
        queue_size --;
      }
    }// end clear send_pq_
  }

  void RtpSender::WriteAndSend(Transport* transport, int priority,
                               unsigned char* data, int len) {
    unsigned char* buf = reinterpret_cast<unsigned char*>(malloc(len));
    memcpy(buf, data, len);
    RtpSendPacket packet;
    packet.priority = priority;
    packet.transport = transport;
    /* Get the sequence number of the RTP packet (buf) */
    const RtpHeader* h = reinterpret_cast<const RtpHeader*>(buf);
    packet.seqn = h->getSeqNumber();

    // Get the inqueue current timestamp.
    packet.queue_ts = getTimeMS();
    packet.buf = buf;
    packet.buf_size = len;
    boost::mutex::scoped_lock lock(send_pq_mutex_);
    send_pq_.push(packet);  
  }

  void RtpSender::SendThreadLoop() {
    /* Set thread name */
    prctl(PR_SET_NAME, (unsigned long)"TransportSendLoop");

    auto mypq = &(send_pq_);
    while (running_) {
      bool transport_ready = (transport_delegate_->transport_state_ == TRANSPORT_READY);
      if (!transport_ready || mypq->empty()) {
        usleep(SEND_QUEUE_SLEEP_TIME);
        continue;
      }
      boost::mutex::scoped_lock lock(send_pq_mutex_);
      RtpSendPacket p = mypq->top();
      if (p.priority == 1) {
        const RtpHeader* h = reinterpret_cast<const RtpHeader*>(p.buf);
        VLOG(3) << "Write ---- p.buf_size=" << p.buf_size;
        VLOG(3) << " payload=" << (int)(h->getPayloadType())
                  << " ts=" <<  h->getTimestamp()  
                  << " seq=" << h->getSeqNumber()
                  << " h->getMarker()=" << (int)(h->getMarker())
                  << " headerLength=" << h->getHeaderLength();
        VLOG(3) << "pq.size=" << mypq->size();
        p.transport->write((char*)p.buf, p.buf_size);
        free(p.buf);
        mypq->pop();
        transport_delegate_->UpdateSenderBitrate(p.buf_size);
      } else {
        //....Send
        long cur = getTimeMS();
        const RtpHeader* h = reinterpret_cast<const RtpHeader*>(p.buf);
        VLOG(3) << "SendBUF******* ---- p.buf_size=" << p.buf_size;
        VLOG(3) << " payload=" << (int)(h->getPayloadType())
                  << " ts=" <<  h->getTimestamp()
                  << " seq=" << h->getSeqNumber();
        VLOG(3) << " inQueueTime=" << cur - p.queue_ts << " ms";

        p.transport->write((char*)p.buf, p.buf_size);
        free(p.buf);
        mypq->pop();
        transport_delegate_->UpdateSenderBitrate(p.buf_size);
        continue;
      }
    }
  }
} // namespace orbit
