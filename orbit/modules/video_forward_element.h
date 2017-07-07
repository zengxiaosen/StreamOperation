/*
 * Copyright 2016 (C) Orange Labs Inc. All Rights Reserved.
 * Author: qingyong@orangelab.cn (Qingyong Zhang)
 *         cheng@orangelab.cn (Cheng Xu)
 *         caixinyu@orangelab.cn (Xinyu Cai)
 *
 * VideoForwardElement.h
 * --------------------------------------------------------
 *  Defines and implements a SFU algorithm for video forwarding unit and system
 *  in Orbit. The VideoForward is an element to receive all the media streams
 *  from every participants, and then select one or few streams from them as
 *  forwarding stream to send to (usually) all of them separately.
 * 
 * Glossy of SFU:
 *  SFU stands for Selective Forwarding Unit. It is a technique used in
 *  multi-party conference system. This video routing technique supports
 *  receving multiple party's media stream and select one of the stream
 *  to forward to all the parties. An SFU is capable of receiving multiple
 *  media streams and then decide which of these media streams should be
 *  sent to which participants.
 * --------------------------------------------------------
 */

#ifndef MODULES_VIDEO_FORWARD_ELEMENT_H_
#define MODULES_VIDEO_FORWARD_ELEMENT_H_

#include <boost/scoped_ptr.hpp>
#include <boost/thread.hpp>

#include "speaker_estimator.h"
#include "rtp_packet_buffer.h"

#include "stream_service/orbit/media_definitions.h"
#include "stream_service/orbit/modules/media_packet.h"
#include "stream_service/orbit/base/timeutil.h"
#include "stream_service/orbit/rtp/rtp_headers.h"

#include "video_map.h"
#include "video_frame_queue.h"

#include <memory>
#include <mutex>
#include <thread>
#include <atomic>

namespace orbit {

class IVideoForwardEventListener{
public:
  virtual void OnRelayRtcpPacket(int stream_id, const dataPacket& packet) = 0;
  /**
   * @param stream_id: plugin's stream_id biger than zero
   *
   * when stream_id == 0, we regard it as a total packet for live stream and so on.
   */
  virtual void OnRelayRtpPacket(int stream_id, const std::shared_ptr<MediaOutputPacket> packet) = 0;
};

class IVideoPacketListener{
public:
  virtual void OnIncomingPacket(const int stream_id, const dataPacket& packet) = 0;
};

struct VideoSwitchContext {
  /* RTP seq and ts */
  uint16_t seq = 0;
  uint32_t ts = 0;

  /* The context data of source stream */
  int stream_id = -1;
  uint32_t last_ts = 0;
  uint32_t base_ts = -1;
  uint16_t last_seq = 0;
  uint16_t base_seq = -1;

  /* Changing status */
  bool changing_ = false;
  int new_stream_id = -1;

  std::string DebugString() {
    std::ostringstream oss;
    oss << "seq=" << seq << " ts=" << ts << " stream_id=" << stream_id
        << "last_ts=" << last_ts << " base_ts=" << base_ts
        << " last_seq=" << last_seq << " base_seq=" << base_seq
        << " changing_=" << changing_ << " new_stream_id=" << new_stream_id
        << std::endl;
    return oss.str();
  }
};

class FirSender {
 public:
  FirSender(IVideoForwardEventListener *video_forward_event_listener)
   : video_forward_event_listener_(video_forward_event_listener) {}
  void SendFir(int32_t stream_id);
 private:
  mutable std::mutex mutex_;
  std::map<int32_t, int> firseq_map_;
  IVideoForwardEventListener *video_forward_event_listener_;
};

class AbstractVideoForwardElement : public IVideoPacketListener {
 public:
  virtual ~AbstractVideoForwardElement() {}
  virtual void LinkVideoStream(int stream_id) = 0;
  virtual void UnlinkVideoStream(int stream_id) = 0;
  virtual void ChangeToSpeaker(int stream_id) = 0;
  virtual void RequestSpeakerKeyFrame() = 0;
};

class VideoForwardElement : public AbstractVideoForwardElement {
public:
  VideoForwardElement(IVideoForwardEventListener* video_forward_event_listener);
  virtual ~VideoForwardElement();

  // IVideoPacketListener - when the incoming packets are coming.
  void OnIncomingPacket(const int stream_id, const dataPacket& packet) override;

  /**
   * Link stream to current element.
   */
  virtual void LinkVideoStream(int stream_id) override;
  /**
   * Unlink stream to current element.
   */
  virtual void UnlinkVideoStream(int stream_id) override;

  /**
   * Used for raise hand.
   * If called current element will always show stream_id's video.
   */
  virtual void ChangeToSpeaker(int stream_id) override;

  virtual void RequestSpeakerKeyFrame() override;
  //std::shared_ptr<ISpeakerChangeListener> GetSpeakerChangeListener();

private:
  void SelectiveVideoForwardLoop();
  void SwitchPublisher(int stream_id);
  void SendFirPacket(int stream_id);

  void ChangeContext(std::shared_ptr<VideoSwitchContext> context, int source_id);

  std::shared_ptr<VideoAwareFrameBuffer> GetVideoQueue(int new_viewer);
  bool HasKeyFrameArrive(int new_viewer_id);
  std::shared_ptr<orbit::dataPacket> PopFromBuffer(int viewer_id, bool pop_from_queue);

  IVideoForwardEventListener* video_forward_event_listener_;

  boost::scoped_ptr<boost::thread> selective_forward_thread_;

  map<int,int> key_frame_seq_;
  bool changing_ = false;
  int current_viewer_ = -1;
  int second_viewer_ = -1;
  int new_viewer_ = -1;
  std::deque<int> request_key_frame_stream_ids_;
  //int request_key_frame_;
  int running_;
  long last_switch_time_ = 0;
  std::mutex video_queue_mutex_;
  map<int, std::shared_ptr<VideoAwareFrameBuffer>> video_queues_;
  map<int, std::shared_ptr<VideoSwitchContext>> video_switch_contexts_;
};

// The New VideoForwardElement, implements the AbstractVideoForwardElement interface.
class VideoForwardElementNew : public AbstractVideoForwardElement {
 public:
  VideoForwardElementNew(IVideoForwardEventListener *video_forward_event_listener);
  virtual ~VideoForwardElementNew();
  virtual void OnIncomingPacket(const int stream_id, const dataPacket &packet) override;
  virtual void LinkVideoStream(int stream_id) override;
  virtual void UnlinkVideoStream(int stream_id) override;
  virtual void ChangeToSpeaker(int stream_id) override;
  virtual void RequestSpeakerKeyFrame() override;
 private:
  static const long VIDEO_FORWARD_THREAD_SLEEP_MS = 1;

  bool Contains(int stream_id) const;
  void ForwardLoop();
  void ForwardData();

  typedef std::recursive_mutex Mutex;
  typedef std::lock_guard<Mutex> Lock;
  mutable Mutex mutex_;
  FirSender *fir_sender_ = nullptr;
  IVideoForwardEventListener *video_forward_event_listener_ = nullptr;
  std::map<int, std::shared_ptr<VideoFrameQueue>> queue_map_;
  VideoMap *video_mapper_ = nullptr;
  std::map<int, std::shared_ptr<VideoSwitchContext>> video_switch_contexts_;
  std::thread forward_loop_;
  bool running_ = true;
};

} /* namespace orbit */

#endif /* MODULES_VIDEO_FORWARD_ELEMENT_H_ */
