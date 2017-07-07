/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * janus_rtcp_processor.h
 * ---------------------------------------------------------------------------
 * Defines the interface for processing the rtcp packets.
 * Referred from Janus gateway code.
 * ---------------------------------------------------------------------------
 */

#ifndef JANUS_RTCP_PROCESSOR_H__
#define JANUS_RTCP_PROCESSOR_H__

#include <map>
#include <list>
#include "rtp_headers.h"
#include <glib.h>

#include <memory>
#include <boost/thread/mutex.hpp>

#include <arpa/inet.h>
#ifdef __MACH__
#include <machine/endian.h>
#else
#include <endian.h>
#endif
#include <inttypes.h>
#include <string.h>
#include <mutex>          // std::mutex

#include "janus_rtcp_defines.h"

namespace orbit {
class JanusRtcpProcessor {
public:
	JanusRtcpProcessor();
	virtual ~JanusRtcpProcessor() {
	}
	;
	void ProcessPacket(char* buf, int len);
private:
};

enum {
	SEQ_MISSING, SEQ_NACKED, SEQ_GIVEUP, SEQ_RECVED
};


/* Janus enqueued (S)RTP/(S)RTCP packet to send */
typedef struct janus_ice_queued_packet {
	char *data;
	gint length;
	gint type;
	gboolean control;
	gboolean encrypted;
} janus_ice_queued_packet;

/*! \brief A helper struct for determining when to send NACKs */
typedef struct janus_seq_info {
	gint64 ts;
	guint16 seq;
	guint16 state;
	guint16 times;
	struct janus_seq_info *next;
	struct janus_seq_info *prev;
} seq_info_t;


/*! \brief RTCP REMB (http://tools.ietf.org/html/draft-alvestrand-rmcat-remb-03) */
typedef struct rtcp_remb
{
  /*! \brief Unique identifier ('R' 'E' 'M' 'B') */
  char id[4];
  /*! \brief Num SSRC, Br Exp, Br Mantissa (bit mask) */
  uint32_t bitrate;
  /*! \brief SSRC feedback */
  uint32_t ssrc[1];
} rtcp_remb;


/*! \brief RTCP FIR (http://tools.ietf.org/search/rfc5104#section-4.3.1.1) */
typedef struct rtcp_fir
{
  /*! \brief SSRC of the media sender that needs to send a key frame */
  uint32_t ssrc;
  /*! \brief Sequence number (only the first 8 bits are used, the other 24 are reserved) */
  uint32_t seqnr;
} rtcp_fir;


/*! \brief RTCP-FB (http://tools.ietf.org/html/rfc4585) */
typedef struct rtcp_fb
{
  /*! \brief Common header */
  rtcp_header header;
  /*! \brief Sender SSRC */
  uint32_t ssrc;
  /*! \brief Media source */
  uint32_t media;
  /*! \brief Feedback Control Information */
  char fci[1];
} rtcp_fb;


/*! \brief Internal RTCP state context (for RR/SR) */
typedef struct rtcp_context
{
  /* Whether we received any RTP packet at all (don't send RR otherwise) */
  uint8_t enabled:1;

  uint16_t last_seq_nr;
  uint16_t seq_cycle;
  uint16_t base_seq;
  /* Payload type */
  uint16_t pt;

  /* RFC 3550 A.8 Interarrival Jitter */
  uint64_t transit;
  double jitter;
  /* Timestamp base (e.g., 48000 for opus audio, or 90000 for video) */
  uint32_t tb;

  /* Last SR received */
  uint32_t lsr;
  /* Monotonic time of last SR received */
  int64_t lsr_ts;

  /* Last RR/SR we sent */
  int64_t last_sent;

  /* RFC 3550 A.3 */
  uint32_t received;
  uint32_t received_prior;
  uint32_t expected;
  uint32_t expected_prior;
  int32_t lost;
} rtcp_context;

typedef struct JanusRtcpContext {
  /*! \brief RTCP context for the audio stream (may be bundled) */
  rtcp_context *rtcp_ctx = NULL;
  std::mutex rtcp_mtx;
}JanusRtcpContext;
#define AudioRtcpContext JanusRtcpContext
#define VideoRtcpContext JanusRtcpContext

/*! \brief Method to retrieve the LSR from an existing RTCP context
 * @param[in] ctx The RTCP context to query
 * @returns The last SR received */
uint32_t janus_rtcp_context_get_lsr(rtcp_context *ctx);
/*! \brief Method to retrieve the number of received packets from an existing RTCP context
 * @param[in] ctx The RTCP context to query
 * @returns The number of received packets */
uint32_t janus_rtcp_context_get_received(rtcp_context *ctx);
/*! \brief Method to retrieve the number of lost packets from an existing RTCP context
 * @param[in] ctx The RTCP context to query
 * @returns The number of lost packets */
uint32_t janus_rtcp_context_get_lost(rtcp_context *ctx);
/*! \brief Method to compute the fraction of lost packets from an existing RTCP context
 * @param[in] ctx The RTCP context to query
 * @returns The fraction of lost packets */
uint32_t janus_rtcp_context_get_lost_fraction(rtcp_context *ctx);
/*! \brief Method to conpute the number of lost packets (pro mille) from an existing RTCP context
 * @param[in] ctx The RTCP context to query
 * @returns The number of lost packets (pro mille) */
uint32_t janus_rtcp_context_get_lost_promille(rtcp_context *ctx);
/*! \brief Method to retrieve the jitter from an existing RTCP context
 * @param[in] ctx The RTCP context to query
 * @returns The computed jitter */
uint32_t janus_rtcp_context_get_jitter(rtcp_context *ctx);


/*! \brief Method to quickly retrieve the sender SSRC (needed for demuxing RTCP in BUNDLE)
 * @param[in] packet The message data
 * @param[in] len The message data length in bytes
 * @returns The sender SSRC, or 0 in case of error */
guint32 janus_rtcp_get_sender_ssrc(char *packet, int len);
/*! \brief Method to quickly retrieve the received SSRC (needed for demuxing RTCP in BUNDLE)
 * @param[in] packet The message data
 * @param[in] len The message data length in bytes
 * @returns The receiver SSRC, or 0 in case of error */
guint32 janus_rtcp_get_receiver_ssrc(char *packet, int len);

/*! \brief Method to quickly retrieve the media SSRC
 * @param[in] packet The message data
 * @param[in] len    The message data length in bytes
 * @returns media ssrc or -1 on errors , 0 on not found*/
uint32_t janus_rtcp_get_fbnack_media_ssrc(char *packet, int len);

/*! \brief Method to parse/validate an RTCP message
 * @param[in] ctx RTCP context to update, if needed (optional)
 * @param[in] packet The message data
 * @param[in] len The message data length in bytes
 * @returns 0 in case of success, -1 on errors */
int janus_rtcp_parse(rtcp_context *ctx, char *packet, int len);


/* seq_info_t list functions */
void janus_seq_append(seq_info_t **head, seq_info_t *new_seq);
seq_info_t *janus_seq_pop_head(seq_info_t **head);
gint64 janus_get_monotonic_time(void);
void janus_seq_list_free(seq_info_t **head);
int janus_seq_in_range(guint16 seqn, guint16 start, guint16 len);

uint janus_get_max_nack_queue(void);

/*! \brief Method to fix an RTCP message (http://tools.ietf.org/html/draft-ietf-straw-b2bua-rtcp-00)
 * @param[in] packet The message data
 * @param[in] len The message data length in bytes
 * @param[in] fixssrc Whether the method needs to fix the message or just parse it
 * @param[in] newssrcl The SSRC of the sender to put in the message
 * @param[in] newssrcr The SSRC of the receiver to put in the message
 * @returns 0 in case of success, -1 on errors */
int janus_rtcp_fix_ssrc(rtcp_context *ctx, char *packet, int len, int fixssrc, uint32_t newssrcl, uint32_t newssrcr);

/*! \brief Method to quickly process the header of an incoming RTP packet to update the associated RTCP context
 * @param[in] ctx RTCP context to update, if needed (optional)
 * @param[in] packet The RTP packet
 * @param[in] len The packet data length in bytes
 * @param[in] max_nack_queue Current value of the max NACK value in the handle stack
 * @returns 0 in case of success, -1 on errors */
int janus_rtcp_process_incoming_rtp(rtcp_context *ctx, char *packet, int len, int max_nack_queue);

/*! \brief Method to fill in a Report Block in a Receiver Report
 * @param[in] ctx The RTCP context to use for the report
 * @param[in] rb Pointer to a valid report_block area of the RTCP data
 * @returns 0 in case of success, -1 on errors */
int janus_rtcp_report_block(rtcp_context *ctx, report_block *rb);

/*! \brief Method to check whether an RTCP message contains a FIR request
 * @param[in] packet The message data
 * @param[in] len The message data length in bytes
 * @returns TRUE in case of success, FALSE otherwise */
gboolean janus_rtcp_has_fir(char *packet, int len);

/*! \brief Method to check whether an RTCP message contains a PLI request
 * @param[in] packet The message data
 * @param[in] len The message data length in bytes
 * @returns TRUE in case of success, FALSE otherwise */
gboolean janus_rtcp_has_pli(char *packet, int len);

/*! \brief Inspect an existing RTCP REMB message to retrieve the reported bitrate
 * @param[in] packet The message data
 * @param[in] len The message data length in bytes
 * @returns The reported bitrate if successful, 0 if no REMB packet was available */
uint64_t janus_rtcp_get_remb(char *packet, int len);


/*! \brief Method to parse an RTCP NACK message
 * @param[in] packet The message data
 * @param[in] len The message data length in bytes
 * @returns A list of janus_nack elements containing the sequence numbers to send again */
GSList *janus_rtcp_get_nacks(char *packet, int len);

/*! \brief Method to remove an RTCP NACK message
 * @param[in] packet The message data
 * @param[in] len The message data length in bytes
 * @returns The new message data length in bytes
 * @note This is mostly a placeholder: for the sake of simplicity, whenever we handle
 * some sequence numbers in a NACK, we remove the NACK as a whole before forwarding the
 * RTCP message. Future versions will only selectively remove the sequence numbers that
 * have been handled. */
int janus_rtcp_remove_nacks(char *packet, int len);

/*! \brief Method to modify an existing RTCP REMB message to cap the reported bitrate
 * @param[in] packet The message data
 * @param[in] len The message data length in bytes
 * @param[in] bitrate The new bitrate to report (e.g., 128000)
 * @returns 0 in case of success, -1 on errors */
int janus_rtcp_cap_remb(char *packet, int len, uint64_t bitrate);


/*! \brief Method to generate a new RTCP SDES message
 * @param[in] packet The buffer data
 * @param[in] len The buffer data length in bytes
 * @param[in] cname The CNAME to write
 * @param[in] cnamelen The CNAME data length in bytes
 * @returns The message data length in bytes, if successful, -1 on errors */
int janus_rtcp_sdes(char *packet, int len, const char *cname, int cnamelen);

/*! \brief Method to generate a new RTCP REMB message to cap the reported bitrate
 * @param[in] packet The buffer data (MUST be at least 24 chars)
 * @param[in] len The message data length in bytes (MUST be 24)
 * @param[in] bitrate The bitrate to report (e.g., 128000)
 * @returns The message data length in bytes, if successful, -1 on errors */
int janus_rtcp_remb(char *packet, int len, uint64_t bitrate);

/*! \brief Method to generate a new RTCP FIR message to request a key frame
 * @param[in] packet The buffer data (MUST be at least 20 chars)
 * @param[in] len The message data length in bytes (MUST be 20)
 * @param[in,out] seqnr The current FIR sequence number (will be incremented by the method)
 * @returns The message data length in bytes, if successful, -1 on errors */
int janus_rtcp_fir(char *packet, int len, int *seqnr);

/*! \brief Method to generate a new legacy RTCP FIR (RFC2032) message to request a key frame
 * \note This is actually identical to janus_rtcp_fir(), with the difference that we set 192 as packet type
 * @param[in] packet The buffer data (MUST be at least 20 chars)
 * @param[in] len The message data length in bytes (MUST be 20)
 * @param[in,out] seqnr The current FIR sequence number (will be incremented by the method)
 * @returns The message data length in bytes, if successful, -1 on errors */
int janus_rtcp_fir_legacy(char *packet, int len, int *seqnr);

/*! \brief Method to generate a new RTCP PLI message to request a key frame
 * @param[in] packet The buffer data (MUST be at least 12 chars)
 * @param[in] len The message data length in bytes (MUST be 12)
 * @returns The message data length in bytes, if successful, -1 on errors */
int janus_rtcp_pli(char *packet, int len);

int janus_rtcp_bye(char *packet, int len);

/* Sort GSList from small to big*/
gint sort_asc(gconstpointer p1, gconstpointer p2);

/*! \brief Method to generate a new RTCP NACK message to report lost packets
 * @param[in] packet The buffer data (MUST be at least 16 chars)
 * @param[in] len The message data length in bytes (MUST be 16)
 * @param[in] nacks List of packets to NACK
 * @returns The message data length in bytes, if successful, -1 on errors */
int janus_rtcp_nacks(char *packet, int len, GSList *nacks);

}  // namespace orbit

#endif  // JANUS_RTCP_PROCESSOR_H__
