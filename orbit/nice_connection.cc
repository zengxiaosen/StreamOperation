/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * nice_connection.cc
 * ---------------------------------------------------------------------------
 * Implements the interface to communicate via the Nice connection.
 * ---------------------------------------------------------------------------
 * Reference implementation: NiceConnection.cc (Erizo)
 */

#include <nice/nice.h>
//#include "stream_service/third_party/libnice/upstream/nice/nice.h"
#include <cstdio>
#include <poll.h>

#include "logger_helper.h"
#include "nice_connection.h"
#include "sdp_info.h"
#include "stream_service/orbit/base/strutil.h"
#include "stream_service/orbit/base/timeutil.h"
#include "stream_service/orbit/base/session_info.h"

#include "media_definitions.h"

#include "gflags/gflags.h"
#include "glog/logging.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/prctl.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <net/if.h>

// If true (and configured properly below) orbit will generate relay candidates for itself MOSTLY USEFUL WHEN ORBIT ITSELF IS BEHIND A NAT
#define SERVER_SIDE_TURN 0

#define REMOTE_CANDIDATE_TIMEOUT 60000

#define NICE_QUEUE_MAX_NUMBER 1000

DEFINE_string(apply_network_interfaces, "eth0,eth1,wlan0",
              "Apply the following interfaces to ICE.");
DEFINE_bool(enable_keepalive, true,
            "Enable the keepalive-conncheck in the Libnice.");
namespace orbit {

  namespace {
  }
  
  static const char *IceStateToString(IceState state) {
    switch (state) {
      case NICE_INITIAL:
        return "NICE_INITIAL";
      case NICE_GATHERING_DONE:
        return "NICE_GATHERING_DONE";
      case NICE_READY:
        return "NICE_READY";
      case NICE_FINISHED:
        return "NICE_FINISHED";
      case NICE_FAILED:
        return "NICE_FAILED";
      default:
        LOG(ERROR) << "Unknown IceState" << state;
        return "UNKNOWN_IceState";
    }
  }

  static const char *NiceComponentStateToString(NiceComponentState state) {
    switch (state) {
      case NICE_COMPONENT_STATE_DISCONNECTED:
        return "NICE_COMPONENT_STATE_DISCONNECTED";
      case NICE_COMPONENT_STATE_GATHERING:
        return "NICE_COMPONENT_STATE_GATHERING";
      case NICE_COMPONENT_STATE_CONNECTING:
        return "NICE_COMPONENT_STATE_CONNECTING";
      case NICE_COMPONENT_STATE_CONNECTED:
        return "NICE_COMPONENT_STATE_CONNECTED";
      case NICE_COMPONENT_STATE_READY:
        return "NICE_COMPONENT_STATE_READY";
      case NICE_COMPONENT_STATE_FAILED:
        return "NICE_COMPONENT_STATE_FAILED";
      case NICE_COMPONENT_STATE_LAST:
        return "NICE_COMPONENT_STATE_LAST";
      default:
        LOG(ERROR) << "Unknown NiceComponentState";
        return "UNKNOWN_NiceComponentState";
    }
  }


  struct queue_not_empty
  {
    std::queue<packetPtr>& queue;

    queue_not_empty(std::queue<packetPtr>& queue_):
      queue(queue_)
    {}
    bool operator()() const
    {
      return !queue.empty();
    }
  };


  int timed_poll(GPollFD* fds, guint nfds, gint timeout){
    return poll((pollfd*)fds,nfds,200);
  }

  void cb_nice_recv(NiceAgent* agent, guint stream_id, guint component_id,
      guint len, gchar* buf, gpointer user_data) {
    if (agent==NULL||user_data==NULL||len==0) return;
    NiceConnection* nicecon = (NiceConnection*) user_data;
    nicecon->queueData(component_id, reinterpret_cast<char*> (buf), 
                       static_cast<unsigned int> (len));
  }

