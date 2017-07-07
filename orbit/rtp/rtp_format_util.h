/*
 * rtp_format_util.h
 * 
 *  Created on: Aug 16, 2016
 *      Author: chenteng
 */

#pragma once
#include <memory>

namespace orbit {
  class dataPacket; 
  bool IsValidVideoPacket(const orbit::dataPacket& rtp_packet);
  bool IsKeyFramePacket(std::shared_ptr<orbit::dataPacket> rtp_packet); 

} // end namespace orbit
