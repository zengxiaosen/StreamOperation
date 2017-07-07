/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * sdp_info.h
 * ---------------------------------------------------------------------------
 * Defines the class and interface to process the sdp message.
 * ---------------------------------------------------------------------------
 * Reference implementation: SdpInfo.h (Erizo)
 */

#ifndef SDP_INFO_H__
#define SDP_INFO_H__

#include <string>
#include <sstream>
#include <vector>
#include <map>

#include "glog/logging.h"

#include "stream_service/orbit/common_def.h"

namespace orbit {

/**
 * ICE candidate types
 */
enum HostType {
    HOST, SRFLX, PRFLX, RELAY
};
/**
 * Channel types
 */
enum MediaType {
    VIDEO_TYPE, AUDIO_TYPE, OTHER
};
/** 
 * Stream directions
 */
enum StreamDirection {
  SENDRECV, SENDONLY, RECVONLY
};
/**
 * RTP Profile
 */
enum Profile {
  AVPF, SAVPF
};
/**
 * SRTP info.
 */
class CryptoInfo {
public:
    CryptoInfo() :
            tag(0) {
    }
    /**
     * tag number
     */
    int tag;
    /**
     * The cipher suite. Only AES_CM_128_HMAC_SHA1_80 is supported as of now.
     */
    std::string cipherSuite;
    /**
     * The key
     */
    std::string keyParams;
    /**
     * The MediaType
     */
    MediaType mediaType;
};
/**
 * Contains the information of an ICE Candidate
 */
class CandidateInfo {
public:
    CandidateInfo() :
            tag(0) {
    }
    bool isBundle;
    int tag;
    unsigned int priority;
    unsigned int componentId;
    std::string foundation;
    std::string hostAddress;
    std::string rAddress;
    int hostPort;
    int rPort;
    std::string netProtocol;
    HostType hostType;
    std::string transProtocol;
    std::string username;
    std::string password;
    MediaType mediaType;

    std::string ToDebugString() const {
      std::string hostType_str;
      switch (hostType) {
      case HOST:
        hostType_str = "host";
        break;
      case SRFLX:
        hostType_str = "srflx";
        break;
      case PRFLX:
        hostType_str = "prflx";
        break;
      case RELAY:
        hostType_str = "relay";
        break;
      default:
        hostType_str = "host";
        break;
      }
      std::ostringstream cand;
      cand << "a=candidate:" << foundation << " " << componentId
           << " " << netProtocol << " " << priority << " "
           << hostAddress << " " << hostPort << " typ "
           << hostType_str;

      if (hostType == SRFLX || hostType == RELAY) {
        //raddr 192.168.0.12 rport 50483
        cand << " raddr " << rAddress << " rport " << rPort;
      }
      return cand.str();
    }

    void FromString(const std::string &s) {
      static const std::string::size_type SDP_HEAD_LEN = 12; // length of "a=candidate:"
      std::string tmps = s.substr(SDP_HEAD_LEN);

      std::string placeholder;
      std::string   hostType_str;

      std::istringstream ss(tmps);
      ss >> foundation  >> componentId >> netProtocol
         >> priority    >> hostAddress >> hostPort
         >> placeholder >> hostType_str;

      if (hostType_str == "host") {
        hostType = HOST;
      } else if (hostType_str == "srflx") {
        hostType = SRFLX;
      } else if (hostType_str == "prflx") {
        hostType = PRFLX;
      } else if (hostType_str == "relay") {
        hostType = RELAY;
      } else {
        hostType = HOST;
      }

      if (hostType == SRFLX || hostType == RELAY) {
        ss >> placeholder >> rAddress
           >> placeholder >> rPort;
      }
    }

    std::string ToString() const {
      std::string generation = " generation 0";
      std::string hostType_str;
      switch (hostType) {
        case HOST:
          hostType_str = "host";
          break;
        case SRFLX:
          hostType_str = "srflx";
          break;
        case PRFLX:
          hostType_str = "prflx";
          break;
        case RELAY:
          hostType_str = "relay";
          break;
        default:
          hostType_str = "host";
          break;
      }

      std::ostringstream ss;
      ss << "a=candidate:";
      ss << foundation  << " " << componentId << " "
         << netProtocol << " " << priority    << " "
         << hostAddress << " " << hostPort    << " typ "
         << hostType_str;

      if (hostType == SRFLX || hostType == RELAY) {
        //raddr 192.168.0.12 rport 50483
        ss << " raddr " << rAddress << " rport " << rPort;
      }

      ss << generation;
      return ss.str();
    }
};

/**
 * A bundle tag
 */
class BundleTag {
  public:
    BundleTag(std::string theId, MediaType theType):id(theId),mediaType(theType){
    };
    std::string id;
    MediaType mediaType;
};

/**
 * A PT to Codec map
 */
struct RtpMap {
  unsigned int payloadType;
  std::string encodingName;
  unsigned int clockRate;
  MediaType mediaType;
  unsigned int channels;
  std::vector<std::string> feedbackTypes;
  std::map<std::string, std::string> formatParameters;
};

// Constant variables.
const int kTargetBandwidthKbps = 250;
const int kDefaultMinBitrateKbps = 120;

/**
 * Contains the information of a single SDP.
 * Used to parse and generate SDPs
 */
class SdpInfo {
public:
  // ------------------------- methods ---------------------
    /**
     * Constructor
     */
    SdpInfo();
    virtual ~SdpInfo();
    /**
     * Inits the object with a given SDP.
     * @param sdp An string with the SDP.
     * @return true if success
     */
    bool initWithSdp(const std::string& sdp, const std::string& media);
    /**
     * Adds a new candidate.
     * @param info The CandidateInfo containing the new candidate
     */
    std::string addCandidate(const CandidateInfo& info);
    /**
     * Adds SRTP info.
     * @param info The CryptoInfo containing the information.
     */
    void addCrypto(const CryptoInfo& info);
    /**
     * Gets the candidates.
     * @return A vector containing the current candidates.
     */
    std::vector<CandidateInfo>& getCandidateInfos();