  void cb_new_candidate(NiceAgent *agent, guint stream_id, guint component_id, 
                        gchar *foundation, gpointer user_data) {
    if (agent==NULL||user_data==NULL) return;
    NiceConnection *conn = (NiceConnection*) user_data;
    std::string found(foundation);
    conn->getCandidate(stream_id, component_id, found);
  }

  void cb_candidate_gathering_done(NiceAgent *agent, guint stream_id,
      gpointer user_data) {
    if (agent==NULL||user_data==NULL) return;
    NiceConnection *conn = (NiceConnection*) user_data;
    conn->gatheringDone(stream_id);
  }

  void cb_component_state_changed(NiceAgent *agent, guint stream_id,
      guint component_id, guint state, gpointer user_data) {
    if (agent==NULL||user_data==NULL) return;
    NiceConnection* conn = (NiceConnection*)user_data;

    string state_str = NiceComponentStateToString((NiceComponentState)state);
    LOG(INFO) << "cb_component_state_changed triggered....stream_id=" 
              << stream_id << "component_id" << component_id 
	            << " state=" << state_str;

    conn->updateComponentState(component_id, state);
  }

  void cb_new_selected_pair(NiceAgent *agent, guint stream_id, guint component_id,
      gchar *lfoundation, gchar *rfoundation, gpointer user_data) {
    if (agent==NULL||user_data==NULL) return;
    NiceConnection *conn = (NiceConnection*) user_data;
    LOG(INFO) << "cb_new_selected_pair:" << component_id
            << " lfoundation" << lfoundation << " rfoundation:" << rfoundation;
    CandidatePair pair = conn->getSelectedPair();
    conn->onNewSelectedPair(pair);
  }

  void NiceConnection::EnableDebug() {
    LOG(INFO) << "Enable Libnice Debugging.";
    nice_debug_enable(true);
  }

