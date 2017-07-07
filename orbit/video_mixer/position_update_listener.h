/*
 * position_update_listener.h
 *
 *  Created on: Mar 21, 2016
 *      Author: vellee
 */

#ifndef VIDEO_MIXER_INTERFACE_POSITION_UPDATE_LISTENER_H_
#define VIDEO_MIXER_INTERFACE_POSITION_UPDATE_LISTENER_H_



class PositionUpdateListener {
public:
  virtual void OnPositionUpdated() = 0;
};

#endif /* VIDEO_MIXER_INTERFACE_POSITION_UPDATE_LISTENER_H_ */