    void copyCandidateInfos(std::vector<CandidateInfo> *vec) const;
    /**
     * Gets the SRTP information.
     * @return A vector containing the CryptoInfo objects with the SRTP information.
     */
    std::vector<CryptoInfo>& getCryptoInfos();
    /**
    * Gets the payloadType information
    * @return A vector containing the PT-codec information
    */
    const std::vector<RtpMap>& getPayloadInfos();
    /**
     * Gets the actual SDP.
     * @return The SDP in string format.
     */
    std::string getSdp();
    /**
     * @brief map external payload type to an internal id
     * @param externalPT The audio payload type as coming from this source
     * @return The internal payload id
     */
    int getAudioInternalPT(int externalPT);
    /**
     * @brief map external payload type to an internal id
     * @param externalPT The video payload type as coming from this source
     * @return The internal payload id
     */
    int getVideoInternalPT(int externalPT);
    /**
     * @brief map internal payload id to an external payload type
     * @param internalPT The payload type id used internally
     * @return The external payload type as provided to this source
     */
    int getAudioExternalPT(int internalPT);
    /**
     * @brief map internal payload it to an external payload type
     * @param internalPT The payload id as used internally
     * @return The external video payload type
     */
    int getVideoExternalPT(int internalPT);

    void setCredentials(const std::string& username, const std::string& password, MediaType media);

    void getCredentials(std::string& username, std::string& password, MediaType media);

    RtpMap* getCodecByName(const std::string codecName, const unsigned int clockRate);

    bool supportCodecByName(const std::string codecName, const unsigned int clockRate);

    bool supportPayloadType(const int payloadType);

    void createOfferSdp();
    /**
     * @brief copies relevant information from the offer sdp for which this will be an answer sdp
     * @param offerSdp The offer SDP as received via signaling and parsed
     */
    void setOfferSdp(const SdpInfo& offerSdp);

    // Getter/setter function for private variable: isFingerprint
    void setIsFingerprint(bool is_fingerprint) {
      isFingerprint = is_fingerprint;
    }
    bool getIsFingerprint() {
      return isFingerprint;
    }

    // Getter/setter function for private variable: fingerprint
    std::string getFingerprint() {
      return fingerprint;
    }
    void setFingerprint(std::string _fingerprint) {
      fingerprint = _fingerprint;
    }

    // Getter/setter function for private variable: audioSsrc, videoSsrc, videoRtxSsrc
    void setAudioSsrc(unsigned int _audioSsrc) {
      audioSsrc = _audioSsrc;
    }
    unsigned int getAudioSsrc() const {
      return audioSsrc;
    }

    void setVideoSsrc(unsigned int _videoSsrc) {
      videoSsrc = _videoSsrc;
    }

    unsigned int getVideoSsrc() const {
      return videoSsrc;
    }

    std::vector<unsigned int> getExtendedVideoSsrcs() {
      return extended_video_ssrc;
    }

    void initExtendedVideoSsrc(unsigned int number) {
      while(extended_video_ssrc.size() < number) {
        extended_video_ssrc.push_back(rand());
      }
    }

    void setVideoRtxSsrc(unsigned int _videoRtxSsrc) {
      videoRtxSsrc = _videoRtxSsrc;
    }

    unsigned int getVideoRtxSsrc() {
      return videoRtxSsrc;
    }

    // Getter/setter function for private variable: isBundle
    void setIsBundle(bool is_bundle) {
      isBundle = is_bundle;
    }

    bool getIsBundle() {
      return isBundle;
    }
    // Getter/setter function for private variable: hasAudio
    void setHasAudio(bool _hasAudio) {
      hasAudio = _hasAudio;
    }

    bool getHasAudio() {
      return hasAudio;
    }

    // Getter/setter function for private variable: hasVideo
    void setHasVideo(bool _hasVideo) {
      hasVideo = _hasVideo;
    }

    bool getHasVideo() {
      return hasVideo;
    }

    // Getter/setter function for private variable: profile
    Profile getProfile() {
      return profile;
    }

