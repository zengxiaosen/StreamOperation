/*
 * audio_mixer_element.h
 *
 *  Created on: Mar 31, 2016
 *      Author: vellee
 */

#ifndef MODULES_AUDIO_MIXER_ELEMENT_H_
#define MODULES_AUDIO_MIXER_ELEMENT_H_

#include <vector>
#include <thread>
#include <map>
#include <sys/prctl.h>

#include <boost/scoped_ptr.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>

#include "audio_buffer_manager.h"
#include "speaker_estimator.h"
#include "stream_service/orbit/base/thread_util.h"
#include "stream_service/orbit/audio_processing/audio_energy.h"

namespace orbit {
using namespace std;

class IAudioMixerRawListener{
public:
  virtual void OnAudioMixed(const char* outBuffer, int size) = 0;
};
class IAudioMixerRtpPacketListener{
public:
  virtual void OnAudioMixed(const std::shared_ptr<MediaOutputPacket> packet) = 0;
};

class AbstractAudioMixerElement {
 public:
  virtual ~AbstractAudioMixerElement() {}
  virtual bool Start() = 0;
  virtual bool IsStarted() = 0;
  virtual void AddAudioBuffer(int stream_id) = 0;
  virtual void RemoveAudioBuffer(int stream_id) = 0;
  virtual void PushPacket(int stream_id, const dataPacket &packet) = 0;
  virtual void OnPacketLoss(int stream_id, int percent) = 0;
  virtual bool RegisterMixResultListener(int stream_id, IAudioMixerRtpPacketListener *listener) = 0;
  virtual bool UnregisterMixResultListener(int stream_id) = 0;
  virtual bool SetMixAllRtpPacketListener(IAudioMixerRtpPacketListener *listener) = 0;
  virtual bool Mute(const int stream_id, const bool mute) = 0;
};

class AudioMixerElement : public AbstractAudioMixerElement {
public:
  virtual ~AudioMixerElement();
  virtual void AddAudioBuffer(int stream_id) override;
  virtual void RemoveAudioBuffer(int stream_id) override;
  virtual void PushPacket(int stream_id, const dataPacket& packet) override;
  /**
   * When packet loss is changed, we need to set percent of packet loss(from 0 to 100).
   */
  virtual void OnPacketLoss(int stream_id, int percent) override;

  virtual bool RegisterMixResultListener(int stream_id, IAudioMixerRtpPacketListener* event) override;
  virtual bool UnregisterMixResultListener(int stream_id) override;

  virtual bool SetMixAllRtpPacketListener(IAudioMixerRtpPacketListener* listener) override;

  /**
   * @param stream_id : mute stream_id
   * @param mute : when mute is true audio stream will not be mixed.
   */
  virtual bool Mute(const int stream_id,const bool mute) override;

  virtual bool Start() override;
  virtual bool IsStarted() override { return running_; };
private:
  static const opus_int32 ENERGY_LOW = 1000000;
  typedef std::shared_ptr<MediaDataPacket> DataPtr;
  typedef std::map<int32_t, DataPtr> DataMap;

  AudioMixerElement(int32_t session_id,
                    AudioOption* option,
                    IAudioMixerRawListener* mix_all_listener,
                    ISpeakerChangeListener* speaker_change_listener);

  bool Destroy();

  void MixPacketLoopOnStable3();

  void MixPacketLoopWithMultiThread();
  ProductQueue<std::shared_ptr<UnencodedPacket>> unencoded_queue_;
  std::vector<std::shared_ptr<std::thread>> encode_threads_;

  void DecodeLoop();
  void EncodeLoop();
  void HandleDataLoop();
  void HandleData(const std::map<int32_t, DataPtr> &decoded_packet_map);
  /**
   * Normal mixer loop like：
   * Total four user A,B,C,D
   * A = B + C + D;
   * B = A + C + D;
   * C = A + B + D;
   * D = A + B + C；
   */
  void MixPacketLoop();

  /**
   *New mixer loop according to Viktor T.Toch's mixer method
   *@http://www.vttoth.com/CMS/index.php/technicol-notes/68
   *Assuming that A and B have values between 0 and 1):
   *    Z=2AB
   * if A<0.5 and B<0.5,
   *    Z=2(A+B)−2AB−1
   *
   *otherwise.
   *Normalized for values between 0 and 255, the equations look like this:
   *    Z=AB/128,
   *  if A<0.5 and B<0.5,
   *    Z=2(A+B)−AB128−256.
   */
  void MixPacketLoop_New();