  void NiceConnection::ApplyNetworkInterfaces() {
    if (FLAGS_apply_network_interfaces.empty()) {
      return;
    } else {
      vector<string> result;
      SplitStringUsing(FLAGS_apply_network_interfaces, ",", &result);
      for (auto item : result) {
        apply_network_interfaces_.insert(item);
      }
    }
    
    /* Add all local addresses, except those in the ignore list */
    struct ifaddrs *ifaddr, *ifa;
    int family, s, n;
    char host[NI_MAXHOST];
    if(getifaddrs(&ifaddr) == -1) {
      LOG(ERROR) << "Error getting list of interfaces...";
    } else {
      for(ifa = ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next, n++) {
        if(ifa->ifa_addr == NULL)
          continue;
        /* Skip interfaces which are not up and running */
        if (!((ifa->ifa_flags & IFF_UP) && (ifa->ifa_flags & IFF_RUNNING)))
          continue;
        /* Skip loopback interfaces */
        if (ifa->ifa_flags & IFF_LOOPBACK)
          continue;
        family = ifa->ifa_addr->sa_family;
        if(family != AF_INET && family != AF_INET6)
          continue;
        if(family == AF_INET6)
          continue;
        /* Check the interface name first, we can ignore that as well: enforce list would be checked later */
        s = getnameinfo(ifa->ifa_addr,
                        (family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6),
                        host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
        if(s != 0) {
          LOG(ERROR) << "getnameinfo() failed:" << gai_strerror(s);
          continue;
        }
        string interface_name = ifa->ifa_name;
        LOG(INFO) << "interface_name=" << interface_name;
        if (apply_network_interfaces_.find(interface_name) == apply_network_interfaces_.end()) {
          LOG(INFO) << "Skip the interface=" << interface_name;
          continue;
        }

        /* Skip 0.0.0.0, :: and local scoped addresses  */
        if(!strcmp(host, "0.0.0.0") || !strcmp(host, "::") || !strncmp(host, "fe80:", 5))
          continue;
        /* Check if this IP address is in the ignore/enforce list, now: the enforce list has the precedence */

        /* Ok, add interface to the ICE agent */
        LOG(INFO) << "Adding " << host << " to the address to gather candidates.";
        NiceAddress addr_local;
        nice_address_init (&addr_local);
        if(!nice_address_set_from_string (&addr_local, host)) {
          LOG(ERROR) << "Skipping invalid address " << host;
          continue;
        }
        nice_agent_add_local_address (agent_, &addr_local);
      }
      freeifaddrs(ifaddr);
    }
  }

  NiceConnection::NiceConnection(MediaType med, const std::string &transport_name,
                                 NiceConnectionListener* listener, 
                                 unsigned int iceComponents, const IceConfig& iceConfig,
                                 std::string username, std::string password)
     : mediaType(med), agent_(NULL), listener_(listener), candsDelivered_(0), context_(NULL), iceState_(NICE_INITIAL), iceComponents_(iceComponents) {

    ice_config_ = iceConfig;
    transportName.reset(new std::string(transport_name));
    for (unsigned int i = 1; i<=iceComponents_; i++) {
      comp_state_list_[i] = NICE_INITIAL;
    }
    
    g_type_init();
    context_ = g_main_context_new();
    g_main_context_set_poll_func(context_,timed_poll);
    VLOG(3) << "Creating Agent";

    nice_debug_enable( FALSE );
    // Create a nice agent
    agent_ = nice_agent_new(context_, NICE_COMPATIBILITY_RFC5245);
    //agent_ = nice_agent_new(context_, NICE_COMPATIBILITY_GOOGLE);
    GValue controllingMode = { 0 };
    g_value_init(&controllingMode, G_TYPE_BOOLEAN);
    g_value_set_boolean(&controllingMode, false);
    g_object_set_property(G_OBJECT( agent_ ), "controlling-mode", &controllingMode);

    if (FLAGS_enable_keepalive) {
      GValue keepaliveMode = { 0 };
      g_value_init(&keepaliveMode, G_TYPE_BOOLEAN);
      g_value_set_boolean(&keepaliveMode, true);
      g_object_set_property(G_OBJECT( agent_ ), "keepalive-conncheck", &keepaliveMode);
    }

    GValue checks = { 0 };
    g_value_init(&checks, G_TYPE_UINT);
    g_value_set_uint(&checks, 100);
    g_object_set_property(G_OBJECT( agent_ ), "max-connectivity-checks", &checks);

    //use stunServer
    if (iceConfig.stunServer.compare("") != 0 && iceConfig.stunPort!=0){
      GValue val = { 0 }, val2 = { 0 };
      g_value_init(&val, G_TYPE_STRING);
      g_value_set_string(&val, iceConfig.stunServer.c_str());
      g_object_set_property(G_OBJECT( agent_ ), "stun-server", &val);

      g_value_init(&val2, G_TYPE_UINT);
      g_value_set_uint(&val2, iceConfig.stunPort);
      g_object_set_property(G_OBJECT( agent_ ), "stun-server-port", &val2);

      VLOG(3) << "Setting STUN server " << iceConfig.stunServer.c_str()
              << " : " << iceConfig.stunPort;
    }

    // Connect the signals
    gather_done_handler_ = g_signal_connect( G_OBJECT( agent_ ), 
        "candidate-gathering-done", 
        G_CALLBACK( cb_candidate_gathering_done ), this);
    state_change_handler_ = g_signal_connect( G_OBJECT( agent_ ), 
        "component-state-changed",
        G_CALLBACK( cb_component_state_changed ), this);
    new_pair_handler_ = g_signal_connect( G_OBJECT( agent_ ), 
        "new-selected-pair", G_CALLBACK( cb_new_selected_pair ), this);
    new_candidate_handler_ = g_signal_connect( G_OBJECT( agent_ ), 
        "new-candidate", G_CALLBACK( cb_new_candidate ), this);

    ApplyNetworkInterfaces();

    // Create a new stream and start gathering candidates
    VLOG(3) << "Adding Stream... Number of components " << iceComponents_;

    nice_agent_add_stream(agent_, iceComponents_);
    gchar *ufrag = NULL, *upass = NULL;
    nice_agent_get_local_credentials(agent_, 1, &ufrag, &upass);
    ufrag_ = std::string(ufrag); g_free(ufrag);
    upass_ = std::string(upass); g_free(upass);

    // Set our remote credentials.  This must be done *after* we add a stream.
    if (username.compare("")!=0 && password.compare("")!=0){
      VLOG(3) << "Setting remote credentials in constructor";
      this->setRemoteCredentials(username, password);
    }

    // Set Port Range ----> If this doesn't work when linking the file libnice.sym has to be modified to include this call
    if (iceConfig.minPort!=0 && iceConfig.maxPort!=0){
      VLOG(3) << "Setting port range: " << iceConfig.minPort << " to " 
              << iceConfig.maxPort;

      nice_agent_set_port_range(agent_, (guint)1, (guint)1, 
          (guint)iceConfig.minPort, (guint)iceConfig.maxPort);
    }

    if (iceConfig.turnServer.compare("") != 0 && iceConfig.turnPort!=0){
        VLOG(3) << "Setting TURN server: " << iceConfig.turnServer << " : " 
                << iceConfig.turnPort;
        VLOG(3) << "Setting TURN credentials " << iceConfig.turnUsername.c_str() 
                << ":" << iceConfig.turnPass.c_str();
        for (unsigned int i = 1; i <= iceComponents_ ; i++){
          nice_agent_set_relay_info (agent_,
              1,
              i,
              iceConfig.turnServer.c_str(),      // TURN Server IP
              iceConfig.turnPort,    // TURN Server PORT
              iceConfig.turnUsername.c_str(),      // Username
              iceConfig.turnPass.c_str(),      // Pass
              NICE_RELAY_TYPE_TURN_UDP);
        }
    }
    
    if(agent_){
      for (unsigned int i = 1; i<=iceComponents_; i++){
        nice_agent_attach_recv(agent_, 1, i, context_, cb_nice_recv, this);
      }
      running_ = true;
    }
    else{
      running_=false;
    }
    m_Thread_ = boost::thread(&NiceConnection::init, this);

    // Update the stats
    drop_queue_packet_ = 0;
  }

  NiceConnection::~NiceConnection() {
    this->close();
  }
  
  packetPtr NiceConnection::getPacket(){
      if(this->checkIceState()==NICE_FINISHED || !running_) {
          packetPtr p (new dataPacket());
          p->length=-1;
          return p;
      }
      boost::unique_lock<boost::mutex> lock(queueMutex_);
      boost::system_time const timeout=boost::get_system_time()+ boost::posix_time::milliseconds(300);
      if(!cond_.timed_wait(lock,timeout, queue_not_empty(niceQueue_))){
        packetPtr p (new dataPacket());
        p->length=0;
        return p;
      }
      if(this->checkIceState()==NICE_FINISHED || !running_) {
          packetPtr p (new dataPacket());
          p->length=-1;
          return p;
      }
      if(!niceQueue_.empty()){
        packetPtr p (niceQueue_.front());
        niceQueue_.pop();
        return  p;
      }
      packetPtr p (new dataPacket());
      p->length=0;
      return p;
  }

  void NiceConnection::close() {
    if(this->checkIceState()==NICE_FINISHED){
      return;
    }
    running_ = false;
    VLOG(3) << "Closing nice";
    this->updateIceState(NICE_FINISHED);
    cond_.notify_one();
    boost::system_time const timeout = 
        boost::get_system_time()+ boost::posix_time::milliseconds(500);
    if (!m_Thread_.timed_join(timeout) ){
      m_Thread_.interrupt();
    }

    if (agent_!=NULL){
      // disconnect the handlers of this agent
      g_signal_handler_disconnect ( G_OBJECT( agent_ ), gather_done_handler_);
      g_signal_handler_disconnect ( G_OBJECT( agent_ ), state_change_handler_);
      g_signal_handler_disconnect ( G_OBJECT( agent_ ), new_pair_handler_);
      g_signal_handler_disconnect ( G_OBJECT( agent_ ), new_candidate_handler_);
      g_object_unref(agent_);
      agent_ = NULL;
    }
    if (context_!=NULL) {
      g_main_context_unref(context_);
      context_=NULL;
    }

    listener_ = NULL;
    VLOG(3) << "Nice Closed.";
  }

  void NiceConnection::queueData(unsigned int component_id, char* buf, int len) {

    if (this->checkIceState() == NICE_READY && running_){
      boost::mutex::scoped_lock lock(queueMutex_);
      while (niceQueue_.size() > NICE_QUEUE_MAX_NUMBER) {
        niceQueue_.pop();
        boost::unique_lock<boost::mutex> lock(stats_mutex_);
        drop_queue_packet_++;
      }
      packetPtr p_ (new dataPacket());
      memcpy(p_->data, buf, len);
      p_->comp = component_id;
      p_->length = len;
      niceQueue_.push(p_);
      cond_.notify_one();
    }
  }

  int NiceConnection::sendData(unsigned int compId, const void* buf, int len) {

    int val = -1;

    VLOG(3) << "nice sendData checkIceState=" << this->checkIceState()
            << " expected:" << NICE_READY << " running====" << running_;

    if (this->checkIceState() == NICE_READY && running_) {
      val = nice_agent_send(agent_, 1, compId, len,
                            reinterpret_cast<const gchar*>(buf));
    }
    if (val != len) {
      VLOG(3) << "Data sent " << val << " of " << len;
    } else {
    	VLOG(3) << "Data sent equal value";
    }
    return val;
  }

  void NiceConnection::init() {
    /* Set thread name */
    prctl(PR_SET_NAME, (unsigned long)"NiceConnectInit");

    VLOG(3) << "Gathering candidates threads initialized.";
    nice_agent_gather_candidates(agent_, 1);   
    // Attach to the component to receive the data
    while(running_){
      if(this->checkIceState()>=NICE_FINISHED || !running_)
        break;
      g_main_context_iteration(context_, true);
    }
  }

  bool NiceConnection::setRemoteCandidates(std::vector<CandidateInfo> &candidates, bool isBundle) {
    boost::unique_lock<boost::mutex> lock(candidatesMutex_);
    for (unsigned int it = 0; it < candidates.size(); it++) {
      CandidateInfo cinfo = candidates[it];
      candidates_.push_back(cinfo);
    }

    last_candidate_timestamp_ = getTimeMS();
    
    if(agent_==NULL){
      running_=false;
      return false;
    }
    GSList* candList = NULL;
    VLOG(3) << "Setting remote candidates " << candidates.size() << ",mediatype " << this->mediaType;

    for (unsigned int it = 0; it < candidates_.size(); it++) {
      NiceCandidateType nice_cand_type;
      CandidateInfo cinfo = candidates_[it];
      //If bundle we will add the candidates regardless the mediaType
      if (cinfo.componentId !=1 || (!isBundle && cinfo.mediaType!=this->mediaType ))
        continue;

      switch (cinfo.hostType) {
        case HOST:
          nice_cand_type = NICE_CANDIDATE_TYPE_HOST;
          break;
        case SRFLX:
          nice_cand_type = NICE_CANDIDATE_TYPE_SERVER_REFLEXIVE;
          break;
        case PRFLX:
          nice_cand_type = NICE_CANDIDATE_TYPE_PEER_REFLEXIVE;
          break;
        case RELAY:
          nice_cand_type = NICE_CANDIDATE_TYPE_RELAYED;
          break;
        default:
          nice_cand_type = NICE_CANDIDATE_TYPE_HOST;
          break;
      }
      NiceCandidate* thecandidate = nice_candidate_new(nice_cand_type);
      thecandidate->username = strdup(cinfo.username.c_str());
      thecandidate->password = strdup(cinfo.password.c_str());
      thecandidate->stream_id = (guint) 1;
      thecandidate->component_id = cinfo.componentId;
      thecandidate->priority = cinfo.priority;
      thecandidate->transport = NICE_CANDIDATE_TRANSPORT_UDP;
      nice_address_set_from_string(&thecandidate->addr, cinfo.hostAddress.c_str());
      nice_address_set_port(&thecandidate->addr, cinfo.hostPort);
      
      if (cinfo.hostType == RELAY||cinfo.hostType==SRFLX){
        nice_address_set_from_string(&thecandidate->base_addr, cinfo.rAddress.c_str());
        nice_address_set_port(&thecandidate->base_addr, cinfo.rPort);
        ELOG_DEBUG("Adding remote candidate type %d addr %s port %d raddr %s rport %d", cinfo.hostType, cinfo.hostAddress.c_str(), cinfo.hostPort,
            cinfo.rAddress.c_str(), cinfo.rPort);
      }else{
        ELOG_DEBUG("Adding remote candidate type %d addr %s port %d priority %d componentId %d, username %s, pass %s", 
          cinfo.hostType, 
          cinfo.hostAddress.c_str(), 
          cinfo.hostPort, 
          cinfo.priority, 
          cinfo.componentId,
          cinfo.username.c_str(),
          cinfo.password.c_str()
          );
      }
      candList = g_slist_prepend(candList, thecandidate);
    }
    //TODO: Set Component Id properly, now fixed at 1 
    nice_agent_set_remote_candidates(agent_, (guint) 1, 1, candList);
    VLOG(3) << "set_remote_candidates finished....Is any error?";
    g_slist_free_full(candList, (GDestroyNotify)&nice_candidate_free);
    
    return true;
  }

  void NiceConnection::gatheringDone(uint stream_id){
    ELOG_DEBUG("Gathering done for stream %u", stream_id);
    this->updateIceState(NICE_GATHERING_DONE);
  }

  void NiceConnection::getCandidate(uint stream_id, uint component_id, const std::string &foundation) {
    if(!running_)
      return;
    
    GSList* lcands = nice_agent_get_local_candidates(agent_, stream_id, component_id);
    // We only want to get the new candidates
    int listLength = g_slist_length(lcands);
    ELOG_DEBUG("List length %u, candsDelivered %u", listLength, candsDelivered_);
    if (candsDelivered_ <= g_slist_length(lcands)){
      GSList* newCands = g_slist_nth(lcands, (candsDelivered_));
      newCands = g_slist_copy_deep(newCands, (GCopyFunc)&nice_candidate_copy, NULL);
      g_slist_free_full(lcands, (GDestroyNotify)&nice_candidate_free);
      lcands = newCands;
    }
    ELOG_DEBUG("getCandidate %u", g_slist_length(lcands));

    for (GSList* iterator = lcands; iterator; iterator = iterator->next) {
      ELOG_DEBUG("Candidate");
      char address[NICE_ADDRESS_STRING_LEN], baseAddress[NICE_ADDRESS_STRING_LEN];
      NiceCandidate *cand = (NiceCandidate*) iterator->data;
      nice_address_to_string(&cand->addr, address);
      nice_address_to_string(&cand->base_addr, baseAddress);

      candsDelivered_++;
      if (strstr(address, ":") != NULL) { // We ignore IPv6 candidates at this point
        continue;
      }
      if (!ice_config_.publicIp.empty()) {
        if (!strstr(address, ice_config_.publicIp.c_str())) {
          LOG(INFO) << "not a public ip. drop it:" << address;
          continue;
        }
      }
      CandidateInfo cand_info;
      cand_info.componentId = cand->component_id;
      cand_info.foundation = cand->foundation;
      cand_info.priority = cand->priority;
      cand_info.hostAddress = std::string(address);
      cand_info.hostPort = nice_address_get_port(&cand->addr);
      cand_info.mediaType = mediaType;

      /*
       *   NICE_CANDIDATE_TYPE_HOST,
       *    NICE_CANDIDATE_TYPE_SERVER_REFLEXIVE,
       *    NICE_CANDIDATE_TYPE_PEER_REFLEXIVE,
       *    NICE_CANDIDATE_TYPE_RELAYED,
       */
      switch (cand->type) {
        case NICE_CANDIDATE_TYPE_HOST:
          cand_info.hostType = HOST;
          break;
        case NICE_CANDIDATE_TYPE_SERVER_REFLEXIVE:
          cand_info.hostType = SRFLX;
          cand_info.rAddress = std::string(baseAddress);
          cand_info.rPort = nice_address_get_port(&cand->base_addr);
          break;
        case NICE_CANDIDATE_TYPE_PEER_REFLEXIVE:
          cand_info.hostType = PRFLX;
          break;
        case NICE_CANDIDATE_TYPE_RELAYED:
          //char turnAddres[NICE_ADDRESS_STRING_LEN];
          ELOG_DEBUG("TURN LOCAL CANDIDATE");
          ELOG_DEBUG("address %s", address);
          ELOG_DEBUG("baseAddress %s", baseAddress);
          ELOG_DEBUG("stream_id %u", cand->stream_id);
          ELOG_DEBUG("priority %u", cand->priority);

          cand_info.hostType = RELAY;
          cand_info.rAddress = std::string(baseAddress);
          cand_info.rPort = nice_address_get_port(&cand->base_addr);
          break;
        default:
          break;
      }
      cand_info.netProtocol = "udp";
      cand_info.transProtocol = std::string(*transportName.get());
      cand_info.username = ufrag_;
      cand_info.password = upass_;
      this->getNiceListener()->onCandidate(cand_info, this);
    }
    // for nice_agent_get_local_candidates,  the caller owns the returned GSList as well as the candidates contained within it.
    // let's free everything in the list, as well as the list.
    g_slist_free_full(lcands, (GDestroyNotify)&nice_candidate_free);

  }

  void NiceConnection::getLocalCredentials(std::string& username, std::string& password) {
    username = ufrag_;
    password = upass_;
  }

  void NiceConnection::setRemoteCredentials (const std::string& username, const std::string& password){
    ELOG_DEBUG("Setting remote credentials %s, %s", username.c_str(), password.c_str());
    nice_agent_set_remote_credentials(agent_, (guint) 1, username.c_str(), password.c_str());
  }

  NiceConnectionListener* NiceConnection::getNiceListener() {
    return this->listener_;
  }

  void NiceConnection::onNewSelectedPair(CandidatePair pair) {
    if (this->listener_ != NULL) {
      this->listener_->onNewSelectedPair(pair, this);
    }    
  }

  void NiceConnection::updateComponentState(unsigned int compId,
                                            guint component_state) {
    IceState nice_state;
    if (this->listener_ != NULL && component_state != -1) {
      std::string showStatus = "";
      switch(component_state) {
      case NICE_COMPONENT_STATE_DISCONNECTED :
        nice_state = checkIceState();
        showStatus = "DISCONNECTED";
        break;
      case NICE_COMPONENT_STATE_GATHERING :
        nice_state = checkIceState();
        showStatus = "GATHERING";
        break;
      case NICE_COMPONENT_STATE_CONNECTING :
        nice_state = checkIceState();
        showStatus = "CONNECTING";
        break;
      case NICE_COMPONENT_STATE_CONNECTED :
        nice_state = NICE_READY;
        showStatus = "CONNECTED";
        break;
      case NICE_COMPONENT_STATE_READY :
        nice_state = checkIceState();
        showStatus = "READY";
        break;
      case NICE_COMPONENT_STATE_FAILED :
        nice_state = NICE_FAILED;
        showStatus = "FAILED";
        break;
      default :
        nice_state = checkIceState();
        showStatus =
          "UNKNOWN(" + std::to_string((unsigned int)component_state) + ")";
      }
      this->listener_->updateNiceComponentState(showStatus, this);
    }

    if (nice_state == NICE_FAILED) {
      // HACK(chengxu): If the nice_state is NICE_FAILED, we should check this in the
      // trickle ice status. If it is trickle_ice, the NICE_FAILED should be set
      // unless all the remote candidates are checked and failed. Thus, we have to
      // have a time_out mechanism here to avoid false failed.
      // The mechanism is:
      //   every time we set a remote candidate, we set a timestamp there.
      //   if the nice_failed event occurred after a certain timeout off the
      //   the timestamp we set the remote last time, we will really set it.
      long long cur = getTimeMS();
      long long diff = cur - last_candidate_timestamp_;
      if (last_candidate_timestamp_ !=  -1 && diff < REMOTE_CANDIDATE_TIMEOUT) {
        VLOG(3) << "Receivied NICE_FAILED but the NICE_FAILED is not set deferred until a certain timeout is met.";
        return;
      }
    }
    
    ELOG_DEBUG("%s - NICE Component State Changed %u - %u, total comps %u",
               transportName->c_str(), compId, nice_state, iceComponents_);
    VLOG(3) << transportName->c_str() << "  - NICE Component State Changed "
            << compId << " - " << nice_state << " total comps " << iceComponents_;

    comp_state_list_[compId] = nice_state;
    if (nice_state == NICE_READY) {
      for (unsigned int i = 1; i<=iceComponents_; i++) {
        if (comp_state_list_[i] != NICE_READY) {
          return;
        }
      }
    }else if (nice_state == NICE_FAILED){
      ELOG_ERROR("%s - NICE Component %u FAILED", transportName->c_str(), compId);
      for (unsigned int i = 1; i<=iceComponents_; i++) {
        if (comp_state_list_[i] != NICE_FAILED) {
          return;
        }
      }
    }
    this->updateIceState(nice_state);
  }

  IceState NiceConnection::checkIceState(){
    return iceState_;
  }

  void NiceConnection::updateIceState(IceState state) {
    ELOG_INFO("NICE State NICE_FINISHED %u", NICE_FINISHED);
    ELOG_INFO("NICE State NICE_FAILED %u", NICE_FAILED);
    ELOG_INFO("NICE State NICE_READY %u", NICE_READY);
    ELOG_INFO("NICE State NICE_GATHERING_DONE %u", NICE_GATHERING_DONE);
    ELOG_INFO("NICE State current %u", this->iceState_);
    ELOG_INFO("NICE state change to %u", state);
      
    if(iceState_==state)
      return;

    if (state == NICE_GATHERING_DONE) {
      if (this->listener_ != NULL) {
        this->listener_->onCandidateGatheringDone(this);
      }
    } else {
      ELOG_INFO("%s - NICE State Changing from %u to %u %p", transportName->c_str(), this->iceState_, state, this);
      this->iceState_ = state;

      switch( iceState_) {
        case NICE_FINISHED:
          this->running_=false;
          return;
        case NICE_FAILED:
          ELOG_ERROR("Nice Failed, stopping ICE");
          this->running_=false;
          break;
        case NICE_READY:
          break;
        default:
          break;
      }
      // Important: send this outside our state lock.  Otherwise,
      // serious risk of deadlock.
      if (this->listener_ != NULL) {
        this->listener_->updateIceState(state, this);
      }
    }
  }

  CandidatePair NiceConnection::getSelectedPair(){
    char ipaddr[NICE_ADDRESS_STRING_LEN];
    CandidatePair selectedPair;
    NiceCandidate* local, *remote;
    nice_agent_get_selected_pair(agent_, 1, 1, &local, &remote);
    nice_address_to_string(&local->addr, ipaddr);
    selectedPair.orbitCandidateIp = std::string(ipaddr);
    selectedPair.orbitCandidatePort = nice_address_get_port(&local->addr);
    ELOG_INFO("Selected pair:\nlocal candidate addr: %s:%d",ipaddr, nice_address_get_port(&local->addr));
    nice_address_to_string(&remote->addr, ipaddr);
    selectedPair.clientCandidateIp = std::string(ipaddr);
    selectedPair.clientCandidatePort = nice_address_get_port(&remote->addr);
    ELOG_INFO("remote candidate addr: %s:%d",ipaddr, nice_address_get_port(&remote->addr));
    return selectedPair;

  }
} /* namespace orbit */
