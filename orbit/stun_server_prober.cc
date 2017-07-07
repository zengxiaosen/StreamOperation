#include <nice/nice.h>
//#include "stream_service/third_party/libnice/upstream/nice/nice.h"
#include <netdb.h>

#include "stun_server_prober.h"
#include "logger_helper.h"

# ifdef __cplusplus
extern "C" {
  //#include <stun/stunagent.h>
  //#include <stun/usages/bind.h>
  #include "stream_service/third_party/libnice/upstream/stun/stunagent.h"
  #include "stream_service/third_party/libnice/upstream/stun/usages/bind.h"
}
#endif

namespace orbit {
  /*! \brief Convert a sockaddr address to an IP string
   * \note The resulting string is allocated, which means the caller must free it itself when done
   * @param address The sockaddr address to convert
   * @returns A string containing the IP address, if successful, NULL otherwise */
  char *janus_address_to_ip(struct sockaddr *address) {
    if(address == NULL)
      return NULL;
    char addr_buf[INET6_ADDRSTRLEN];
    const char *addr = NULL;
    struct sockaddr_in *sin = NULL;
    struct sockaddr_in6 *sin6 = NULL;

    switch(address->sa_family) {
    case AF_INET:
      sin = (struct sockaddr_in *)address;
      addr = inet_ntop(AF_INET, &sin->sin_addr, addr_buf, INET_ADDRSTRLEN);
      break;
    case AF_INET6:
      sin6 = (struct sockaddr_in6 *)address;
      addr = inet_ntop(AF_INET6, &sin6->sin6_addr, addr_buf, INET6_ADDRSTRLEN);
      break;
    default:
      /* Unknown family */
      break;
    }
    return addr ? g_strdup(addr) : NULL;
  }


  int StunServerProber::ValidateStunServer(std::string stun_server_str,
                                           uint16_t stun_port) {
    gchar *stun_server = (gchar*)stun_server_str.c_str();
    if(stun_server == NULL)
      return 0; /* No initialization needed */
    if(stun_port == 0)
      stun_port = 3478;
    ELOG_DEBUG("STUN server to use: %s:%u\n", stun_server, stun_port);
    /* Resolve address to get an IP */
    struct addrinfo *res = NULL;
    if(getaddrinfo(stun_server, NULL, NULL, &res) != 0) {
      ELOG_DEBUG("Could not resolve %s...\n", stun_server);
      if(res)
        freeaddrinfo(res);
      return -1;
    }
    janus_stun_server_ = janus_address_to_ip(res->ai_addr);
    freeaddrinfo(res);
    if(janus_stun_server_ == NULL) {
      ELOG_DEBUG("Could not resolve %s...\n", stun_server);
      return -1;
    }
    janus_stun_port_ = stun_port;
    ELOG_DEBUG("  >> %s:%u\n", janus_stun_server_, janus_stun_port_);

    /* Test the STUN server */
    StunAgent stun;
    stun_agent_init (&stun,
                     STUN_ALL_KNOWN_ATTRIBUTES,
                     STUN_COMPATIBILITY_RFC5389,
                     STUN_AGENT_USAGE_SHORT_TERM_CREDENTIALS);
    StunMessage msg;
    uint8_t buf[1500];
    size_t len = stun_usage_bind_create(&stun, &msg, buf, 1500);
    ELOG_DEBUG(" Testing STUN server: message is of %zu bytes\n", len);
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in address, remote;
    address.sin_family = AF_INET;
    address.sin_port = 0;
    address.sin_addr.s_addr = INADDR_ANY;
    remote.sin_family = AF_INET;
    remote.sin_port = htons(janus_stun_port_);
    remote.sin_addr.s_addr = inet_addr(janus_stun_server_);
    if(bind(fd, (struct sockaddr *)(&address), sizeof(struct sockaddr)) < 0) {
      LOG(FATAL) << "Bind failed for STUN BINDING test.";
      return -1;
    }
    int bytes = sendto(fd, buf, len, 0, (struct sockaddr*)&remote, sizeof(remote));
    if(bytes < 0) {
      LOG(FATAL) << "Error sending STUN BINDING test.";
      return -1;
    }
    ELOG_DEBUG(" >> Sent %d bytes %s:%u, waiting for reply...\n", bytes, janus_stun_server_, janus_stun_port_);
    struct timeval timeout;
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    timeout.tv_sec = 5; /* FIXME Don't wait forever */
    timeout.tv_usec = 0;
    select(fd+1, &readfds, NULL, NULL, &timeout);
    if(!FD_ISSET(fd, &readfds)) {
      LOG(ERROR) << "No response to our STUN BINDING test..";
      return -1;
    }
    socklen_t addrlen = sizeof(remote);
    bytes = recvfrom(fd, buf, 1500, 0, (struct sockaddr*)&remote, &addrlen);
    ELOG_DEBUG(" >> Got %d bytes...\n", bytes);
    if(stun_agent_validate (&stun, &msg, buf, bytes, NULL, NULL) != STUN_VALIDATION_SUCCESS) {
      LOG(ERROR) <<  "Failed to validate STUN BINDING response";
      return -1;
    }
    StunClass stun_class = stun_message_get_class(&msg);
    StunMethod method = stun_message_get_method(&msg);
    if(stun_class != STUN_RESPONSE || method != STUN_BINDING) {
      LOG(ERROR) << "Unexpected STUN response: " << stun_class << "/" << method;
      return -1;
    }
    StunMessageReturn ret = stun_message_find_xor_addr(&msg, STUN_ATTRIBUTE_XOR_MAPPED_ADDRESS, (struct sockaddr_storage *)&address, &addrlen);
    VLOG(3) << "  >> XOR-MAPPED-ADDRESS: " << ret;
    if(ret == STUN_MESSAGE_RETURN_SUCCESS) {
      char *public_ip = janus_address_to_ip((struct sockaddr *)&address);
      VLOG(3) << "  >> Our public address is " << public_ip;
      public_ip_ = public_ip;
      g_free(public_ip);
      return 0;
    }
    ret = stun_message_find_addr(&msg, STUN_ATTRIBUTE_MAPPED_ADDRESS, (struct sockaddr_storage *)&address, &addrlen);
    VLOG(3) << "  >> MAPPED-ADDRESS: " << ret;
    if(ret == STUN_MESSAGE_RETURN_SUCCESS) {
      char *public_ip = janus_address_to_ip((struct sockaddr *)&address);
      LOG(INFO) << "  >> Our public address is " << public_ip;
      public_ip_ = public_ip;
      g_free(public_ip);
      return 0;
    }
    return -1;
  }

  namespace net {
    std::string ResolveAddress(const std::string& host_name) {
      gchar *host_name_char = (gchar*)host_name.c_str();
      struct addrinfo *res = NULL;
      if(getaddrinfo(host_name_char, NULL, NULL, &res) != 0) {
        ELOG_DEBUG("Could not resolve %s...\n", host_name_char);
        if(res)
          freeaddrinfo(res);
        return "";
      }
      char* ip = janus_address_to_ip(res->ai_addr);
      std::string ret = "";
      ret += ip;
      delete ip;
      return ret;
    }
  }
} /* namespace orbit */
