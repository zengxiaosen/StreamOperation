/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * transport_plugin.h
 * ---------------------------------------------------------------------------
 * Defines the abstract interface for transport plugin.
 * ---------------------------------------------------------------------------
 */

#ifndef TRANSPORT_PLUGIN_H__
#define TRANSPORT_PLUGIN_H__

#include "media_definitions.h"
#include "transport_delegate.h"
#include "transport.h"
#include <boost/thread/mutex.hpp>
#include <memory>
namespace orbit {
  //class TransportDelegate;
  class TransportPluginInterface {
  public:
    virtual void IncomingRtpPacket(const dataPacket& packet) = 0;
    virtual void IncomingRtcpPacket(const dataPacket& packet) = 0;
    virtual void RelayRtpPacket(const dataPacket& packet) = 0;
    virtual void RelayRtcpPacket(const dataPacket& packet) = 0;
  };

  class TransportPlugin : public TransportPluginInterface {
  public:
    //TransportPlugin(TransportDelegate* gateway) {
    //  gateway_.reset(gateway);
    //}
    TransportPlugin() {
      active_ = false;
    }
    virtual ~TransportPlugin() {
    	LOG(INFO) <<"Enter into ~TransportPlugin()";
    }

    void SetGateway(TransportDelegate* gateway) {
      gateway_ = gateway;
    }

    TransportDelegate* GetGateway() {
		  return gateway_;
	  }

    bool active() {
      return active_;
    }

    void set_active(bool active) {
      active_ = active;
    }
    
    /**
     * while ice connected or failed, callback this function.
     * @param status true means connect successfully, or failed
     */
    virtual void OnTransportStateChange(TransportState state) {}

    virtual void IncomingRtpPacket(const dataPacket& packet) override;
    virtual void IncomingRtcpPacket(const dataPacket& packet) override;
    virtual void RelayRtpPacket(const dataPacket& packet) override;
    virtual void RelayRtcpPacket(const dataPacket& packet) override;
    void Stop();

    virtual int GetExtendedVideoSsrcNumber() {
      return 0;
    }
    virtual unsigned int GetDownstreamSsrc() {
      return downstream_ssrc_;
    }
    virtual void SetDownstreamSsrc(unsigned int ssrc) {
      downstream_ssrc_ = ssrc;
    }
    virtual unsigned int GetUpstreamSsrc() {
      return upstream_ssrc_;
    }
    virtual void SetUpstreamSsrc(unsigned int ssrc) {
      upstream_ssrc_ = ssrc;
    }
    virtual void SetExtendedVideoSsrcs(std::vector<unsigned int> extended_video_ssrcs) {
      extended_video_ssrcs_ = extended_video_ssrcs;
    }
    virtual std::vector<unsigned int> GetExtendedVideoSsrcs() {
      return extended_video_ssrcs_;
    }
  protected:
    boost::mutex gateway_mutex_;
    TransportDelegate* gateway_;
    bool active_;
    // The video SSRCs of the plugin(room participant).
    // Typicially a plugin (i.e. one participant) will only based on the
    // assumption that it will have and only have a downstream video and a
    // upstream video by default. But in some special type of Room and Plugins
    // for example as ClassRoom, it will have some additional downstream video
    // as a multiple stream combination in the single peer connection.
    unsigned int downstream_ssrc_ = 0;
    unsigned int upstream_ssrc_ = 0;
    std::vector<unsigned int> extended_video_ssrcs_;
  };

  class Room {
    // All the participants, as a plugin
  public:
    Room() {
    }
    virtual ~Room() {
    }
    virtual void Create() {
    }
    virtual void Destroy() {
    }
    virtual void Start() {
    }
    virtual void SetupLiveStream(bool support, bool need_return_video, const char* rtmp_location) {};

    virtual void IncomingRtpPacket(const int stream_id, const dataPacket& packet) {};
    virtual void IncomingRtcpPacket(const int stream_id, const dataPacket& packet) {};

    virtual void OnTransportStateChange(const int stream_id, const TransportState state) {};
    virtual void AddParticipant(TransportPlugin* plugin) {
      boost::mutex::scoped_lock lock(room_plugin_mutex_);
      plugins_.push_back(plugin);
    }
    virtual std::vector<TransportPlugin*> GetParticipants() {
      return plugins_;
    }
    virtual int GetParticipantCount() {
      return plugins_.size();
    }
    virtual bool RemoveParticipant(TransportPlugin* plugin) {
      for (auto it = plugins_.begin(); it != plugins_.end(); ++it) {
        if (*it == plugin) {
          plugin->set_active(false);
          plugins_.erase(it);
          break;
        }
      }
      return true;
    }
    virtual bool ProcessMessage(int stream_id, int message_type, void *data) {
      return false;
    }  
  protected:
    std::vector<TransportPlugin*> plugins_;
    boost::mutex room_plugin_mutex_;
  };


 class SimpleEchoPlugin : public TransportPlugin {
 public:
   void IncomingRtpPacket(const dataPacket& packet);
   void IncomingRtcpPacket(const dataPacket& packet);
};

}  // namespace orbit

#endif  // TRANSPORT_PLUGIN_H__
