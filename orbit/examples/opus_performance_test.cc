/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * opus_performance_test.cc
 * ---------------------------------------------------------------------------
 *  Measure the performance of the opus encoder/decoder.
 * ---------------------------------------------------------------------------
 * Usage command line:
 *  bazel-bin/stream_service/orbit/examples/opus_performance_test \
 *     --replay_file=./audio.pb  --logtostderr
 * ---------------------------------------------------------------------------
 *  Performance result:
 *     Run sequentially.
 *    -------------------------------------------
 *     Decode 20 packets (each with 20ms data):  takes 1-2 ms 
 *     Encode 20 packets (each with 20ms data):  takes 9-13 ms
 * Result from Desktop machine 2.8Ghz 8 Core 
 * 1. No parallelism.
 * --------------------------------------------
 * Encoding time (almost ~20 times)  
 *  Mean=13
 *  95th percentile=14
 *  hardware_concurrency = 8
 * 2. Use use_async=true
 * --------------------------------------------
 * Encoding time (almost ~20 times)  
 *  Mean=10
 *  95th percentile=15
 *  hardware_concurrency=8
 */
#include "stream_service/orbit/debug_server/rtp_capture.h"

#include "stream_service/orbit/base/timeutil.h"
#include "stream_service/orbit/modules/audio_buffer_manager.h"

// For Gflags and Glog
#include "gflags/gflags.h"
#include "glog/logging.h"

#include <memory>
#include <vector>
#include <future>

DEFINE_string(replay_file, "stream_service/orbit/testdata/audio_test_data/audio.pb", "Specifies the replay pb files");
DEFINE_int32(repeat_times, 20, "How many times of decoding the packet.");
DEFINE_bool(use_async, false, "Wheter to use the std::async API to encode packet");
using namespace std;
namespace orbit {
namespace {
}  // namespace annoymous

  int EncodeDataTask(shared_ptr<OpusCodec> opus_codec_, shared_ptr<MediaDataPacket> media_packet) {
  shared_ptr<MediaOutputPacket> media_output_packet(new MediaOutputPacket());
  if (!opus_codec_->EncodePacket(media_packet->media_buf, media_packet->length, media_output_packet)) {
    LOG(ERROR) << "Decode packet failed. Error!";
  }
  return 1;
}

class OpusPerformanceBenchmark {
public:
  OpusPerformanceBenchmark() {
  }

  int ReplayRtpPackets() {
    vector<shared_ptr<OpusCodec> > codecs;
    for (int i = 0; i < FLAGS_repeat_times; ++i) {
      AudioOption* option = NULL;
      codecs.push_back(std::make_shared<orbit::OpusCodec>(option));
    }

    vector<int> time_array;

    std::unique_ptr<RtpReplay> replay(new RtpReplay);
    replay->Init(FLAGS_replay_file);
    int i = 0;
    std::shared_ptr<StoredPacket> packet;
    long ts = 0;

    do {
      packet = replay->Next();
      if (packet != NULL) {
        if (ts != 0) {
          long long now = packet->ts();
          int sleep_time = (int)(now - ts);
          VLOG(2) << "sleep_time=" << sleep_time << " ms";
          if (sleep_time != 0) {
            usleep(sleep_time * 1000);
          }
        }
        ts = packet->ts();
      
        VLOG(2) << "packet_type=" << StoredPacketType_Name(packet->packet_type())
                  << " type=" << StoredMediaType_Name(packet->type())
                  << " length=" << packet->packet_length();

        if (packet->packet_type() == RTP_PACKET && packet->type() == AUDIO) {
          VLOG(2) << "n=" << i;
          ++i;
          unsigned char* buffer =  (unsigned char*)(packet->data().c_str());
          int len = packet->packet_length();
          shared_ptr<MediaDataPacket> media_packet(new MediaDataPacket);
          {
            SimpleClock clock;
            for (int i = 0; i < FLAGS_repeat_times; ++i) {
              shared_ptr<OpusCodec> opus_codec_ = codecs[i];
              if (!opus_codec_->DecodePacket(buffer, len, media_packet)) {
                LOG(ERROR) << "Decode packet failed. Error!";
              }
            }
            //clock.PrintDuration();
          }

          {
            SimpleClock clock;
            if (FLAGS_use_async) {
              std::vector<std::future<int> > res_futures;
              for (int i = 0; i < FLAGS_repeat_times; ++i) {
                shared_ptr<OpusCodec> opus_codec_ = codecs[i];
                //EncodeDataTask(opus_codec_, media_packet);
                //shared_ptr<MediaOutputPacket> media_out_packet;
                auto t = std::async(std::launch::async, EncodeDataTask, opus_codec_, media_packet);
                res_futures.push_back(std::move(t));
              }
              int sum = 0;
              for (int i = 0; i < FLAGS_repeat_times; ++i) {
                sum += res_futures[i].get();
              }
              assert(FLAGS_repeat_times == sum);
            } else {
              for (int i = 0; i < FLAGS_repeat_times; ++i) {
                shared_ptr<OpusCodec> opus_codec_ = codecs[i];
                EncodeDataTask(opus_codec_, media_packet);
              }
            }
            //clock.PrintDuration();
            time_array.push_back(clock.ElapsedTime());
          }
        }
      }
    } while(packet != NULL);
    nth_element(time_array.begin(),
                time_array.begin()+int(time_array.size()*0.5), 
                time_array.end());
    int mean = time_array[time_array.size()/2];
    
    nth_element(time_array.begin(),
                time_array.begin()+int(time_array.size()*0.95), 
                time_array.end());
    int n95percentile = time_array[time_array.size()*0.95];
                           
    LOG(INFO) << "Encoding time (almost ~" << FLAGS_repeat_times << " times) ";
    LOG(INFO) << "Mean=" << mean;
    LOG(INFO) << "95th percentile=" << n95percentile;
    return 0;    
  }
private:

};  // end of class.

}  // namespace orbit
  
int main(int argc, char** argv) {
  google::InstallFailureSignalHandler();
  google::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);

  orbit::OpusPerformanceBenchmark main;
  int ret = main.ReplayRtpPackets();
  LOG(INFO) << "hardware_concurrency = " << thread::hardware_concurrency() << endl;
  return ret;
}
