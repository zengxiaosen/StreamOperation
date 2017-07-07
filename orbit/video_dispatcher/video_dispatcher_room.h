/*
 * video_dispatcher_room.h
 *
 *  Created on: Mar 23, 2016
 *      Author: vellee
 */

#ifndef VIDEO_DISPATCHER_VIDEO_DISPATCHER_ROOM_H_
#define VIDEO_DISPATCHER_VIDEO_DISPATCHER_ROOM_H_

#include "stream_service/orbit/transport_plugin.h"
#include "stream_service/orbit/live_stream/live_stream_processor.h"
#include "stream_service/orbit/live_stream/live_stream_processor_impl.h"
#include <vector>

#define MAX_DISPATCHER_PARTICIPANT_COUNT 100

namespace orbit {
  class VideoDispatcherRoom : public Room {
   public:
     VideoDispatcherRoom();
     ~VideoDispatcherRoom() {
       Destroy();
     }

     void Create();
     void Destroy();
     void Start();
     void SetupLiveStream(bool support, bool need_return_video,
                          const char* live_location) override;
     void AddParticipant(TransportPlugin* plugin) override;
     bool RemoveParticipant(TransportPlugin* plugin) override;
     bool ProcessMessage(int stream_id, int message_type, void *data);

     // custom 
     void SwitchPublisher(int publisher_id);
     void SendFirPacketToViewer();
     void RequestFirPacket(const int stream_id);

   private:
     bool running_;

     int publisher_stream_id_;
     int current_viewer_;

     int32_t fir_seq_[MAX_DISPATCHER_PARTICIPANT_COUNT];
     long long last_fir_time_{0};
     
     // The thread to do the SFU stuff.
     void SelectiveVideoForward();
     void SendFirPacket(int viewer, const std::vector<TransportPlugin*>& participants );

     // Thread to mix the packets from different participants.
     bool use_webcast_ = false;
     LiveStreamProcessor *live_stream_processor_;
     boost::scoped_ptr<boost::thread> selective_forward_thread_;

   };
}  // namespace orbit

#endif /* VIDEO_DISPATCHER_VIDEO_DISPATCHER_ROOM_H_ */
