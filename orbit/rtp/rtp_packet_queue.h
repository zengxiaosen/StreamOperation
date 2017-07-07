/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * rtp_packet_queue.h
 * ---------------------------------------------------------------------------
 * Defines the interface to implement the rtp_packet_queue
 * A priority queue based on the sorting mertics of the timestamp of the
 * rtp packet.
 * ---------------------------------------------------------------------------
 */

#ifndef RTP_PACKET_QUEUE_H__
#define RTP_PACKET_QUEUE_H__

#include <list>
#include <memory>
#include <boost/thread/mutex.hpp>

using std::shared_ptr;
namespace orbit {
  
//forward declaration
class dataPacket;

static const double DEFAULT_DEPTH = 3.0;
static const double DEFAULT_MAX = 5.0;

// This class implements a packet reordering queue. Here's what it does:
//
// 1. Receives incoming packets and pushes them into a queue based on sequence number
// 2. Rejects duplicate packets--duplicate sequence numbers are dropped on the floor
// 3. Handles sequence number wrap (e.g. packet "1" is technically greater than "65535" because that
//    is a sequence number wrap
// 4. Handles out of order packets
// 5. Handles late packets.  Packets with sequence number greater than the last sequence number
//    handed out through popPacket are discarded.  There's a log message to help identify
//    a sane value for their queue depth.
// 6. Is threadsafe.  All public methods lock to ensure the container isn't fouled by multithreaded
//    access.  This also prevents a minimal amount of locking in calling classes, which prevents
//    blocking of worker threads.
// 7. Manages queue depth.  It won't return data until depth (which is % of seconds) is attained, and
//    will prevent the queue from growing over max seconds.
//
// Usage is straight-forward:
//
// 1. instantiate and push incoming data with pushPacket().
// 2. check if data is ready to be popped by calling hasData().
// 3. pop data with popPacket().  popPacket returns a null shared_ptr if nothing is available.
// 4. popPacket() can be called with an override to drain the queue regardless of the current depth.
//
 
class RtpPacketQueue {
public:
    RtpPacketQueue(double depthInSeconds = DEFAULT_DEPTH,
                   double maxDepthInSeconds = DEFAULT_MAX );
    ~RtpPacketQueue(void);
    void setTimebase(unsigned int timebase);
    void pushPacket(const char *data, int length);
    /**
     * @param ignore_old_packet: If true will check next packet's seq if is smaller than before or not.
     * @auth QingyongZhang
     */
    std::shared_ptr<dataPacket> popPacket(bool ignore_depth = false, bool ignore_old_packet = false);
    std::shared_ptr<dataPacket> getNextPacket(bool ignore_depth = false);
    int getSize();  // total size of all items in the queue
    bool hasData(); // whether or not current queue depth is >= depth_
    bool IsEmpty(); //Whether or not has data;
private:
    // Only used internally; does the math to calculate our current depth based on the supplied timebase.
    // Must be called with queueMutex_ locked.
    double getDepthInSeconds();

    boost::mutex queueMutex_;
    std::list<std::shared_ptr<dataPacket> > queue_;
    int lastSequenceNumberGiven_;
    bool rtpSequenceLessThan(uint16_t x, uint16_t y);

    // We use a timebase so we can understand how many seconds of data we have in our queue.
    unsigned int timebase_;
    double depthInSeconds_;
    double maxDepthInSeconds_;
};

}  // namespace orbit
#endif  // RTP_PACKET_QUEUE_H__
