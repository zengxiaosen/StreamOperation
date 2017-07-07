/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * janus_rtcp_processor.cc
 * ---------------------------------------------------------------------------
 * Implements the Rtcp processor to process all the Rtcp message.
 * Using Janus code.
 * ---------------------------------------------------------------------------
 */

#include "janus_rtcp_processor.h"
#include "glog/logging.h"
#include "stream_service/orbit/logger_helper.h"
#include "stream_service/orbit/rtp/janus_rtp_defines.h"
using namespace std;

namespace orbit {

  /* Maximum values for the NACK queue/retransmissions */
  #define DEFAULT_MAX_NACK_QUEUE  300
  /* Maximum ignore count after retransmission (100ms) */
  #define MAX_NACK_IGNORE     100000

  static uint max_nack_queue = DEFAULT_MAX_NACK_QUEUE;
  void janus_set_max_nack_queue(uint mnq) {
    max_nack_queue = mnq;
    VLOG(3) <<"Setting max NACK queue to " << max_nack_queue;
  }

  uint janus_get_max_nack_queue(void) {
    return max_nack_queue;
  }

  namespace {
    /*! \brief Method to quickly retrieve the sender SSRC (needed for demuxing RTCP in BUNDLE)
     * @param[in] packet The message data
     * @param[in] len The message data length in bytes
     * @returns The sender SSRC, or 0 in case of error */
    uint32_t janus_rtcp_get_sender_ssrc(char *packet, int len);
    /*! \brief Method to quickly retrieve the received SSRC (needed for demuxing RTCP in BUNDLE)
     * @param[in] packet The message data
     * @param[in] len The message data length in bytes
     * @returns The receiver SSRC, or 0 in case of error */
    uint32_t janus_rtcp_get_receiver_ssrc(char *packet, int len);

  }

  int janus_rtcp_parse(rtcp_context *ctx, char *packet, int len) {
    return janus_rtcp_fix_ssrc(ctx, packet, len, 0, 0, 0);
  }

  /* Helper to handle an incoming SR: triggered by a call to janus_rtcp_fix_ssrc with fixssrc=0 */
  static void janus_rtcp_incoming_sr(rtcp_context *ctx, rtcp_sr *sr) {
    if(ctx == NULL)
      return;
    /* Update the context with info on the monotonic time of last SR received */
    ctx->lsr_ts = janus_get_monotonic_time();
    /* Compute the last SR received as well */
    uint64_t ntp = ntohl(sr->si.ntp_ts_msw);
    ntp = (ntp << 32) | ntohl(sr->si.ntp_ts_lsw);
    ctx->lsr = (ntp >> 16);
  }

  uint32_t janus_rtcp_get_fbnack_media_ssrc(char *packet, int len) {
    if(packet == NULL || len <= 0)
      return -1;
    rtcp_header *rtcp = (rtcp_header *)packet;
    if(rtcp->version != 2)
      return -2;
    int total = len;
    while(rtcp) {
		  if(rtcp->type == RTCP_RTPFB) {
			  gint fmt = rtcp->rc;
			  if(fmt == 1) {
				  rtcp_fb *rtcpfb = (rtcp_fb *)rtcp;
          uint32_t media_data = ntohl(rtcpfb->media);
          return media_data;
        }
      } // end of if

      /* Is this a compound packet? */
      int length = ntohs(rtcp->length);
      if(length == 0) {
        break;
      }
      total -= length*4+4;
      if(total <= 0) {
        break;
      }
      rtcp = (rtcp_header *)((uint32_t*)rtcp + length + 1);
    } // end of while(rtcp)
    return 0;
  }

