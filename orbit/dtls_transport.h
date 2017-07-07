/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * dtls_transport.h
 * ---------------------------------------------------------------------------
 * Defines the class and interface to manage the DTLS based transportion.
 * ---------------------------------------------------------------------------
 * Reference implementation: DtlsTransport.h (Erizo)
 */

#ifndef DTLS_TRANSPORT_H__
#define DTLS_TRANSPORT_H__

#include <string.h>
#include <boost/thread/mutex.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio.hpp>
#include <boost/scoped_ptr.hpp>
#include "dtls/DtlsSocket.h"
#include "nice_connection.h"
#include "transport.h"
#include "logger_helper.h"

namespace orbit {

class SrtpChannel;
class Resender;
class DtlsTransport : dtls::DtlsReceiver, public Transport {
  public:
    DtlsTransport(MediaType med, const std::string &transport_name, bool bundle, bool rtcp_mux, TransportListener *transportListener, const IceConfig& iceConfig, std::string username, std::string password, bool isServer);
    virtual ~DtlsTransport();
    void connectionStateChanged(IceState newState);
    std::string getMyFingerprint();
    static bool isDtlsPacket(const char* buf, int len);
    void onNiceData(unsigned int component_id, char* data, int len, NiceConnection* nice);
    void onCandidate(const CandidateInfo &candidate, NiceConnection *conn);
    void write(char* data, int len);
    void writeDtls(dtls::DtlsSocketContext *ctx, const unsigned char* data, unsigned int len);
    void onHandshakeCompleted(dtls::DtlsSocketContext *ctx, std::string clientKey, std::string serverKey, std::string srtp_profile);
    void updateIceState(IceState state, NiceConnection *conn);
    void processLocalSdp(SdpInfo *localSdp_);

  private:
    char protectBuf_[5000];
    char unprotectBuf_[5000];
    std::shared_ptr<dtls::DtlsSocketContext> dtlsRtp, dtlsRtcp;
    boost::mutex writeMutex_,sessionMutex_;
    boost::scoped_ptr<SrtpChannel> srtp_, srtcp_;
    bool readyRtp, readyRtcp;
    bool running_, isServer_;
    boost::scoped_ptr<Resender> rtcpResender, rtpResender;
    boost::thread getNice_Thread_;
    void getNiceDataLoop();
    packetPtr p_;

    // Stats
    boost::mutex stats_mutex_;
    int rtp_packet_total_;
    int rtcp_packet_total_;
    int rtp_unprotect_fail_;
    int rtcp_unprotect_fail_;
  };

  class Resender {
  public:
    Resender(std::shared_ptr<NiceConnection> nice, unsigned int comp, const unsigned char* data, unsigned int len);
    virtual ~Resender();
    void start();
    void run();
    void cancel();
    int getStatus();
    void resend(const boost::system::error_code& ec);
  private:
    std::shared_ptr<NiceConnection> nice_;
    unsigned int comp_;
    int sent_;
    const unsigned char* data_;
    unsigned int len_;
    boost::asio::io_service service;
    boost::asio::deadline_timer timer;
    boost::scoped_ptr<boost::thread> thread_;
  };
}  // namespace orbit

#endif // DTLS_TRANSPORT_H__