  /**
   * Encode mixed packet.
   */
  bool EncodePacket(opus_int16* buffer, int length,
                    std::shared_ptr<MediaOutputPacket> packet) {
    return opus_codec_->EncodePacket(buffer, length, packet);
  }

  const int32_t session_id_;

  bool running_ = false;

  int mixer_samples_;

  // Codecs, Owns by the class
  std::unique_ptr<OpusCodec> opus_codec_;

  // Owns the audio_buffer_managers and the AudioBufferManager instances in the map.
  boost::mutex buffer_manager_mutex_;
  map<int, std::shared_ptr<AudioBufferManager>> audio_buffer_managers_;
  // Owns the speaker_estimator.
  std::unique_ptr<SpeakerEstimator> speaker_estimator_;

  // The mixer thread, owns by this class.
  boost::scoped_ptr<boost::thread> mixer_thread_;
  boost::scoped_ptr<boost::thread> decode_thread_;

  ProductQueue<std::shared_ptr<DataMap>> queue_;

  int16_t seq_ = 0;
  int32_t ts_ = 0;

  // Indicates the mixer processing elapsed time (in ms).
  int mixer_elapsed_time_us_ = 0;

  // All listeners, not own by us.
  boost::mutex mixer_listener_mutex_;
  map<int, IAudioMixerRtpPacketListener*> audio_mixer_listeners_;
  IAudioMixerRawListener* mix_all_listener_ = NULL;
  IAudioMixerRtpPacketListener *mix_all_rtp_packet_listener_ = NULL;

  friend class AudioMixerFactory;
};


class AudioMixerElementOfStable1 : public AbstractAudioMixerElement {
 public:
  virtual ~AudioMixerElementOfStable1();
  virtual bool Start() override;
  virtual bool IsStarted() override;
  virtual void AddAudioBuffer(int stream_id) override;
  virtual void RemoveAudioBuffer(int stream_id) override;
  virtual void PushPacket(int stream_id, const dataPacket &packet) override;
  virtual void OnPacketLoss(int stream_id, int percent) override;
  virtual bool RegisterMixResultListener(int stream_id, IAudioMixerRtpPacketListener *listener) override;
  virtual bool UnregisterMixResultListener(int stream_id) override;
  virtual bool SetMixAllRtpPacketListener(IAudioMixerRtpPacketListener *listener) override;
  virtual bool Mute(const int stream_id, const bool mute) override;
 private:
  AudioMixerElementOfStable1(int32_t session_id, AudioOption *option,
                             IAudioMixerRawListener *mix_all_listener,
                             ISpeakerChangeListener *speaker_change_listener);
  bool Init();
  void MixPacketLoop();
  bool EncodePacket(opus_int16 *buffer, int length,
                    std::shared_ptr<MediaOutputPacket> packet);

  const int32_t session_id_;
  bool running_ = false;
  AudioOption *audio_option_ = nullptr;
  WebRtcOpusEncInst *opus_encoder_ = nullptr;

  boost::mutex buffer_manager_mutex_;
  std::map<int, AudioBufferManagerOfStable1*> audio_buffer_managers_;

  boost::mutex mixer_listener_mutex_;
  std::map<int, IAudioMixerRtpPacketListener*> audio_mixer_listeners_;

  IAudioMixerRawListener *mix_all_listener_ = nullptr;
  IAudioMixerRtpPacketListener *mix_all_rtp_packet_listener_ = nullptr;

  SpeakerEstimator speaker_estimator_;

  boost::scoped_ptr<boost::thread> mixer_thread_;

  friend class AudioMixerFactory;
};

class AudioMixerFactory final {
 public:
  static AbstractAudioMixerElement *Make(
      int32_t session_id, AudioOption *option,
      IAudioMixerRawListener *mix_all_listener,
      ISpeakerChangeListener *speaker_change_listener);
};


} /* namespace orbit */

#endif /* MODULES_AUDIO_MIXER_ELEMENT_H_ */
