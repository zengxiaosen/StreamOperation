/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * nice_connection.h
 * ---------------------------------------------------------------------------
 * Defines the class and interface to communicate via the Nice connection.
 * ---------------------------------------------------------------------------
 * Reference implementation: NiceConnection.h (Erizo)
 * 
 */

#ifndef NICE_CONNECTION_H_
#define NICE_CONNECTION_H_

#include <string>
#include <vector>
#include <queue>
#include <atomic>
#include <boost/scoped_ptr.hpp>
#include <boost/thread.hpp>

#include "media_definitions.h"
#include "sdp_info.h"

typedef struct _NiceAgent NiceAgent;
typedef struct _GMainContext GMainContext;

typedef unsigned int uint;

namespace orbit {

#define NICE_STREAM_MAX_UFRAG   (256 + 1)  /* ufrag + NULL */
#define NICE_STREAM_MAX_UNAME   (256 * 2 + 1 + 1) /* 2*ufrag + colon + NULL */
#define NICE_STREAM_MAX_PWD     (256 + 1)  /* pwd + NULL */
#define NICE_STREAM_DEF_UFRAG   (4 + 1)    /* ufrag + NULL */
#define NICE_STREAM_DEF_PWD     (22 + 1)   /* pwd + NULL */

//forward declarations
typedef std::shared_ptr<dataPacket> packetPtr;
class CandidateInfo;
class WebRtcConnection;

struct CandidatePair{
  std::string orbitCandidateIp;
  int orbitCandidatePort;
  std::string clientCandidateIp;
  int clientCandidatePort;
};

class IceConfig {
  public:
    std::string turnServer, turnUsername, turnPass;
    std::string stunServer, publicIp;
    uint16_t stunPort, turnPort, minPort, maxPort;
    bool use_relay_mode;
    IceConfig(){
      publicIp = "";
      turnServer = "";
      turnUsername = "";
      turnPass = "";
      stunServer = "";
      stunPort = 0;
      turnPort = 0;
      minPort = 0;
      maxPort = 0;
      use_relay_mode = false;
    };
};

/**
 * States of ICE
 */
enum IceState {
    NICE_INITIAL,
    NICE_GATHERING_DONE,
    NICE_READY,
    NICE_FINISHED,
    NICE_FAILED
};

class NiceConnectionListener {
  public:
    virtual void onNiceData(unsigned int component_id, char* data, int len, NiceConnection* conn)=0;
    virtual void onCandidate(const CandidateInfo &candidate, NiceConnection *conn)=0;
    virtual void updateIceState(IceState state, NiceConnection *conn)=0;
    virtual void onNewSelectedPair(CandidatePair pair, NiceConnection* conn)=0;
    virtual void onCandidateGatheringDone(NiceConnection* conn)=0;
    virtual void updateNiceComponentState(std::string show_state, NiceConnection* conn)=0;
};

/**
 * An ICE connection via libNice
 * Represents an ICE Connection in an new thread.
 *
 */
class NiceConnection {
 public:
  unsigned int nrecv_lost = 0;
  unsigned int nsend_lost = 0;
	/**
	 * The MediaType of the connection
	 */
	MediaType mediaType;
	/**
	 * The transport name
	 */
  boost::scoped_ptr<std::string> transportName;

	/**
	 * Constructs a new NiceConnection.
	 * @param med The MediaType of the connection.
	 * @param transportName The name of the transport protocol. Was used when WebRTC used video_rtp instead of just rtp.
   * @param iceComponents Number of ice components pero connection. Default is 1 (rtcp-mux).
	 */
	NiceConnection(MediaType med, const std::string &transportName, NiceConnectionListener* listener, unsigned int iceComponents,
		const IceConfig& iceConfig, std::string username = "", std::string password = "");

  virtual ~NiceConnection();

  static void EnableDebug();

	/**
	 * Starts Gathering candidates in a new thread.
	 */
	void start();
	/**
	 * Sets the remote ICE Candidates.
	 * @param candidates A vector containing the CandidateInfo.
	 * @return true if successfull.
	 */
	bool setRemoteCandidates(std::vector<CandidateInfo> &candidates, bool isBundle);
	/**
	 * Sets the local ICE Candidates. Called by C Nice functions.
	 * @param candidates A vector containing the CandidateInfo.
	 * @return true if successfull.
	 */
	void gatheringDone(uint stream_id);
	/**
	 * Sets a local ICE Candidates. Called by C Nice functions.
	 * @param candidate info to look for
	 */
	void getCandidate(uint stream_id, uint component_id, const std::string &foundation);
	/**
	 * Get local ICE credentials.
	 * @param username and password where credentials will be stored
	 */
	void getLocalCredentials(std::string& username, std::string& password);
  
	/**
	 * Set remote credentials
	 * @param username and password
	 */
  void setRemoteCredentials (const std::string& username, const std::string& password);
    /**
	 * Gets the associated Listener.
	 * @param connection Pointer to the NiceConnectionListener.
	 */
	NiceConnectionListener* getNiceListener();
	/**
	 * Sends data via the ICE Connection.
	 * @param buf Pointer to the data buffer.
	 * @param len Length of the Buffer.
	 * @return Bytes sent.
	 */
	int sendData(unsigned int compId, const void* buf, int len);

	void updateIceState(IceState state);
  IceState checkIceState();
  void updateComponentState(unsigned int compId,
                            unsigned int component_state);
  void onNewSelectedPair(CandidatePair pair);

  void queueData(unsigned int component_id, char* buf, int len);
  
  CandidatePair getSelectedPair();

  packetPtr getPacket();
  void close();

private:
	void init();

  void ApplyNetworkInterfaces();

	NiceAgent* agent_;
	NiceConnectionListener* listener_;
  std::queue<packetPtr> niceQueue_;
  unsigned int candsDelivered_;

	GMainContext* context_;
	boost::thread m_Thread_;
	IceState iceState_;
  boost::mutex queueMutex_;
	boost::condition_variable cond_;
  unsigned int iceComponents_;
  std::map <unsigned int, IceState> comp_state_list_;
  bool running_;
	std::string ufrag_, upass_;

  std::set<std::string> apply_network_interfaces_;

  boost::mutex candidatesMutex_;
  std::vector<CandidateInfo> candidates_;

  IceConfig ice_config_;

  // agent handlers
  unsigned long gather_done_handler_{0};
  unsigned long state_change_handler_{0}; 
  unsigned long new_pair_handler_{0};
  unsigned long new_candidate_handler_{0};

  // Stats
  boost::mutex stats_mutex_;
  int drop_queue_packet_;

  long long last_candidate_timestamp_;
};
} /* namespace orbit */
#endif /* NICE_CONNECTION_H_ */
