/*
 * audio_event_listener.h
 *
 *  Created on: Apr 8, 2016
 *      Author: vellee
 */

#ifndef MODULES_AUDIO_EVENT_LISTENER_H_
#define MODULES_AUDIO_EVENT_LISTENER_H_

namespace orbit {

class IAudioEventListener{
public:
  virtual bool MuteStream(const int stream_id,const bool mute) = 0;
};

}  // namespace orbit

#endif /* MODULES_AUDIO_EVENT_LISTENER_H_ */