    void setProfile(Profile _profile) {
      profile = _profile;
    }
    
    // Getter function for private variable: isRtcpMux
    bool getIsRtcpMux() {
      return isRtcpMux;
    }
    // Getter function for private variable: videoSdpMLine
    bool getVideoSdpMLine() {
      return videoSdpMLine;
    }
    // Getter function for private variable: audioSdpMLine
    bool getAudioSdpMLine() {
      return audioSdpMLine;
    }

    unsigned int get_target_bandwidth() {
      return target_bandwidth_;
    }

    void set_target_bandwidth(unsigned int target_bwe) {
      target_bandwidth_ = target_bwe;
    }

    unsigned int get_min_bitrate() {
      return min_bitrate_;
    }

    void set_min_bitrate(unsigned int min_bitrate) {
      min_bitrate_ = min_bitrate;
    }

    void set_video_encoding(olive::VideoEncoding encoding) {
      video_encoding_ = encoding;
      internalPayloadVector_.clear();
      InitializeRtpMap();
    }

    std::string get_setup() {
      return setup_;
    }

    void set_setup(std::string setup) {
      setup_ = setup;
    }

    void AddVideoBundleTag() {
      bundleTags.push_back(orbit::BundleTag("video", VIDEO_TYPE));
    }
    void AddAudioBundleTag() {
      bundleTags.push_back(orbit::BundleTag("audio", AUDIO_TYPE));
    }

    StreamDirection GetVideoDirection() {
      return videoDirection;
    }

    int GetCodec(int source_codec) {
      int result = outIn_payload_[source_codec];
      if (result == 0) {
        return source_codec;
      } else {
        return result;
      }
    }

    // Extract the video number from client's sdp.
    int GetCodesNumber(const std::string& offer, const std::string& reg);
    void ExtractNewCodes(const std::string& offer);

private:
    // Initialize the RTPMap and payload vector.
    void InitializeRtpMap();

    // ------------------------- private methods ---------------------    
    bool processSdp(const std::string& sdp, const std::string& media);
    bool processCandidate(std::vector<std::string>& pieces, MediaType mediaType);
    std::string stringifyCandidate(const CandidateInfo & candidate);
    void gen_random(char* s, int len);

    // ------------------------- private properties ---------------------    
    // All the properties
    std::vector<CandidateInfo> candidateVector_;
    std::vector<CryptoInfo> cryptoVector_;
    std::vector<RtpMap> internalPayloadVector_;
    std::string iceVideoUsername_, iceAudioUsername_;
    std::string iceVideoPassword_, iceAudioPassword_;

    /**
     * The audio and video SSRCs for this particular SDP.
     */
    unsigned int audioSsrc; 
    unsigned int videoSsrc;
    unsigned int videoRtxSsrc;
    std::vector<unsigned int> extended_video_ssrc;

    /**
    * Is it Bundle
    */
    bool isBundle;
    /**
    * Has audio
    */
    bool hasAudio;
    /**
    * Has video
    */
    bool hasVideo;
    /**
    * Is there rtcp muxing
    */
    bool isRtcpMux;
    /**
    * Is there extmap enabled for ssrc-audio-level.
    */
    bool isExtMapEnabled;
    /**
    * Is RED/FEC codec enabled in the sdp.
    */    
    bool isRedFecEnabled;

    StreamDirection videoDirection, audioDirection;
    /**
    * RTP Profile type
    */
    Profile profile;
    /**
    * Is there DTLS fingerprint
    */
    bool isFingerprint;
    /**
    * DTLS Fingerprint
    */
    std::string fingerprint;
    /**
     * Setup (actpass/passive/active)
     */
    std::string setup_;    
    /**
    * Mapping from internal PT (key) to external PT (value)
    */
    std::map<int, int> inOutPTMap;
    /**
    * Mapping from external PT (key) to intermal PT (value)
    */
    std::map<int, int> outInPTMap;
    /**
     * The negotiated payload list
     */
    std::vector<RtpMap> payloadVector;
    std::vector<BundleTag> bundleTags;

    /*
     * MLines for video and audio
     */
    int videoSdpMLine;
    int audioSdpMLine;
    int videoCodecs, audioCodecs;

    /*
     * Bandwidth setting.
     */
    // The videoBandwidth of the parsed offer
    unsigned int videoBandwidth;
    // The default video_bandwidth of the stream
    unsigned int target_bandwidth_ = kTargetBandwidthKbps;

    /*
     * Min bitrate for codec.
     */
    unsigned int min_bitrate_ = kDefaultMinBitrateKbps;

    /*
     * The preferred video encoding algorithm: VP8, VP9 and H264.
     */
    olive::VideoEncoding video_encoding_;

    /**
     * Mapping from entry PT (key) to out PT (value)
     */
    std::map<int, int> inOut_payload_;
    /**
     * Mapping from out PT to entry PT (value)
     */
    std::map<int, int> outIn_payload_;
};
}/* namespace orbit */

#endif /* SDP_INFO_H__ */
