/*
 * Copyright © 2016 Orangelab Inc. All Rights Reserved.
 * 
 * Author: qingyong@orangelab.cn (Qingyong Zhang)
 * Created on: Jul 16, 2016
 *
 *
 * audio_mixer_test.cc
 * --------------------------------------------------------
 * Used to test mixer time.
 * Use '--mixer_count' to set pairticipant count.
 * --------------------------------------------------------
 */

#include "audio_mixer_element.h"
#include "stream_service/orbit/webrtc/modules/audio_coding/neteq/tools/audio_loop.h"
#include "stream_service/orbit/webrtc/modules/audio_coding/codecs/opus/opus_interface.h"
#include "stream_service/orbit/webrtc/modules/audio_coding/codecs/opus/opus_inst.h"
#include "stream_service/orbit/base/timeutil.h"

#include "gtest/gtest.h"
#include "glog/logging.h"
#include <memory>

using webrtc::test::AudioLoop;

// Maximum number of bytes in output bitstream.
const size_t kMaxBytes = 1000;
// Sample rate of Opus.
const size_t kOpusRateKhz = 48;
// Number of samples-per-channel in a 20 ms frame, sampled at 48 kHz.
const size_t kOpus20msFrameSamples = kOpusRateKhz * 20;
// Number of samples-per-channel in a 10 ms frame, sampled at 48 kHz.
const size_t kOpus10msFrameSamples = kOpusRateKhz * 10;
namespace orbit {
namespace{

class AudioParticipant: public IAudioMixerRtpPacketListener{
public:
  AudioParticipant(std::shared_ptr<AbstractAudioMixerElement> mixer_element, int id) {
    stream_id = id;
    std::string file_name = "stream_service/orbit/webrtc/modules/audio_coding/testdata/";
    if (channel_ == 1) {
      file_name += "testfile32kHz.pcm";
    } else if (channel_ == 2) {
      file_name += "teststereo32kHz.pcm";
    }

    opus_codec_.reset(new OpusCodec(new AudioOption()));
    EXPECT_TRUE(speech_data_.Init(file_name,
                                  200 * kOpusRateKhz * channel_,
                                  20 * kOpusRateKhz * channel_));
    mixer_element_ = mixer_element;
    mixer_element->AddAudioBuffer(id);

    mixer_element->RegisterMixResultListener(id, this);
    thread.reset(new std::thread([this]{
      this->PacketGeneratorLoop();
    }));
  }
  void OnAudioMixed(const std::shared_ptr<MediaOutputPacket> packet) {
    VLOG(2)<<"Audio mixer"<<stream_id;
  }
  void PacketGeneratorLoop() {
    while(true) {
      packets ++;
      if(packets > 1000) {
        break;
      }
      const rtc::ArrayView<const int16_t> array_view =  speech_data_.GetNextBlock();
      const int16_t* data =  array_view.data();
      int size = array_view.size();
      int count =  rtc::CheckedDivExact(size, channel_);

      std::shared_ptr<MediaOutputPacket> packet = std::make_shared<MediaOutputPacket>();

      bool encode = opus_codec_->EncodePacket(data, size, packet);
      VLOG(2)<<"Encode is "<<encode<< " size is "<<packet->length;
      usleep(15000);
    }
  }
  void release(){
    thread->join();
  }
private :
  int packets = 0;
  int channel_ = 1;
  int stream_id ;

  /**
   * pcm generator.
   */
  AudioLoop speech_data_;

  std::shared_ptr<AbstractAudioMixerElement> mixer_element_;
  std::unique_ptr<OpusCodec> opus_codec_;
  std::unique_ptr<std::thread> thread;

};

class AudioMixerTest : public IAudioMixerRawListener,
                       public ISpeakerChangeListener {
 public:
  AudioMixerTest() {
    SetUp();
  }

  ~AudioMixerTest() {
    TearDown();
  }

  int run(int mixer_count) {
    user_count_ = mixer_count;
    AudioParticipant* participants[mixer_count];
    for(int i = 0; i<mixer_count; i++) {
      participants[i] =new AudioParticipant(element, i);
    }
    for(AudioParticipant* participant : participants ) {
      participant->release();
    }
    return 1;
  }

private :
  int user_count_;
  std::shared_ptr<AbstractAudioMixerElement> element;

  void SetUp() {
    element.reset(AudioMixerFactory::Make(1, 
                                          new AudioOption(),
                                          (IAudioMixerRawListener*)this,
                                          (ISpeakerChangeListener*)this));
  }

  void TearDown() {
  }

  void OnAudioMixed(const char* outBuffer, int size){
    LOG(INFO) << "[USER_SIZE : " << user_count_ << "]";
  }

  void OnSpeakerChanged(int speaker_id) {
  }

};

}  // annoymous namespace
}  // namespace orbit
