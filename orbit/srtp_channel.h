/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * srtp_channel.h
 * ---------------------------------------------------------------------------
 * Defines the class and interface to implement the SRTP based communication
 * channels.
 * ---------------------------------------------------------------------------
 * Reference implementation: Srtpchannel.h (Erizo)
 */

#ifndef SRTP_CHANNEL_H_
#define SRTP_CHANNEL_H_

#include <string>
#include <netinet/in.h>
#include <srtp/srtp.h>
#include <boost/thread/mutex.hpp>

#include "rtp/rtp_headers.h"
#include "stream_service/orbit/logger_helper.h"

namespace orbit {

/**
 * A SRTP data Channel.
 * Represents a SRTP Channel with keys for protecting and unprotecting RTP and RTCP data.
 */
class SrtpChannel {
	static bool initialized;
	static boost::mutex sessionMutex_;

public:
	/**
	 * The constructor. At this point the class is only initialized but it still needs the Key pair.
	 */
	SrtpChannel();
	virtual ~SrtpChannel();
	/**
	 * Protects RTP Data
	 * @param buffer Pointer to the buffer with the data. The protected data is returned here
	 * @param len Pointer to the length of the data. The length is returned here
	 * @return 0 or an error code
	 */
	int protectRtp(char* buffer, int *len);
	/**
	 * Unprotects RTP Data
	 * @param buffer Pointer to the buffer with the data. The unprotected data is returned here
	 * @param len Pointer to the length of the data. The length is returned here
	 * @return 0 or an error code
	 */
	int unprotectRtp(char* buffer, int *len);
	/**
	 * Protects RTCP Data
	 * @param buffer Pointer to the buffer with the data. The protected data is returned here
	 * @param len Pointer to the length of the data. The length is returned here
	 * @return 0 or an error code
	 */
	int protectRtcp(char* buffer, int *len);
	/**
	 * Unprotects RTCP Data
	 * @param buffer Pointer to the buffer with the data. The unprotected data is returned here
	 * @param len Pointer to the length of the data. The length is returned here
	 * @return 0 or an error code
	 */
	int unprotectRtcp(char* buffer, int *len);
	/**
	 * Sets a key pair for the RTP channel
	 * @param sendingKey The key for protecting data
	 * @param receivingKey The key for unprotecting data
	 * @return true if everything is ok
	 */
	bool setRtpParams(char* sendingKey, char* receivingKey);
	/**
	 * Sets a key pair for the RTCP channel
	 * @param sendingKey The key for protecting data
	 * @param receivingKey The key for unprotecting data
	 * @return true if everything is ok
	 */
	bool setRtcpParams(char* sendingKey, char* receivingKey);

private:
	enum TransmissionType {
		SENDING, RECEIVING
	};

	bool configureSrtpSession(srtp_t *session, const char* key,
			enum TransmissionType type);

	bool active_;
	srtp_t send_session_;
	srtp_t receive_session_;
	srtp_t rtcp_send_session_;
	srtp_t rtcp_receive_session_;
};

} /* namespace orbit */

#endif /* SRTP_CHANNEL_H_ */