  int janus_rtcp_fix_ssrc(rtcp_context *ctx, char *packet, int len, int fixssrc, uint32_t newssrcl, uint32_t newssrcr) {
    if(packet == NULL || len <= 0)
      return -1;
    rtcp_header *rtcp = (rtcp_header *)packet;
    if(rtcp->version != 2)
      return -2;
    int pno = 0, total = len;
    ELOG_DEBUG( "   Parsing compound packet (total of %d bytes)\n", total);
    while(rtcp) {
      pno++;
      /* TODO Should we handle any of these packets ourselves, or just relay them? */
      switch(rtcp->type) {
        case RTCP_SR: {
          /* SR, sender report */
          ELOG_DEBUG( "     #%d SR (200)\n", pno);
          rtcp_sr *sr = (rtcp_sr*)rtcp;
          if(ctx != NULL) {
            /* RTCP context provided, update it with info on this SR */
            janus_rtcp_incoming_sr(ctx, sr);
          }
          //~ ELOG_DEBUG( "       -- SSRC: %u (%u in RB)\n", ntohl(sr->ssrc), report_block_get_ssrc(&sr->rb[0]));
          //~ ELOG_DEBUG( "       -- Lost: %u/%u\n", report_block_get_fraction_lost(&sr->rb[0]), report_block_get_cum_packet_loss(&sr->rb[0]));
          if(fixssrc && newssrcl) {
            sr->ssrc = htonl(newssrcl);
          }
          if(fixssrc && newssrcr && sr->header.rc > 0) {
            sr->rb[0].ssrc = htonl(newssrcr);
          }
          break;
        }
        case RTCP_RR: {
          /* RR, receiver report */
          ELOG_DEBUG( "     #%d RR (201)\n", pno);
          rtcp_rr *rr = (rtcp_rr*)rtcp;
          //~ ELOG_DEBUG( "       -- SSRC: %u (%u in RB)\n", ntohl(rr->ssrc), report_block_get_ssrc(&rr->rb[0]));
          //~ ELOG_DEBUG( "       -- Lost: %u/%u\n", report_block_get_fraction_lost(&rr->rb[0]), report_block_get_cum_packet_loss(&rr->rb[0]));
          if(fixssrc && newssrcl) {
            rr->ssrc = htonl(newssrcl);
          }
          if(fixssrc && newssrcr && rr->header.rc > 0) {
            rr->rb[0].ssrc = htonl(newssrcr);
          }
          break;
        }
        case RTCP_SDES: {
          /* SDES, source description */
          ELOG_DEBUG( "     #%d SDES (202)\n", pno);
          rtcp_sdes *sdes = (rtcp_sdes*)rtcp;
          //~ ELOG_DEBUG( "       -- SSRC: %u\n", ntohl(sdes->chunk.ssrc));
          if(fixssrc && newssrcl) {
            sdes->chunk.ssrc = htonl(newssrcl);
          }
          break;
        }
        case RTCP_BYE: {
          /* BYE, goodbye */
          ELOG_DEBUG( "     #%d BYE (203)\n", pno);
          rtcp_bye_t *bye = (rtcp_bye_t*)rtcp;
          //~ ELOG_DEBUG( "       -- SSRC: %u\n", ntohl(bye->ssrc[0]));
          if(fixssrc && newssrcl) {
            bye->ssrc[0] = htonl(newssrcl);
          }
          break;
        }
        case RTCP_APP: {
          /* APP, application-defined */
          ELOG_DEBUG( "     #%d APP (204)\n", pno);
          rtcp_app_t *app = (rtcp_app_t*)rtcp;
          //~ ELOG_DEBUG( "       -- SSRC: %u\n", ntohl(app->ssrc));
          if(fixssrc && newssrcl) {
            app->ssrc = htonl(newssrcl);
          }
          break;
        }
        case RTCP_FIR: {
          /* FIR, rfc2032 */
          ELOG_DEBUG( "     #%d FIR (192)\n", pno);
          rtcp_fb *rtcpfb = (rtcp_fb *)rtcp;
          if(fixssrc && newssrcr && (ntohs(rtcp->length) >= 20)) {
            rtcpfb->media = htonl(newssrcr);
          }
          if(fixssrc && newssrcr) {
            uint32_t *ssrc = (uint32_t *)rtcpfb->fci;
            *ssrc = htonl(newssrcr);
          }
          break;
        }
        case RTCP_RTPFB: {
          /* RTPFB, Transport layer FB message (rfc4585) */
          //~ ELOG_DEBUG( "     #%d RTPFB (205)\n", pno);
          gint fmt = rtcp->rc;
          //~ ELOG_DEBUG( "       -- FMT: %u\n", fmt);
          rtcp_fb *rtcpfb = (rtcp_fb *)rtcp;
          //~ ELOG_DEBUG( "       -- SSRC: %u\n", ntohl(rtcpfb->ssrc));
          if(fmt == 1) {
            ELOG_DEBUG( "     #%d NACK -- RTPFB (205)\n", pno);
            if(fixssrc && newssrcr) {
              rtcpfb->media = htonl(newssrcr);
            }
            int nacks = ntohs(rtcp->length)-2;  /* Skip SSRCs */
            if(nacks > 0) {
              ELOG_DEBUG("        Got %d nacks\n", nacks);
              rtcp_nack *nack = NULL;
              uint16_t pid = 0;
              uint16_t blp = 0;
              int i=0, j=0;
              char bitmask[20];
              for(i=0; i< nacks; i++) {
                nack = (rtcp_nack *)rtcpfb->fci + i;
                pid = ntohs(nack->pid);
                blp = ntohs(nack->blp);
                memset(bitmask, 0, 20);
                for(j=0; j<16; j++) {
                  bitmask[j] = (blp & ( 1 << j )) >> j ? '1' : '0';
                }
                bitmask[16] = '\n';
                ELOG_DEBUG( "[%d] %"SCNu16" / %s\n", i, pid, bitmask);
              }
            }
          } else if(fmt == 3) { /* rfc5104 */
            /* TMMBR: http://tools.ietf.org/html/rfc5104#section-4.2.1.1 */
            ELOG_DEBUG( "     #%d TMMBR -- RTPFB (205)\n", pno);
            if(fixssrc && newssrcr) {
              uint32_t *ssrc = (uint32_t *)rtcpfb->fci;
              *ssrc = htonl(newssrcr);
            }
          } else {
            ELOG_DEBUG( "     #%d ??? -- RTPFB (205, fmt=%d)\n", pno, fmt);
          }
          if(fixssrc && newssrcl) {
            rtcpfb->ssrc = htonl(newssrcl);
          }
          break;
        }
        case RTCP_PSFB: {
          /* PSFB, Payload-specific FB message (rfc4585) */
          //~ ELOG_DEBUG( "     #%d PSFB (206)\n", pno);
          gint fmt = rtcp->rc;
          //~ ELOG_DEBUG( "       -- FMT: %u\n", fmt);
          rtcp_fb *rtcpfb = (rtcp_fb *)rtcp;
          //~ ELOG_DEBUG( "       -- SSRC: %u\n", ntohl(rtcpfb->ssrc));
          if(fmt == 1) {
            ELOG_DEBUG("     #%d PLI -- PSFB (206)\n", pno);
            if(fixssrc && newssrcr) {
              rtcpfb->media = htonl(newssrcr);
            }
          } else if(fmt == 2) {
            ELOG_DEBUG( "     #%d SLI -- PSFB (206)\n", pno);
          } else if(fmt == 3) {
            ELOG_DEBUG( "     #%d RPSI -- PSFB (206)\n", pno);
          } else if(fmt == 4) { /* rfc5104 */
            /* FIR: http://tools.ietf.org/html/rfc5104#section-4.3.1.1 */
            ELOG_DEBUG( "     #%d FIR -- PSFB (206)\n", pno);
            if(fixssrc && newssrcr) {
              rtcpfb->media = htonl(newssrcr);
            }
            if(fixssrc && newssrcr) {
              uint32_t *ssrc = (uint32_t *)rtcpfb->fci;
              *ssrc = htonl(newssrcr);
            }
          } else if(fmt == 5) { /* rfc5104 */
            /* FIR: http://tools.ietf.org/html/rfc5104#section-4.3.2.1 */
            ELOG_DEBUG( "     #%d PLI -- TSTR (206)\n", pno);
            if(fixssrc && newssrcr) {
              uint32_t *ssrc = (uint32_t *)rtcpfb->fci;
              *ssrc = htonl(newssrcr);
            }
          } else if(fmt == 15) {
            //~ ELOG_DEBUG( "       -- This is a AFB!\n");
            rtcp_fb *rtcpfb = (rtcp_fb *)rtcp;
            rtcp_remb *remb = (rtcp_remb *)rtcpfb->fci;
            if(remb->id[0] == 'R' && remb->id[1] == 'E' && remb->id[2] == 'M' && remb->id[3] == 'B') {
              ELOG_DEBUG( "     #%d REMB -- PSFB (206)\n", pno);
              if(fixssrc && newssrcr) {
                remb->ssrc[0] = htonl(newssrcr);
              }
              /* FIXME From rtcp_utility.cc */
              unsigned char *_ptrRTCPData = (unsigned char *)remb;
              _ptrRTCPData += 4;  // Skip unique identifier and num ssrc
              //~ ELOG_DEBUG( " %02X %02X %02X %02X\n", _ptrRTCPData[0], _ptrRTCPData[1], _ptrRTCPData[2], _ptrRTCPData[3]);
              uint8_t numssrc = (_ptrRTCPData[0]);
              uint8_t brExp = (_ptrRTCPData[1] >> 2) & 0x3F;
              uint32_t brMantissa = (_ptrRTCPData[1] & 0x03) << 16;
              brMantissa += (_ptrRTCPData[2] << 8);
              brMantissa += (_ptrRTCPData[3]);
              uint64_t bitRate = brMantissa << brExp;
              ELOG_DEBUG( "       -- -- -- REMB: %u * 2^%u = %"SCNu64" (%d SSRCs, %u)\n",
                brMantissa, brExp, bitRate, numssrc, ntohl(remb->ssrc[0]));
            } else {
              ELOG_DEBUG( "     #%d AFB ?? -- PSFB (206)\n", pno);
            }
          } else {
            ELOG_DEBUG( "     #%d ?? -- PSFB (206, fmt=%d)\n", pno, fmt);
          }
          if(fixssrc && newssrcl) {
            rtcpfb->ssrc = htonl(newssrcl);
          }
          break;
        }
        default:
          ELOG_DEBUG( "     Unknown RTCP PT %d\n", rtcp->type);
          break;
      }
      /* Is this a compound packet? */
      int length = ntohs(rtcp->length);
      ELOG_DEBUG("       RTCP PT length: %d bytes\n", length*4+4);
      if(length == 0) {
        //~ ELOG_DEBUG("  0-length, end of compound packet\n");
        break;
      }
      total -= length*4+4;
      //~ ELOG_DEBUG( "     Packet has length %d (%d bytes, %d remaining), moving to next one...\n", length, length*4+4, total);
      if(total <= 0) {
        ELOG_DEBUG("  End of compound packet\n");
        break;
      }
      rtcp = (rtcp_header *)((uint32_t*)rtcp + length + 1);
    }
    return 0;
  }

/*! \brief Method to fix an RTCP message (http://tools.ietf.org/html/draft-ietf-straw-b2bua-rtcp-00)
 * @param[in] ctx The rtcp context
 * @param[in] packet The message data
 * @param[in] len The message data length in bytes
 * @param[in] fixssrc Whether the method needs to fix the message or just parse it
 * @param[in] newssrcl The SSRC of the sender to put in the message
 * @param[in] newssrcr The SSRC of the receiver to put in the message
 * @returns 0 in case of success, -1 on errors */
int janus_rtcp_process_incoming_rtp(rtcp_context *ctx, char *packet, 
                                    int len, int max_nack_queue) {
  if(ctx == NULL || packet == NULL || len < 1)
    return -1;

  /* RTP packet received: it means we can start sending RR */
  ctx->enabled = 1;
  /* Parse this RTP packet header and update the rtcp_context instance */
  rtp_header *rtp = (rtp_header *)packet;
  uint16_t seq_number = ntohs(rtp->seq_number);
  if(ctx->base_seq == 0 && ctx->seq_cycle == 0) {
    ctx->base_seq = seq_number;
    LOG(INFO) <<"seq_number = " <<seq_number;
  }
  if(max_nack_queue + seq_number < ctx->last_seq_nr)
    ctx->seq_cycle++;
  ctx->last_seq_nr = seq_number;
  ctx->received++;
  uint32_t rtp_expected = 0x0;
  if(ctx->seq_cycle > 0) {
    rtp_expected = ctx->seq_cycle;
    rtp_expected = rtp_expected << 16;
  }
  rtp_expected = rtp_expected + 1 + seq_number - ctx->base_seq;
  if ((rtp_expected - ctx->received) > 0) {
    ctx->lost = rtp_expected - ctx->received;
  } else {
    ctx->lost = 0;
  }
  ctx->expected = rtp_expected;

  uint64_t arrival = (janus_get_monotonic_time() * ctx->tb) / 1000000;   // Hz
  uint64_t transit = arrival - ntohl(rtp->timestamp);
  uint64_t d = abs((int64_t)(transit - ctx->transit));
  if (ctx->transit == 0) {
    ctx->jitter = 0;
  } else {
    ctx->jitter += (1./16.) * ((double)d  - ctx->jitter);
  }
  ctx->transit = transit;
  return 0;
}

int janus_rtcp_has_fir(char *packet, int len) {
	gboolean got_fir = FALSE;
	/* Parse RTCP compound packet */
	rtcp_header *rtcp = (rtcp_header *)packet;
	if(rtcp->version != 2)
		return FALSE;
	int pno = 0, total = len;
	while(rtcp) {
		pno++;
		switch(rtcp->type) {
			case RTCP_FIR:
				got_fir = TRUE;
				break;
			default:
				break;
		}
		/* Is this a compound packet? */
		int length = ntohs(rtcp->length);
		if(length == 0)
			break;
		total -= length*4+4;
		if(total <= 0)
			break;
		rtcp = (rtcp_header *)((uint32_t*)rtcp + length + 1);
	}
	return got_fir ? TRUE : FALSE;
}

int janus_rtcp_has_pli(char *packet, int len) {
	gboolean got_pli = FALSE;
	/* Parse RTCP compound packet */
	rtcp_header *rtcp = (rtcp_header *)packet;
	if(rtcp->version != 2)
		return FALSE;
	int pno = 0, total = len;
	while(rtcp) {
		pno++;
		switch(rtcp->type) {
			case RTCP_PSFB: {
				gint fmt = rtcp->rc;
				if(fmt == 1)
					got_pli = TRUE;
				break;
			}
			default:
				break;
		}
		/* Is this a compound packet? */
		int length = ntohs(rtcp->length);
		if(length == 0)
			break;
		total -= length*4+4;
		if(total <= 0)
			break;
		rtcp = (rtcp_header *)((uint32_t*)rtcp + length + 1);
	}
	return got_pli ? TRUE : FALSE;
}

uint32_t janus_rtcp_context_get_lost(rtcp_context *ctx) {
  if(ctx == NULL)
    return 0;
  uint32_t lost;
  if(ctx->lost > 0x7FFFFF) {
    lost = 0x7FFFFF;
  } else {
    lost = ctx->lost;
  }
  return lost;
}

uint32_t janus_rtcp_context_get_lost_fraction(rtcp_context *ctx) {
  if(ctx == NULL)
    return 0;
  uint32_t expected_interval = ctx->expected - ctx->expected_prior;
  uint32_t received_interval = ctx->received - ctx->received_prior;
  int32_t lost_interval = expected_interval - received_interval;
  uint32_t fraction;
  if(expected_interval == 0 || lost_interval <=0)
    fraction = 0;
  else
    fraction = (lost_interval << 8) / expected_interval;
  return fraction << 24;
}

uint32_t janus_rtcp_context_get_lost_promille(rtcp_context *ctx) {
  if(ctx == NULL)
    return 0;
  uint32_t expected_interval = ctx->expected - ctx->expected_prior;
  uint32_t received_interval = ctx->received - ctx->received_prior;
  int32_t lost_interval = expected_interval - received_interval;
  uint32_t fraction;
  if(expected_interval == 0 || lost_interval <=0)
    fraction = 0;
  else
    fraction = (lost_interval << 8) / expected_interval;
  fraction = fraction << 24;
  return ((fraction>>24) * 1000) >> 8;
}

/* Generate a new SDES message */
int janus_rtcp_sdes(char *packet, int len, const char *cname, int cnamelen) {
	if(packet == NULL || len <= 0 || cname == NULL || cnamelen <= 0)
		return -1;
	memset(packet, 0, len);
	rtcp_header *rtcp = (rtcp_header *)packet;
	/* Set header */
	rtcp->version = 2;
	rtcp->type = RTCP_SDES;
	rtcp->rc = 1;
	int plen = 12;	/* Header + SSRC + CSRC in chunk */
	plen += cnamelen+2;
	if((cnamelen+2)%4)	/* Account for padding */
		plen += 4;
	if(len < plen) {
		ELOG_DEBUG( "Buffer too small for SDES message: %d < %d\n", len, plen);
		return -1;
	}
	rtcp->length = htons((plen/4)-1);
	/* Now set SDES stuff */
	rtcp_sdes *rtcpsdes = (rtcp_sdes *)rtcp;
	rtcpsdes->item.type = 1;
	rtcpsdes->item.len = cnamelen;
	memcpy(rtcpsdes->item.content, cname, cnamelen);
	return plen;
}

/* Generate a new REMB message */
int janus_rtcp_remb(char *packet, int len, uint64_t bitrate) {
	if(packet == NULL || len != 24)
		return -1;
	rtcp_header *rtcp = (rtcp_header *)packet;
	/* Set header */
	rtcp->version = 2;
	rtcp->type = RTCP_PSFB;
	rtcp->rc = 15;
	rtcp->length = htons((len/4)-1);
	/* Now set REMB stuff */
	rtcp_fb *rtcpfb = (rtcp_fb *)rtcp;
	rtcp_remb *remb = (rtcp_remb *)rtcpfb->fci;
	remb->id[0] = 'R';
	remb->id[1] = 'E';
	remb->id[2] = 'M';
	remb->id[3] = 'B';
	/* bitrate --> brexp/brmantissa */
	uint8_t b = 0;
	uint8_t newbrexp = 0;
	uint32_t newbrmantissa = 0;
	for(b=0; b<64; b++) {
		if(bitrate <= ((uint64_t) 0x3FFFF << b)) {
			newbrexp = b;
			break;
		}
	}
	newbrmantissa = bitrate >> b;
	/* FIXME From rtcp_sender.cc */
	unsigned char *_ptrRTCPData = (unsigned char *)remb;
	_ptrRTCPData += 4;	/* Skip unique identifier */
	_ptrRTCPData[0] = (uint8_t)(1);	/* Just one SSRC */
	_ptrRTCPData[1] = (uint8_t)((newbrexp << 2) + ((newbrmantissa >> 16) & 0x03));
	_ptrRTCPData[2] = (uint8_t)(newbrmantissa >> 8);
	_ptrRTCPData[3] = (uint8_t)(newbrmantissa);
	ELOG_DEBUG("[REMB] bitrate=%"SCNu64" (%d bytes)\n", bitrate, 4*(ntohs(rtcp->length)+1));
	return 24;
}

int janus_rtcp_report_block(rtcp_context *ctx, report_block *rb) {
  if(ctx == NULL || rb == NULL)
    return -1;
  rb->jitter = htonl((uint32_t) ctx->jitter);
  rb->ehsnr = htonl((((uint32_t) 0x0 + ctx->seq_cycle) << 16) + ctx->last_seq_nr);
  uint32_t lost = janus_rtcp_context_get_lost(ctx);
  uint32_t fraction = janus_rtcp_context_get_lost_fraction(ctx);
  ctx->expected_prior = ctx->expected;
  ctx->received_prior = ctx->received;
  rb->flcnpl = htonl(lost | fraction);
  rb->lsr = htonl(ctx->lsr);
  rb->delay = htonl(((janus_get_monotonic_time() - ctx->lsr_ts) << 16) / 1000000);
  ctx->last_sent = janus_get_monotonic_time();
  return 0;
}

/* Generate a new FIR message */
int janus_rtcp_fir(char *packet, int len, int *seqnr) {
	if(packet == NULL || len != 20 || seqnr == NULL)
		return -1;
	rtcp_header *rtcp = (rtcp_header *)packet;
	*seqnr = *seqnr + 1;
	if(*seqnr < 0 || *seqnr >= 256)
		*seqnr = 0;	/* Reset sequence number */
	/* Set header */
	rtcp->version = 2;
	rtcp->type = RTCP_PSFB;
	rtcp->rc = 4;	/* FMT=4 */
	rtcp->length = htons((len/4)-1);
	/* Now set FIR stuff */
	rtcp_fb *rtcpfb = (rtcp_fb *)rtcp;
	rtcp_fir *fir = (rtcp_fir *)rtcpfb->fci;
	fir->seqnr = htonl(*seqnr << 24);	/* FCI: Sequence number */
	ELOG_DEBUG("[FIR] seqnr=%d (%d bytes)\n", *seqnr, 4*(ntohs(rtcp->length)+1));
	return 20;
}

/* Generate a new legacy FIR message */
int janus_rtcp_fir_legacy(char *packet, int len, int *seqnr) {
	/* FIXME Right now, this is identical to the new FIR, with the difference that we use 192 as PT */
	if(packet == NULL || len != 20 || seqnr == NULL)
		return -1;
	rtcp_header *rtcp = (rtcp_header *)packet;
	*seqnr = *seqnr + 1;
	if(*seqnr < 0 || *seqnr >= 256)
		*seqnr = 0;	/* Reset sequence number */
	/* Set header */
	rtcp->version = 2;
	rtcp->type = RTCP_FIR;
	rtcp->rc = 4;	/* FMT=4 */
	rtcp->length = htons((len/4)-1);
	/* Now set FIR stuff */
	rtcp_fb *rtcpfb = (rtcp_fb *)rtcp;
	rtcp_fir *fir = (rtcp_fir *)rtcpfb->fci;
	fir->seqnr = htonl(*seqnr << 24);	/* FCI: Sequence number */
	ELOG_DEBUG("[FIR] seqnr=%d (%d bytes)\n", *seqnr, 4*(ntohs(rtcp->length)+1));
	return 20;
}

/* Generate a new PLI message */
int janus_rtcp_pli(char *packet, int len) {
	if(packet == NULL || len != 12)
		return -1;
	rtcp_header *rtcp = (rtcp_header *)packet;
	/* Set header */
	rtcp->version = 2;
	rtcp->type = RTCP_PSFB;
	rtcp->rc = 1;	/* FMT=1 */
	rtcp->length = htons((len/4)-1);
	return 12;
}

/* Generate a new BYE message */
int janus_rtcp_bye(char *packet, int len) {
  if(packet == NULL || len != 12)
    return -1;
  rtcp_header *rtcp = (rtcp_header *)packet;
  /* Set header */
  rtcp->version = 2;
  rtcp->type = RTCP_BYE;
  rtcp->rc = 1;	/* SC=1 */
  rtcp->length = htons((len/4)-1);
  return 12;
}

/* Ascending function for GSList */
gint sort_asc(gconstpointer p1, gconstpointer p2)
{
    gint32 a, b;
    a = GPOINTER_TO_INT (p1);
    b = GPOINTER_TO_INT (p2);

    return (a > b ? +1 : a == b ? 0 : -1);
}

/* Generate a new NACK message */
int janus_rtcp_nacks(char *packet, int len, GSList *nacks) {
	if(packet == NULL || len < 16 || nacks == NULL)
	{
		return -1;
	}
	nacks = g_slist_sort(nacks, sort_asc);
	memset(packet, 0, len);
	rtcp_header *rtcp = (rtcp_header *)packet;
	/* Set header */
	rtcp->version = 2;
	rtcp->type = RTCP_RTPFB;
	rtcp->rc = 1;	/* FMT=1 */
	/* Now set NACK stuff */
	rtcp_fb *rtcpfb = (rtcp_fb *)rtcp;
	rtcp_nack *nack = (rtcp_nack *)rtcpfb->fci;
	/* FIXME We assume the GSList list is already ordered... */
	guint16 pid = GPOINTER_TO_UINT(nacks->data);
	VLOG(2) <<"pid is " << pid;
	nack->pid = htons(pid);
	nacks = nacks->next;
	int words = 3;
	while(nacks) {
		guint16 npid = GPOINTER_TO_UINT(nacks->data);
		VLOG(2) <<"npid is " <<npid;
		if(npid-pid < 1) {
			ELOG_DEBUG("Skipping PID to NACK (%"SCNu16" already added)...\n", npid);
		} else if(npid-pid > 16) {
			/* We need a new block: this sequence number will be its root PID */
			ELOG_DEBUG("Adding another block of NACKs (%"SCNu16"-%"SCNu16" > %"SCNu16")...\n", npid, pid, npid-pid);
			words++;
			if(len < (words*4+4)) {
				ELOG_DEBUG( "Buffer too small: %d < %d (at least %d NACK blocks needed)\n", len, words*4+4, words);
				return -1;
			}
			char *new_block = packet + words*4;
			nack = (rtcp_nack *)new_block;
			pid = GPOINTER_TO_UINT(nacks->data);
			nack->pid = htons(pid);
		} else {
			uint16_t blp = ntohs(nack->blp);
			blp |= 1 << (npid-pid-1);
			nack->blp = htons(blp);
		}
		nacks = nacks->next;
	}
	rtcp->length = htons(words);
	return words*4+4;
}

/* Query an existing REMB message */
uint64_t janus_rtcp_get_remb(char *packet, int len) {
	if(packet == NULL || len == 0)
		return 0;
	rtcp_header *rtcp = (rtcp_header *)packet;
	if(rtcp->version != 2)
		return 0;
	/* Get REMB bitrate, if any */
	int total = len;
	while(rtcp) {
		if(rtcp->type == RTCP_PSFB) {
			gint fmt = rtcp->rc;
			if(fmt == 15) {
				rtcp_fb *rtcpfb = (rtcp_fb *)rtcp;
				rtcp_remb *remb = (rtcp_remb *)rtcpfb->fci;
				if(remb->id[0] == 'R' && remb->id[1] == 'E' && remb->id[2] == 'M' && remb->id[3] == 'B') {
					/* FIXME From rtcp_utility.cc */
					unsigned char *_ptrRTCPData = (unsigned char *)remb;
					_ptrRTCPData += 4;	/* Skip unique identifier and num ssrc */
					uint8_t brExp = (_ptrRTCPData[1] >> 2) & 0x3F;
					uint32_t brMantissa = (_ptrRTCPData[1] & 0x03) << 16;
					brMantissa += (_ptrRTCPData[2] << 8);
					brMantissa += (_ptrRTCPData[3]);
					uint64_t bitrate = brMantissa << brExp;
					return bitrate;
				}
			}
		}
		/* Is this a compound packet? */
		int length = ntohs(rtcp->length);
		if(length == 0)
			break;
		total -= length*4+4;
		if(total <= 0)
			break;
		rtcp = (rtcp_header *)((uint32_t*)rtcp + length + 1);
	}
	return 0;
}


GSList *janus_rtcp_get_nacks(char *packet, int len) {
	if(packet == NULL || len == 0)
		return NULL;
	rtcp_header *rtcp = (rtcp_header *)packet;
	if(rtcp->version != 2)
		return NULL;
	/* FIXME Get list of sequence numbers we should send again */
	GSList *list = NULL;
	int total = len;
	while(rtcp) {
		if(rtcp->type == RTCP_RTPFB) {
			gint fmt = rtcp->rc;
			if(fmt == 1) {
				rtcp_fb *rtcpfb = (rtcp_fb *)rtcp;
				int nacks = ntohs(rtcp->length)-2;	/* Skip SSRCs */
				if(nacks > 0) {
					//ELOG_DEBUG( "        Got %d nacks\n", nacks);
          ELOG_DEBUG("        Got %d nacks\n", nacks);
					rtcp_nack *nack = NULL;
					uint16_t pid = 0;
					uint16_t blp = 0;
					int i=0, j=0;
					char bitmask[20];
					for(i=0; i< nacks; i++) {
						nack = (rtcp_nack *)rtcpfb->fci + i;
						pid = ntohs(nack->pid);
						list = g_slist_append(list, GUINT_TO_POINTER(pid));
						blp = ntohs(nack->blp);
						memset(bitmask, 0, 20);
						for(j=0; j<16; j++) {
							bitmask[j] = (blp & ( 1 << j )) >> j ? '1' : '0';
							if((blp & ( 1 << j )) >> j)
								list = g_slist_append(list, GUINT_TO_POINTER(pid+j+1));
						}
						bitmask[16] = '\n';
						//ELOG_DEBUG( "[%d] %"SCNu16" / %s\n", i, pid, bitmask);
            ELOG_DEBUG("[%d] %"SCNu16" / %s\n", i, pid, bitmask);
					}
				}
			}
		}
		/* Is this a compound packet? */
		int length = ntohs(rtcp->length);
		if(length == 0)
			break;
		total -= length*4+4;
		if(total <= 0)
			break;
		rtcp = (rtcp_header *)((uint32_t*)rtcp + length + 1);
	}
	return list;
}

int janus_rtcp_remove_nacks(char *packet, int len) {
	if(packet == NULL || len == 0)
		return len;
	rtcp_header *rtcp = (rtcp_header *)packet;
	if(rtcp->version != 2)
		return len;
	/* Find the NACK message */
	char *nacks = NULL;
	int total = len, nacks_len = 0;
	while(rtcp) {
		if(rtcp->type == RTCP_RTPFB) {
			gint fmt = rtcp->rc;
			if(fmt == 1) {
				nacks = (char *)rtcp;
			}
		}
		/* Is this a compound packet? */
		int length = ntohs(rtcp->length);
		if(length == 0)
			break;
		if(nacks != NULL) {
			nacks_len = length*4+4;
			break;
		}
		total -= length*4+4;
		if(total <= 0)
			break;
		rtcp = (rtcp_header *)((uint32_t*)rtcp + length + 1);
	}
	if(nacks != NULL) {
		total = len - ((nacks-packet)+nacks_len);
		if(total < 0) {
			/* FIXME Should never happen, but you never know: do nothing */
			return len;
		} else if(total == 0) {
			/* NACK was the last compound packet, easy enough */
			return len-nacks_len;
		} else {
			/* NACK is between two compound packets, move them around */
			int i=0;
			for(i=0; i<total; i++)
				*(nacks+i) = *(nacks+nacks_len+i);
			return len-nacks_len;
		}
	}
	return len;
}

/* Change an existing REMB message */
int janus_rtcp_cap_remb(char *packet, int len, uint64_t bitrate) {
	if(packet == NULL || len == 0)
		return -1;
	rtcp_header *rtcp = (rtcp_header *)packet;
	if(rtcp->version != 2)
		return -2;
	if(bitrate == 0)
		return 0;	/* No need to cap */
	/* Cap REMB bitrate */
	int total = len;
	while(rtcp) {
		if(rtcp->type == RTCP_PSFB) {
			gint fmt = rtcp->rc;
			if(fmt == 15) {
				rtcp_fb *rtcpfb = (rtcp_fb *)rtcp;
				rtcp_remb *remb = (rtcp_remb *)rtcpfb->fci;
				if(remb->id[0] == 'R' && remb->id[1] == 'E' && remb->id[2] == 'M' && remb->id[3] == 'B') {
					/* FIXME From rtcp_utility.cc */
					unsigned char *_ptrRTCPData = (unsigned char *)remb;
					_ptrRTCPData += 4;	/* Skip unique identifier and num ssrc */
					uint8_t brExp = (_ptrRTCPData[1] >> 2) & 0x3F;
					uint32_t brMantissa = (_ptrRTCPData[1] & 0x03) << 16;
					brMantissa += (_ptrRTCPData[2] << 8);
					brMantissa += (_ptrRTCPData[3]);
					uint64_t origbitrate = brMantissa << brExp;
					if(origbitrate > bitrate) {
            ELOG_DEBUG("Got REMB bitrate %"SCNu64", need to cap it to %"SCNu64"\n", origbitrate, bitrate);
            ELOG_DEBUG("  >> %u * 2^%u = %"SCNu64"\n", brMantissa, brExp, origbitrate);
						/* bitrate --> brexp/brmantissa */
						uint8_t b = 0;
						uint8_t newbrexp = 0;
						uint32_t newbrmantissa = 0;
						for(b=0; b<64; b++) {
							if(bitrate <= ((uint64_t) 0x3FFFF << b)) {
								newbrexp = b;
								break;
							}
						}
						newbrmantissa = bitrate >> b;
            ELOG_DEBUG("new brexp:      %"SCNu8"\n", newbrexp);
            ELOG_DEBUG("new brmantissa: %"SCNu32"\n", newbrmantissa);
						/* FIXME From rtcp_sender.cc */
						_ptrRTCPData[1] = (uint8_t)((newbrexp << 2) + ((newbrmantissa >> 16) & 0x03));
						_ptrRTCPData[2] = (uint8_t)(newbrmantissa >> 8);
						_ptrRTCPData[3] = (uint8_t)(newbrmantissa);
					}
				}
			}
		}
		/* Is this a compound packet? */
		int length = ntohs(rtcp->length);
		if(length == 0)
			break;
		total -= length*4+4;
		if(total <= 0)
			break;
		rtcp = (rtcp_header *)((uint32_t*)rtcp + length + 1);
	}
	return 0;
}
    
/* seq_info_t list functions */
void janus_seq_append(seq_info_t **head, seq_info_t *new_seq) {
  if(*head == NULL) {
	new_seq->prev = new_seq;
	new_seq->next = new_seq;
	*head = new_seq;
  } else {
	seq_info_t *last_seq = (*head)->prev;
	new_seq->prev = last_seq;
	new_seq->next = *head;
	(*head)->prev = new_seq;
	last_seq->next = new_seq;
  }
}

seq_info_t *janus_seq_pop_head(seq_info_t **head) {
  seq_info_t *pop_seq = *head;
  if(pop_seq) {
	seq_info_t *new_head = pop_seq->next;
	if (pop_seq == new_head) {
	  *head = NULL;
	} else {
	  *head = new_head;
	  new_head->prev = pop_seq->prev;
	  new_head->prev->next = new_head;
	}
  }
  return pop_seq;
}

gint64 janus_get_monotonic_time(void) {
  struct timespec ts;
  clock_gettime (CLOCK_MONOTONIC, &ts);
  return (ts.tv_sec*G_GINT64_CONSTANT(1000000)) + (ts.tv_nsec/G_GINT64_CONSTANT(1000));
}

void janus_seq_list_free(seq_info_t **head) {
  if(!*head) return;
  seq_info_t *cur = *head;
  do {
	seq_info_t *next = cur->next;
	g_free(cur);
	cur = next;
  } while(cur != *head);
  *head = NULL;
}

int janus_seq_in_range(guint16 seqn, guint16 start, guint16 len) {
  /* Supports wrapping sequence (easier with int range) */
  int n = seqn;
  int nh = (1<<16) + n;
  int s = start;
  int e = s + len;
  return (s <= n && n < e) || (s <= nh && nh < e);
}
}  // orbit

