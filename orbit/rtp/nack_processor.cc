/*
 * nack_processer.cc
 *
 *  Created on: Apr 7, 2016
 *      Author: chenteng
 */

#include <glib.h>
#include <iostream>
#include <boost/scoped_ptr.hpp>
#include "gflags/gflags.h"

#include "rtp_headers.h"
#include "nack_processor.h"
#include "janus_rtcp_processor.h"
#include "rtcp_processor.h"
#include "stream_service/orbit/transport_delegate.h"
#include "stream_service/orbit/base/timeutil.h"
#include "stream_service/orbit/base/singleton.h"
#include "stream_service/orbit/base/session_info.h"

DEFINE_bool(enable_nack_request_fir, 
            false, 
            "enable send FIR to request intra-frame.");

#define KEY_FRAME_INTERVAL 3000      // in milliseconds
#define MAX_RETRANSMIT_QUEUE_SIZE 300
#define RETRANSMIT_PACKETS_LIMIT 100


namespace orbit {

using namespace std;

NackProcessor::NackProcessor() {
	last_seqs_video_ = NULL;
	trans_delegate_ = NULL;
}

NackProcessor::NackProcessor(TransportDelegate *trans_delegate,
    RtcpProcessor *rtcp_processor) {
	last_seqs_video_ = NULL;
	trans_delegate_ = trans_delegate;
	rtcp_processor_ = rtcp_processor;
}

NackProcessor::~NackProcessor() {
  janus_seq_list_free(&last_seqs_video_);
  retransmit_list_.clear();
}

unsigned int NackProcessor::GetRetransmitListSize() {
  boost::mutex::scoped_lock lock(retransmit_mutex_);
  return retransmit_list_.size();
}

// Push the send packet to retransmit_list queue
// Will remove old retransmit packets to keep size of retransmit list
void NackProcessor::PushPacket(char* buf, int len) {
	RtpHeader* rtp = reinterpret_cast<RtpHeader*>(buf);
	char* rbuf = reinterpret_cast<char*>(malloc(len));
	memcpy(rbuf, buf, len);
  auto rp = std::make_shared<RtpRetransmitPacket>();
	rp->seqn = rtp->getSeqNumber();
	rp->buf = (unsigned char*) rbuf;
	rp->buf_size = len;
	rp->retries = 0;
	rp->ts = getTimeMS();
	{
    boost::mutex::scoped_lock lock(retransmit_mutex_);
    retransmit_list_.push_back(rp);
    int list_size = retransmit_list_.size();
    while (list_size > MAX_RETRANSMIT_QUEUE_SIZE) {
      retransmit_list_.pop_front();
      list_size--;
    }
	}
}

int NackProcessor::GetRtcpNackPackets(char* buf, int buflen) {
  GSList *nacks = janus_rtcp_get_nacks(buf, buflen);
  guint nacks_count = g_slist_length(nacks);
  if (nacks_count) {
    g_slist_free(nacks);
  }
  return nacks_count;
}

/*
 * The function is used to parse the incoming NACK packet(the received NACK 
 * packet is sent by client). To verify whether there are some packets lost ? 
 * If there are some packets lost and be found in local retransmit_list, the 
 * lost packets will be retransmit by transport delegate.  
 */
int NackProcessor::ProcessRtcpNackPackets(char* buf, int buflen) {
  GSList *nacks = janus_rtcp_get_nacks(buf, buflen);
  guint nacks_count = g_slist_length(nacks);
  int lost_not_retrans = 0;
  if(nacks_count) {
    VLOG(3) << "Just got some NACKS ( " << nacks_count << ") we should handle...";

    GSList *nack_list = nacks;
    long now = getTimeMS();
    // NOTE: make it as an constant varible.
    long retransmit_wait_time = 30;

    boost::mutex::scoped_lock lock(retransmit_mutex_);
    while (nack_list) {
      unsigned int seqn = GPOINTER_TO_UINT(nack_list->data);
      std::list<RtpRetransmitPacketPtr>::reverse_iterator it;
      bool find_in_retransmit = false;
      for (it = retransmit_list_.rbegin(); it != retransmit_list_.rend(); ++it) {
        RtpRetransmitPacketPtr rp = *it;
        if (seqn == rp->seqn) {
          find_in_retransmit = true;
          // NOTE: make it as constant and check the right setting. 3 or 5?
          if (rp->retries < 5) { // allow to retransmit 5 times
            if ( rp->retries == 0 || (now - rp->ts) > retransmit_wait_time) {
              /* Statistical number of packet loss, excluding retransmission */
              if (rp->retries == 0) {
                lost_not_retrans++;
              }
              rp->ts = getTimeMS();
              rp->retries += 1;
              /* Enqueue it */
              dataPacket packet;
              packet.comp = 0;
              packet.length = rp->buf_size;
              packet.type = RETRANSMIT_PACKET;
              memcpy(&(packet.data[0]),rp->buf,rp->buf_size);
              trans_delegate_->RelayPacket(packet);

              VLOG(3) << "retransmit. seq: " << seqn << ", cur tries is " 
                      << rp->retries;
              break;
            } else {
              VLOG(3) << "ignore retransmit. seq: " << seqn
                      << ", tries too frequent. cur tries is " << rp->retries;
              break;
            }
          } else {
            VLOG(3) << "ignore retransmit. seq: " << seqn 
                    << ", to many tries than " << rp->retries;
            break;
          }
        }
      } // for
      if (!find_in_retransmit) {
        VLOG(3) << "Not in retransmit_list(" << seqn << ")";
      }
      nack_list = nack_list->next;
    } // while(nack_list)
    g_slist_free(nacks);
  } // if(nacks_count)

  return lost_not_retrans;
}

/*
 * Keep track the sequence numbers of received packets, and to verify whether
 * some packets are lost ? if true, we'll construct an NACK type RTCP message 
 * to request retransmit the lost packets.
 */
int NackProcessor::SendNackByCondition(uint16_t seqnumber, packetType media_type) {
  // The nackbuf is the RTCP NACK packet to send it back to sender.
  char nackbuf[120];
  int res = KeepTrackRtpSequenceNumber(seqnumber, nackbuf);
  if (res > 0) {
    rtcp_processor_->SendRTCP(RTCP_RTP_Feedback_PT, media_type, nackbuf, res);
  }
  return res;
}

/*
 * In this function we'll keep track the sequence number of received packets,
 * and to verify whether some packets are lost ? if true, we'll construct an NACK
 * type RTCP message.
 */
int NackProcessor::KeepTrackRtpSequenceNumber(/* IN */uint16_t new_seqn,
                                              /* OUT */char *nackbuf) {
  int res = 0;
  if (seqn_mtx_.try_lock()) {
    seq_info_t **last_seqs = &last_seqs_video_;
    seq_info_t *cur_seq = *last_seqs;
    guint16 cur_seqn;
    int last_seqs_len = 0;
    if (cur_seq) {
      cur_seq = cur_seq->prev;
      cur_seqn = cur_seq->seq;  //seq number
    } else {
      /* First seq, set up to add one seq */
      cur_seqn = new_seqn - (guint16) 1; /* Can wrap */
    }

    if (!janus_seq_in_range(new_seqn, cur_seqn, LAST_SEQS_MAX_LEN)
        && !janus_seq_in_range(cur_seqn, new_seqn, 1000)) {
      /* Jump too big, start fresh */
      LOG(INFO) << "Big sequence number jump ,cur_seqn=" << cur_seqn
      		<< ",new_seqn=" << new_seqn;
      janus_seq_list_free(last_seqs);
      cur_seq = NULL;
      cur_seqn = new_seqn - (guint16) 1;
    }
    
    GSList *nacks = NULL;
    gint64 now = janus_get_monotonic_time();
    
    if (janus_seq_in_range(new_seqn, cur_seqn, LAST_SEQS_MAX_LEN)) {
      /* Add new seq objs forward */
      while (cur_seqn != new_seqn) {
      	cur_seqn += (guint16) 1; /* can wrap */
      	seq_info_t *seq_obj = (seq_info_t *) g_malloc0(sizeof(seq_info_t));
      	seq_obj->seq = cur_seqn;
      	seq_obj->ts = now;
        seq_obj->times = 0;
      	seq_obj->state = (cur_seqn == new_seqn) ? SEQ_RECVED : SEQ_MISSING;
      	janus_seq_append(last_seqs, seq_obj);
      	last_seqs_len++;
      	seq_obj = NULL;
      }
    }
    
    if (cur_seq) {
      /* Scan old seq objs backwards */
      for (;;) {
        last_seqs_len++;
        if (cur_seq->seq == new_seqn) {
          VLOG(3) << "Recieved missed sequence number" << cur_seq->seq;
          cur_seq->state = SEQ_RECVED;
        } else if (cur_seq->state
             == SEQ_MISSING && now - cur_seq->ts > SEQ_MISSING_WAIT) {
          VLOG(3) << " Missed sequence number sending 1st NACK: "
                  << cur_seq->seq;
          nacks = g_slist_append(nacks, GUINT_TO_POINTER(cur_seq->seq));
          cur_seq->state = SEQ_NACKED;
          cur_seq->ts = now;
          cur_seq->times++;
        }else if (cur_seq->state == SEQ_NACKED &&
                  now - cur_seq->ts > SEQ_NACKED_WAIT &&
                  cur_seq->times <= MAX_TIMES_OF_RESEND) {
          nacks = g_slist_append(nacks, GUINT_TO_POINTER(cur_seq->seq));
          cur_seq->ts = now;
          cur_seq->times++;
          if (cur_seq->times > MAX_TIMES_OF_RESEND) {
            cur_seq->state = SEQ_GIVEUP;
          }
          VLOG(3) << " Missed sequence number sending next NACK "
                  << cur_seq->seq;
        }
        if (cur_seq == *last_seqs) {
          /* Just processed head */
          break;
        }
        cur_seq = cur_seq->prev;
      }
    } // end if (cur_seq)
    
    // delete cell from the head of list if it's length is larger than 
    // LAST_SEQS_MAX_LEN
    while (last_seqs_len > LAST_SEQS_MAX_LEN) {
      seq_info_t *node = janus_seq_pop_head(last_seqs);
      g_free(node);
      last_seqs_len--;
    }

    guint nacks_count = g_slist_length(nacks);
    long long current_time = getTimeMS();
    if (nacks_count > RETRANSMIT_PACKETS_LIMIT && 
        current_time - last_fir_request_ >= KEY_FRAME_INTERVAL) {
      // clear the retransmit list and send a key frame request
      janus_seq_list_free(&last_seqs_video_);
      if (FLAGS_enable_nack_request_fir) {
        SendFirPacket();
        last_fir_request_ = current_time;
        get_fir_ = false;
      }
    } else {
      int nackbuf_size = 0;
      RtcpNackCreate(nacks, nackbuf, &nackbuf_size);
      res = nackbuf_size;
    }

    seqn_mtx_.unlock();
    g_slist_free(nacks);
    nacks = NULL;
  } // end if (seqn_mtx_.try_lock())

  return res;
}

void NackProcessor::RtcpNackCreate(GSList *nacks, char* nackbuf, 
                                   int* nackbuf_size) {
  *nackbuf_size = 0;
  guint nacks_count = g_slist_length(nacks);
  if (nacks_count) {
    /* Generate a NACK and send it */
    VLOG(3) << "now sending NACK msg for " << nacks_count
              << "missed packets";
    nacks = g_slist_reverse(nacks);
    int res = janus_rtcp_nacks(nackbuf, 120, nacks);
    *nackbuf_size = res;
  }
}

void NackProcessor::SendFirPacket() {
  char buf[20];
  memset(buf, 0, 20);
  int fir_tmp = fir_seq_;
  int len = janus_rtcp_fir((char *)&buf, 20, &fir_tmp);
  fir_seq_ = fir_tmp;

  dataPacket p;
  p.comp = 0;
  p.type = VIDEO_PACKET;
  p.length = len;
  memcpy(&(p.data[0]), buf, len);
  trans_delegate_->RelayPacket(p);
  LOG(WARNING) << "NackProcessor Send RTCP...FIR....";

  /* Send a PLI too, just in case... */
  memset(buf, 0, 12);
  len = janus_rtcp_pli((char *)&buf, 12);
  p.length = len;
  memcpy(&(p.data[0]), buf, len);
  trans_delegate_->RelayPacket(p);
  LOG(WARNING) << "NackProcessor Send RTCP...PLI....";
}

void NackProcessor::SendRtcpNackList(const std::vector<uint16_t>& nack_list, 
                                     packetType packet_type) {
  char nackbuf[120];  
  int res;
  GSList *nacks = NULL;
  for (auto nack_it : nack_list) {
    nacks = g_slist_append(nacks, GUINT_TO_POINTER(nack_it));
  }
  RtcpNackCreate(nacks, nackbuf, &res);

  g_slist_free(nacks);
  nacks = NULL;

  /* Enqueue it */
  dataPacket packet;
  packet.comp = 0;
  packet.length = res;
  packet.type = packet_type;
  memcpy(&(packet.data[0]),nackbuf,res);
  LOG(INFO) << "Send a nack...";

  trans_delegate_->RelayPacket(packet);
}

} /* namespace orbit */
