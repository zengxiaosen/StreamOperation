/*
 * Copyright 2016 All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 * 
 * sdp_info.cc
 *
 * NOTE(chengxu): the code is highly influenced by the Licode Erizo
 */

#include <sstream>
#include <stdio.h>
#include <cstdlib>
#include <cstring>
#include <boost/regex.hpp>

#include "rtp/rtp_headers.h"
#include "sdp_info.h"
#include "stream_service/orbit/base/strutil.h"

#include "gflags/gflags.h"

DECLARE_bool(enable_red_fec);
DEFINE_bool(set_min_bitrate, true,
            "Set to true if we want to restrict the min bitrate for webrtc "
            "video codec.");

DEFINE_bool(audio_nack, true,
            "If set, NACK will be used for audio transmition.");

using std::endl;
namespace orbit {
  namespace stringutil {
    std::vector<std::string> splitOneOf(const std::string& str,
                                        const std::string& delims,
                                        const size_t maxSplits = 0) {
      std::string remaining(str);
      std::vector<std::string> result;
      size_t splits = 0, pos;

      while (((maxSplits == 0) || (splits < maxSplits)) &&
             ((pos = remaining.find_first_of(delims)) != std::string::npos)) {
        result.push_back(remaining.substr(0, pos));
        remaining = remaining.substr(pos + 1);
        splits++;
      }

      if (remaining.length() > 0)
        result.push_back(remaining);

      return result;
    }
  }

  static const char *SDP_IDENTIFIER = "Orbit";
  static const char *cand = "a=candidate:";
  static const char *crypto = "a=crypto:";
  static const char *group = "a=group:";
  static const char *video = "m=video";
  static const char *audio = "m=audio";
  static const char *mid = "a=mid";
  static const char *sendrecv = "a=sendrecv";
  static const char *recvonly = "a=recvonly";
  static const char *sendonly = "a=sendonly";
  static const char *ice_user = "a=ice-ufrag";
  static const char *ice_pass = "a=ice-pwd";
  static const char *ssrctag = "a=ssrc";
  static const char *ssrcgrouptag = "a=ssrc-group";
  static const char *savpf = "SAVPF";
  static const char *rtpmap = "a=rtpmap:";
  static const char *rtcpmux = "a=rtcp-mux";
  static const char *fp = "a=fingerprint";
  static const char *rtcpfb = "a=rtcp-fb:";
  static const char *fmtp = "a=fmtp:";
  static const char *bas = "b=AS:";

  SdpInfo::SdpInfo() {
    isExtMapEnabled = false;
    isRedFecEnabled = false;
    if (FLAGS_enable_red_fec) {
      isRedFecEnabled = true;
    }
    isBundle = false;
    isRtcpMux = false;
    isFingerprint = false;
    hasAudio = false;
    hasVideo = false;
    profile = SAVPF;
    audioSsrc = 0;
    videoSsrc = 0;
    videoRtxSsrc = 0;
    videoCodecs = 0;
    audioCodecs = 0;
    videoSdpMLine = -1;
    audioSdpMLine = -1;
    videoBandwidth = 0;
 
    InitializeRtpMap();
  }

  void SdpInfo::InitializeRtpMap() {
    std::string use_codecs = "VP8";
    if (video_encoding_ == olive::VideoEncoding::VP8) {
      use_codecs = "VP8";
    } else if (video_encoding_ == olive::VideoEncoding::H264) {
      use_codecs = "H264";
    } else if (video_encoding_ == olive::VideoEncoding::VP9) {
      use_codecs = "VP9";
    }

    if (use_codecs == "H264") {
      // Use H264 encoding/decoding.
      RtpMap h264;
      h264.payloadType = H264_90000_PT;
      h264.encodingName = "H264";
      h264.clockRate = 90000;
      h264.channels = 1;
      h264.mediaType = VIDEO_TYPE;
      internalPayloadVector_.push_back(h264);
    } else if (use_codecs == "VP9") {
      // Use VP9 encoding/decoding.
      RtpMap vp9;
      vp9.payloadType = VP9_90000_PT;
      vp9.encodingName = "VP9";
      vp9.clockRate = 90000;
      vp9.channels = 1;
      vp9.mediaType = VIDEO_TYPE;
      internalPayloadVector_.push_back(vp9);
    } else {
      // By default, use VP8 codecs.
      RtpMap vp8;
      vp8.payloadType = VP8_90000_PT;
      vp8.encodingName = "VP8";
      vp8.clockRate = 90000;
      vp8.channels = 1;
      vp8.mediaType = VIDEO_TYPE;
      internalPayloadVector_.push_back(vp8);
    }

    if (isRedFecEnabled) {
      RtpMap red;
      red.payloadType = RED_90000_PT;
      red.encodingName = "red";
      red.clockRate = 90000;
      red.channels = 1;
      red.mediaType = VIDEO_TYPE;
      internalPayloadVector_.push_back(red);

      RtpMap ulpfec;
      ulpfec.payloadType = ULP_90000_PT;
      ulpfec.encodingName = "ulpfec";
      ulpfec.clockRate = 90000;
      ulpfec.channels = 1;
      ulpfec.mediaType = VIDEO_TYPE;
      internalPayloadVector_.push_back(ulpfec);
    }

    /*
       RtpMap rtx;
       rtx.payloadType = RTX_90000_PT;
       rtx.encodingName = "rtx";
       rtx.clockRate = 90000;
       rtx.channels = 1;
       rtx.mediaType = VIDEO_TYPE;
       internalPayloadVector_.push_back(rtx);
       */
    
    RtpMap opus;
    opus.payloadType = OPUS_48000_PT;
    opus.encodingName = "opus";
    opus.clockRate = 48000;
    opus.channels = 2;
    opus.mediaType = AUDIO_TYPE;
    opus.formatParameters.insert(std::pair<std::string, std::string>("useinbandfec","1"));
    internalPayloadVector_.push_back(opus);
    RtpMap isac16;
    isac16.payloadType = ISAC_16000_PT;
    isac16.encodingName = "ISAC";
    isac16.clockRate = 16000;
    isac16.channels = 1;
    isac16.mediaType = AUDIO_TYPE;
    internalPayloadVector_.push_back(isac16);

    RtpMap isac32;
    isac32.payloadType = ISAC_32000_PT;
    isac32.encodingName = "ISAC";
    isac32.clockRate = 32000;
    isac32.channels = 1;
    isac32.mediaType = AUDIO_TYPE;
    internalPayloadVector_.push_back(isac32);
    RtpMap pcmu;
    pcmu.payloadType = PCMU_8000_PT;
    pcmu.encodingName = "PCMU";
    pcmu.clockRate = 8000;
    pcmu.channels = 1;
    pcmu.mediaType = AUDIO_TYPE;
    internalPayloadVector_.push_back(pcmu);
/*
    RtpMap pcma;
    pcma.payloadType = PCMA_8000_PT;
    pcma.encodingName = "PCMA";
    pcma.clockRate = 8000;
    pcma.channels = 1;
    pcma.mediaType = AUDIO_TYPE;
    internalPayloadVector_.push_back(pcma);
*/
    RtpMap cn8;
    cn8.payloadType = CN_8000_PT;
    cn8.encodingName = "CN";
    cn8.clockRate = 8000;
    cn8.channels = 1;
    cn8.mediaType = AUDIO_TYPE;
    internalPayloadVector_.push_back(cn8);

    RtpMap cn16;
    cn16.payloadType = CN_16000_PT;
    cn16.encodingName = "CN";
    cn16.clockRate = 16000;
    cn16.channels = 1;
    cn16.mediaType = AUDIO_TYPE;
    internalPayloadVector_.push_back(cn16);

    RtpMap cn32;
    cn32.payloadType = CN_32000_PT;
    cn32.encodingName = "CN";
    cn32.clockRate = 32000;
    cn32.channels = 1;
    cn32.mediaType = AUDIO_TYPE;
    internalPayloadVector_.push_back(cn32);

    RtpMap cn48;
    cn48.payloadType = CN_48000_PT;
    cn48.encodingName = "CN";
    cn48.clockRate = 48000;
    cn48.channels = 1;
    cn48.mediaType = AUDIO_TYPE;
    internalPayloadVector_.push_back(cn48);
 
    RtpMap telephoneevent;
    telephoneevent.payloadType = TEL_8000_PT;
    telephoneevent.encodingName = "telephone-event";
    telephoneevent.clockRate = 8000;
    telephoneevent.channels = 1;
    telephoneevent.mediaType = AUDIO_TYPE;
    internalPayloadVector_.push_back(telephoneevent);
  }

  SdpInfo::~SdpInfo() {
  }

  bool SdpInfo::initWithSdp(const std::string& sdp, const std::string& media) {
    return processSdp(sdp, media);
  }
  std::string SdpInfo::addCandidate(const CandidateInfo& info) {
    candidateVector_.push_back(info);
    return stringifyCandidate(info);
  }

  std::string SdpInfo::stringifyCandidate(const CandidateInfo & candidate){
    return candidate.ToString();
  }

  void SdpInfo::addCrypto(const CryptoInfo& info) {
    cryptoVector_.push_back(info);
  }

  void SdpInfo::setCredentials(const std::string& username, const std::string& password, MediaType media) {
    switch(media){
      case(VIDEO_TYPE):
        iceVideoUsername_ = std::string(username);
        iceVideoPassword_ = std::string(password);
        break;
      case(AUDIO_TYPE):
        iceAudioUsername_ = std::string(username);
        iceAudioPassword_ = std::string(password);
        break;
      default:
        iceVideoUsername_ = std::string(username);
        iceVideoPassword_ = std::string(password);
        iceAudioUsername_ = std::string(username);
        iceAudioPassword_ = std::string(password);
        break;
    }
  }

  void SdpInfo::getCredentials(std::string& username, std::string& password, MediaType media) {
    switch(media){
      case(VIDEO_TYPE):
        username.replace(0, username.length(),iceVideoUsername_);
        password.replace(0, username.length(),iceVideoPassword_);
        break;
      case(AUDIO_TYPE):
        username.replace(0, username.length(),iceAudioUsername_);
        password.replace(0, username.length(),iceAudioPassword_);
        break;
      default:
        username.replace(0, username.length(),iceVideoUsername_);
        password.replace(0, username.length(),iceVideoPassword_);
        break;
    }
  }

  std::string SdpInfo::getSdp() {
    char msidtemp [10];
    gen_random(msidtemp,10);

    VLOG(2) << "Getting SDP";

    std::ostringstream sdp;
    sdp << "v=0\n" << "o=- 0 0 IN IP4 127.0.0.1\n";
    sdp << "s=" << SDP_IDENTIFIER << "\n";
    sdp << "t=0 0\n";

    if (isBundle) {
      sdp << "a=group:BUNDLE";
      /*
         if (this->hasAudio)
         sdp << " audio";
         if (this->hasVideo)
         sdp << " video";
         */
      for (uint8_t i = 0; i < bundleTags.size(); i++){
        sdp << " " << bundleTags[i].id;
      }
      sdp << "\n";
      sdp << "a=msid-semantic: WMS "<< msidtemp << endl;
    }
    //candidates audio
    bool printedAudio = true, printedVideo = true;

    if (printedAudio && this->hasAudio) {
      sdp << "m=audio 1 UDP/TLS/RTP/" << (profile==SAVPF?"SAVPF ":"AVPF ");// << "103 104 0 8 106 105 13 126\n"
      
      int codecCounter=0;
      bool first = true;
      for (unsigned int it =0; it<payloadVector.size(); it++){
        const RtpMap& payload_info = payloadVector[it];
        if (payload_info.mediaType == AUDIO_TYPE){
          codecCounter++;
          if (!first) {
            sdp << " ";
          }
          sdp << payload_info.payloadType;
          first = false;
        }
      }

      sdp << "\n"
        << "c=IN IP4 0.0.0.0" << endl;
      if (isRtcpMux) {
        sdp << "a=rtcp:1 IN IP4 0.0.0.0" << endl;
      }
      for (unsigned int it = 0; it < candidateVector_.size(); it++) {
        if(candidateVector_[it].mediaType == AUDIO_TYPE || isBundle)
          sdp << this->stringifyCandidate(candidateVector_[it]) << endl;
      }
      LOG(INFO) << "iceAudioUsername_ = " << iceAudioUsername_;
      if(iceAudioUsername_.size()>0){
        sdp << "a=ice-ufrag:" << iceAudioUsername_ << endl;
        sdp << "a=ice-pwd:" << iceAudioPassword_ << endl;
      } else{
        sdp << "a=ice-ufrag:" << iceVideoUsername_ << endl;
        sdp << "a=ice-pwd:" << iceVideoPassword_ << endl;
      }
      //sdp << "a=ice-options:google-ice" << endl;
      if (isFingerprint) {
        sdp << "a=fingerprint:sha-256 "<< fingerprint << endl;
      }
      if (!setup_.empty()) {
        sdp << "a=setup:" << setup_ << endl;
      }
      
      switch (this->audioDirection){
        case SENDONLY:
          sdp << "a=sendonly" << endl;
          break;
        case SENDRECV:
          sdp << "a=sendrecv" << endl;
          break;
        case RECVONLY:
          sdp << "a=recvonly" << endl;
          break;
      }
      if (bundleTags.size()>2){
        LOG(ERROR) << "More bundleTags than supported, expect unexpected behaviour";
      }
      for (uint8_t i = 0; i < bundleTags.size(); i++){
        if(bundleTags[i].mediaType == AUDIO_TYPE){
          sdp << "a=mid:" << bundleTags[i].id << endl;
        }
      }
      if (isExtMapEnabled) {
        sdp << "a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level" << endl;
      }

      if (isRtcpMux)
        sdp << "a=rtcp-mux\n";
      for (unsigned int it = 0; it < cryptoVector_.size(); it++) {
        const CryptoInfo& cryp_info = cryptoVector_[it];
        if (cryp_info.mediaType == AUDIO_TYPE) {
          sdp << "a=crypto:" << cryp_info.tag << " "
            << cryp_info.cipherSuite << " " << "inline:"
            << cryp_info.keyParams << endl;
        }
      }

      for (unsigned int it = 0; it < payloadVector.size(); it++) {
        const RtpMap& rtp = payloadVector[it];
        if (rtp.mediaType==AUDIO_TYPE) {
          int payloadType = rtp.payloadType;
          if (rtp.channels>1) {
            sdp << "a=rtpmap:"<< payloadType << " " << rtp.encodingName << "/"
              << rtp.clockRate << "/" << rtp.channels << endl;
          } else {
            sdp << "a=rtpmap:"<< payloadType << " " << rtp.encodingName << "/"
              << rtp.clockRate << endl;
          }
          for (std::map<std::string, std::string>::const_iterator theIt = rtp.formatParameters.begin(); 
              theIt != rtp.formatParameters.end(); theIt++){
            if (theIt->first.compare("none")){
              sdp << "a=fmtp:" << payloadType << " " << theIt->first << "=" << theIt->second << endl;
            }else{
              sdp << "a=fmtp:" << payloadType << " " << theIt->second << endl;
            }

          }
        }
      }

      if (audioSsrc == 0) {
        audioSsrc = 44444;
      }

      sdp << "a=ptime:50" << endl;
      sdp << "a=maxptime:60" << endl;
      sdp << "a=ssrc:" << audioSsrc << " cname:o/i14u9pJrxRKAsu" << endl<<
        "a=ssrc:"<< audioSsrc << " msid:"<< msidtemp << " a0"<< endl<<
        "a=ssrc:"<< audioSsrc << " mslabel:"<< msidtemp << endl<<
        "a=ssrc:"<< audioSsrc << " label:" << msidtemp <<"a0"<<endl;

      if (FLAGS_audio_nack) {
        sdp << "a=rtcp-fb:111 nack" << endl;
      }
    }

    if (printedVideo && this->hasVideo) {
      sdp << "m=video 1 UDP/TLS/RTP/" << (profile==SAVPF?"SAVPF ":"AVPF "); //<<  "100 101 102 103\n"
      int codecCounter = 0;      
      for (unsigned int it =0; it<payloadVector.size(); it++){
        const RtpMap& payload_info = payloadVector[it];
        if (payload_info.mediaType == VIDEO_TYPE){
          codecCounter++;
          sdp << payload_info.payloadType <<((codecCounter<videoCodecs)?" ":"");
        }
      }
      // WARNING !!!!!!!
      //  right now we set the bandwidth to 150kbps? Is this a reasonable value?
      // -----------------------------------------------
      //sdp << "\nb=AS:150";
      // Set the target bandwidth
      sdp << "\nb=AS:" << target_bandwidth_;
      
      sdp << "\n" << "c=IN IP4 0.0.0.0" << endl;
      if (isRtcpMux) {
        sdp << "a=rtcp:1 IN IP4 0.0.0.0" << endl;
      }
      for (unsigned int it = 0; it < candidateVector_.size(); it++) {
        if(candidateVector_[it].mediaType == VIDEO_TYPE)
          sdp << this->stringifyCandidate(candidateVector_[it]) << endl;
      }

      sdp << "a=ice-ufrag:" << iceVideoUsername_ << endl;
      sdp << "a=ice-pwd:" << iceVideoPassword_ << endl;
      //sdp << "a=ice-options:google-ice" << endl;

      if (isExtMapEnabled) {
        sdp << "a=extmap:2 urn:ietf:params:rtp-hdrext:toffset" << endl;
        sdp << "a=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time" << endl;
      }

      if (isFingerprint) {
        sdp << "a=fingerprint:sha-256 "<< fingerprint << endl;
      }
      if (!setup_.empty()) {
        sdp << "a=setup:" << setup_ << endl;
      }
      
      switch (this->videoDirection){
        case SENDONLY:
          sdp << "a=sendonly" << endl;
          break;
        case SENDRECV:
          sdp << "a=sendrecv" << endl;
          break;
        case RECVONLY:
          sdp << "a=recvonly" << endl;
          break;
      }
      for (uint8_t i = 0; i < bundleTags.size(); i++){
        if(bundleTags[i].mediaType == VIDEO_TYPE){
          sdp << "a=mid:" << bundleTags[i].id << endl;
        }
      }
      if (isRtcpMux)
        sdp << "a=rtcp-mux\n";
      for (unsigned int it = 0; it < cryptoVector_.size(); it++) {
        const CryptoInfo& cryp_info = cryptoVector_[it];
        if (cryp_info.mediaType == VIDEO_TYPE) {
          sdp << "a=crypto:" << cryp_info.tag << " "
            << cryp_info.cipherSuite << " " << "inline:"
            << cryp_info.keyParams << endl;
        }
      }

      for (unsigned int it = 0; it < payloadVector.size(); it++) {
        const RtpMap& rtp = payloadVector[it];
        if (rtp.mediaType==VIDEO_TYPE) {
          int payloadType = rtp.payloadType;
          sdp << "a=rtpmap:"<<payloadType << " " << rtp.encodingName << "/"
            << rtp.clockRate <<"\n";
          if(!rtp.feedbackTypes.empty()){
            for (unsigned int itFb = 0; itFb < rtp.feedbackTypes.size(); itFb++){
              sdp << "a=rtcp-fb:" << payloadType << " " << rtp.feedbackTypes[itFb] << "\n";
            }
          }
          for (std::map<std::string, std::string>::const_iterator theIt = rtp.formatParameters.begin(); 
              theIt != rtp.formatParameters.end(); theIt++){
            if (theIt->first.compare("none")){
              sdp << "a=fmtp:" << payloadType << " " << theIt->first << "=" << theIt->second << endl;
            }else{
              sdp << "a=fmtp:" << payloadType << " " << theIt->second << endl;
            }
          }

          if (FLAGS_set_min_bitrate) {
            sdp << "a=fmtp:" << payloadType
                << " x-google-min-bitrate=" << min_bitrate_
                << endl;
          }
        }
      }

      int t = 0;
      if (videoSsrc != 0) {
        sdp << "a=ssrc:" << videoSsrc << " cname:o/orbit" << endl<<
          "a=ssrc:"<< videoSsrc << " msid:"<< msidtemp << " v"<< t << endl<<
          "a=ssrc:"<< videoSsrc << " mslabel:"<< msidtemp << endl<<
          "a=ssrc:"<< videoSsrc << " label:" << msidtemp <<"v"<< t << endl;
      }
      t++;
      for (unsigned int videoSsrc : this->extended_video_ssrc) {
        sdp << "a=ssrc:" << videoSsrc << " cname:o/orbit" << endl<<
          "a=ssrc:"<< videoSsrc << " msid:"<< msidtemp << " v"<<t << endl<<
          "a=ssrc:"<< videoSsrc << " mslabel:"<< msidtemp << endl<<
          "a=ssrc:"<< videoSsrc << " label:" << msidtemp <<"v"<<t << endl;
        t++;
      }
      if (videoRtxSsrc!=0) {
        sdp << "a=ssrc:" << videoRtxSsrc << " cname:o/orbit" << endl<<
          "a=ssrc:"<< videoRtxSsrc << " msid:"<< msidtemp << " v0" << endl<<
          "a=ssrc:"<< videoRtxSsrc << " mslabel:"<< msidtemp << endl<<
          "a=ssrc:"<< videoRtxSsrc << " label:" << msidtemp <<"v0" << endl;
      }
    }
    LOG(INFO) << "sdp local \n " << sdp.str().c_str();
    return sdp.str();
  }

  RtpMap *SdpInfo::getCodecByName(const std::string codecName, const unsigned int clockRate) {
    for (unsigned int it = 0; it < internalPayloadVector_.size(); it++) {
      RtpMap& rtp = internalPayloadVector_[it];
      if (rtp.encodingName == codecName && rtp.clockRate == clockRate) {
        return &rtp;
      }
    }
    return NULL;
  }

  bool SdpInfo::supportCodecByName(const std::string codecName, const unsigned int clockRate) {
    RtpMap *rtp = getCodecByName(codecName, clockRate);
    if (rtp != NULL) {
      return supportPayloadType(rtp->payloadType);
    }
    return false;
  }

  bool SdpInfo::supportPayloadType(const int payloadType) {
    if (inOutPTMap.count(payloadType) > 0) {

      for (unsigned int it = 0; it < payloadVector.size(); it++) {
        const RtpMap& rtp = payloadVector[it];
        if (inOutPTMap[rtp.payloadType] == payloadType) {
          return true;
        }
      }

    }
    return false;
  }

  void SdpInfo::createOfferSdp(){

    this->payloadVector = internalPayloadVector_;
    this->isBundle = true;
    this->profile = SAVPF;
    this->isRtcpMux = true;
    this->videoSdpMLine = 0;
    this->audioSdpMLine = 1;
    this->hasVideo = true;
    this->hasAudio = true;
    this->videoDirection = SENDRECV;
    this->audioDirection = SENDRECV;
    this->videoSsrc = 55543;
    this->videoRtxSsrc = 55555;
    VLOG(2) << "Setting Offer SDP";
  }

  void SdpInfo::setOfferSdp(const SdpInfo& offerSdp) {
    this->videoCodecs = offerSdp.videoCodecs;
    this->audioCodecs = offerSdp.audioCodecs;
    this->payloadVector = offerSdp.payloadVector;
    this->isBundle = offerSdp.isBundle;
    this->profile = offerSdp.profile;
    this->isRtcpMux = offerSdp.isRtcpMux;
    this->videoSdpMLine = offerSdp.videoSdpMLine;
    this->audioSdpMLine = offerSdp.audioSdpMLine;
    this->inOutPTMap = offerSdp.inOutPTMap;
    this->outInPTMap = offerSdp.outInPTMap;
    this->hasVideo = offerSdp.hasVideo;
    this->hasAudio = offerSdp.hasAudio;
    this->bundleTags = offerSdp.bundleTags;
    switch(offerSdp.videoDirection){
      case SENDONLY:
        this->videoDirection = RECVONLY;
        break;
      case RECVONLY:
        this->videoDirection = SENDONLY;
        break;
      case SENDRECV:
        this->videoDirection = SENDRECV;
        break;
      default:
        this->videoDirection = SENDRECV;
        break;
    }
    switch(offerSdp.audioDirection){
      case SENDONLY:
        this->audioDirection = RECVONLY;
        break;
      case RECVONLY:
        this->audioDirection = SENDONLY;
        break;
      case SENDRECV:
        this->audioDirection = SENDRECV;
        break;
      default:
        this->audioDirection = SENDRECV;
        break;
    }

    this->audioSsrc = rand();
    this->videoSsrc = rand();
    this->videoRtxSsrc = rand();

    if (offerSdp.videoRtxSsrc != 0){
      this->videoRtxSsrc = 55555;
    }
    VLOG(2) << "Setting SourceSDP";
  }

  // Extract the new number of video to map.
  // 0:VP8_90000_PT; 1:VP9_90000_PT; 2:H264_90000_PT;
  // 3:RED_90000_PT; 4:ULP_90000_PT;
  void SdpInfo::ExtractNewCodes(const std::string& offer) {
    std::string vp8_regex = "a=rtpmap:([1-9][0-9]*) VP8";
    int codes = GetCodesNumber(offer, vp8_regex);
    if (codes != -1 && codes != VP8_90000_PT) {
      inOut_payload_.insert(std::pair<int, int>(codes, VP8_90000_PT));
      outIn_payload_.insert(std::pair<int, int>(VP8_90000_PT, codes));
    }

    std::string vp9_regex = "a=rtpmap:([1-9][0-9]*) VP9";
    codes = GetCodesNumber(offer, vp9_regex);
    if (codes != -1 && codes != VP9_90000_PT) {
      inOut_payload_.insert(std::pair<int, int>(codes, VP9_90000_PT));
      outIn_payload_.insert(std::pair<int, int>(VP9_90000_PT, codes));
    }

    std::string h264_regex = "a=rtpmap:([1-9][0-9]*) H264";
    codes = GetCodesNumber(offer, h264_regex);
    if (codes != -1 && codes != H264_90000_PT) {
      inOut_payload_.insert(std::pair<int, int>(codes, H264_90000_PT));
      outIn_payload_.insert(std::pair<int, int>(H264_90000_PT, codes));
    }

    std::string red_regex = "a=rtpmap:([1-9][0-9]*) red";
    codes = GetCodesNumber(offer, red_regex);
    if (codes != -1 && codes != RED_90000_PT) {
      inOut_payload_.insert(std::pair<int, int>(codes, RED_90000_PT));
      outIn_payload_.insert(std::pair<int, int>(RED_90000_PT, codes));
    }

    std::string ulp_regex = "a=rtpmap:([1-9][0-9]*) ulpfec";
    codes = GetCodesNumber(offer, ulp_regex);
    if (codes != -1 && codes != ULP_90000_PT) {
      inOut_payload_.insert(std::pair<int, int>(codes, ULP_90000_PT));
      outIn_payload_.insert(std::pair<int, int>(ULP_90000_PT, codes));
    }
  }

  int SdpInfo::GetCodesNumber(const std::string& offer, const std::string& reg) {
    int codes = -1;

    boost::regex tag_regex(reg);
    std::string::const_iterator begin = offer.begin();
    std::string::const_iterator end = offer.end();
    boost::match_results<std::string::const_iterator> what;
    boost::match_flag_type flags = boost::match_default;
    if (boost::regex_search(begin, end, what, tag_regex, flags)) {
      std::string str = std::string(what[1].first, what[1].second);
      std::stringstream ss;
      ss << str;
      ss >> codes;
    }
    
    return codes;
  }

  bool SdpInfo::processSdp(const std::string& sdp, const std::string& media) {

    std::string line;
    std::istringstream iss(sdp);
    int mlineNum = -1;
    std::vector<std::string> tmpFeedbackVector;

    MediaType mtype = OTHER;
    if (media == "audio") {
      mtype = AUDIO_TYPE;
    } else if (media == "video") {
      mtype = VIDEO_TYPE;
    }

    while (std::getline(iss, line)) {
      size_t isVideo = line.find(video);
      size_t isAudio = line.find(audio);
      size_t isGroup = line.find(group);
      size_t isMid = line.find(mid);
      size_t isCand = line.find(cand);
      size_t isCrypt = line.find(crypto);
      size_t isUser = line.find(ice_user);
      size_t isPass = line.find(ice_pass);
      size_t isSsrc = line.find(ssrctag);
      size_t isSsrcGroup = line.find(ssrcgrouptag);
      size_t isSAVPF = line.find(savpf);
      size_t isRtpmap = line.find(rtpmap);
      size_t isRtcpMuxchar = line.find(rtcpmux);
      size_t isSendRecv = line.find(sendrecv);
      size_t isRecvOnly = line.find(recvonly);
      size_t isSendOnly = line.find(sendonly);
      size_t isFP = line.find(fp);
      size_t isFeedback = line.find(rtcpfb);
      size_t isFmtp = line.find(fmtp);
      size_t isBandwidth = line.find(bas);

      VLOG(2) << "current line -> " << line.c_str();

      if (isRtcpMuxchar != std::string::npos){
        isRtcpMux = true;
      }

      // At this point we support only one direction per SDP
      // Any other combination does not make sense at this point in Licode
      if (isRecvOnly != std::string::npos){
        VLOG(2) << "RecvOnly sdp";
          if (mtype == AUDIO_TYPE){
            this->audioDirection = RECVONLY;
          }else{
            this->videoDirection = RECVONLY;
          }
      }else if(isSendOnly != std::string::npos){
        VLOG(2) << "SendOnly sdp";
          if (mtype == AUDIO_TYPE){
            this->audioDirection = SENDONLY;
          }else{
            this->videoDirection = SENDONLY;
          }
      }else if (isSendRecv != std::string::npos){
        if (mtype == AUDIO_TYPE){
          this->audioDirection = SENDRECV;
        }else{
          this->videoDirection = SENDRECV;
        }
        VLOG(2) << "SendRecv sdp";
      }

      if (isSAVPF != std::string::npos){
        profile = SAVPF;
        VLOG(2) << "PROFILE " << line.substr(isSAVPF).c_str() << "(1 SAVPF)";
      }

      if (isFP != std::string::npos){
        std::vector<std::string> parts;

        // FIXME add error checking here
        parts = stringutil::splitOneOf(line, ":", 1);
        parts = stringutil::splitOneOf(parts[1], " ");

        fingerprint = parts[1];
        isFingerprint = true;
        VLOG(2) << "Fingerprint " << fingerprint.c_str();
      }

      if (isGroup != std::string::npos) {
        std::vector<std::string> parts;
        parts = stringutil::splitOneOf(line, ":  \r\n", 10);
        if (!parts[1].compare("BUNDLE")){
          VLOG(2) << "BUNDLE sdp";
          isBundle = true;
        }
        // number of parts will vary depending on whether audio and video are present in the bundle
        if (parts.size() >= 3) {
          for (unsigned int tagno=2; tagno<parts.size(); tagno++) {
            VLOG(2) << "Adding " << parts[tagno].c_str() << "to bundle vector";
            BundleTag theTag(parts[tagno], OTHER);
            bundleTags.push_back(theTag);
          }
        }
      }
      if (isVideo != std::string::npos) {
        videoSdpMLine = ++mlineNum; 
        VLOG(2) << "sdp has video, mline = " << videoSdpMLine;

        mtype = VIDEO_TYPE;
        hasVideo = true;
      }
      if (isAudio != std::string::npos) {
        audioSdpMLine = ++mlineNum; 
        VLOG(2) << "sdp has audio, mline = " << audioSdpMLine;
        mtype = AUDIO_TYPE;
        hasAudio = true;
      }
      if (isCand != std::string::npos) {
        std::vector<std::string> pieces = stringutil::splitOneOf(line, " :");
        processCandidate(pieces, mtype);
      }

      if (isCrypt != std::string::npos) {
        CryptoInfo crypinfo;
        std::vector<std::string> cryptopiece = stringutil::splitOneOf(line, " :");

        // FIXME: add error checking here
        LOG(INFO) << line;
        crypinfo.cipherSuite = cryptopiece[2];
        crypinfo.keyParams = cryptopiece[4];
        crypinfo.mediaType = mtype;
        cryptoVector_.push_back(crypinfo);
        VLOG(2) << "Crypto Info: " << crypinfo.cipherSuite.c_str()
                  << crypinfo.keyParams.c_str()
                  << crypinfo.mediaType;
      }

      if (isUser != std::string::npos) {
        std::vector<std::string> parts = stringutil::splitOneOf(stringutil::splitOneOf(line, ":", 1)[1], "\r", 1);
        if (!parts.empty()) {
          if (mtype == VIDEO_TYPE){
            iceVideoUsername_ = parts[0];
            VLOG(2) << "ICE Video username: " << iceVideoUsername_.c_str();
          }else if (mtype == AUDIO_TYPE){
            iceAudioUsername_ = parts[0];
            VLOG(2) << "ICE Audio username: %s" <<  iceAudioUsername_.c_str();
          }else{
            VLOG(2) << "Unknown media type for ICE credentials, looks like Firefox";
            iceVideoUsername_ = parts[0];
          }
        } else {
          VLOG(2) << "Value of 'ice-ufrag' is empty.";
        }
      }

      if (isPass != std::string::npos) {
        std::vector<std::string> parts = stringutil::splitOneOf(stringutil::splitOneOf(line, ":", 1)[1], "\r", 1);
        if (!parts.empty()) {
          if (mtype == VIDEO_TYPE){
            iceVideoPassword_ = parts[0];
            VLOG(2) << "ICE Video password: " << iceVideoPassword_.c_str();
          }else if (mtype == AUDIO_TYPE){
            iceAudioPassword_ = parts[0];
            VLOG(2) << "ICE Audio password: " << iceAudioPassword_.c_str();
          }else{
            VLOG(2) << "Unknown media type for ICE credentials, looks like Firefox";
            iceVideoPassword_ = parts[0];
          }
        } else {
          VLOG(2) << "Value of 'ice-pwd' is empty.";
        }
      }

      if (isMid!= std::string::npos){
        std::vector<std::string> parts = stringutil::splitOneOf(line, ": \r\n",4);
        if (parts.size()>=2){
          std::string thisId = parts[1];
          for (uint8_t i = 0; i < bundleTags.size(); i++){
            if (!bundleTags[i].id.compare(thisId)){
              VLOG(2) << "Setting tag " << thisId.c_str() << " to mediaType " << mtype;
              bundleTags[i].mediaType = mtype;
            }
          }
        }else{
          LOG(ERROR) << "Unexpected size of a=mid element";
        }
      }

      if (isSsrc != std::string::npos) {
        std::vector<std::string> parts = stringutil::splitOneOf(line, " :", 2);
        // FIXME add error checking
        if ((mtype == VIDEO_TYPE) && (videoSsrc == 0)) {
          videoSsrc = strtoul(parts[1].c_str(), NULL, 10);
          VLOG(2) << "video ssrc: " << videoSsrc;
        } else if ((mtype == AUDIO_TYPE) && (audioSsrc == 0)) {
          audioSsrc = strtoul(parts[1].c_str(), NULL, 10);
          VLOG(2) << "audio ssrc: " << audioSsrc;
        }
      }
      if (isSsrcGroup != std::string::npos) {
        if (mtype == VIDEO_TYPE){
          VLOG(2) << "FID group detected";
          std::vector<std::string> parts = stringutil::splitOneOf(line, " :", 10);
          if (parts.size()>=4){
            videoRtxSsrc = strtoul(parts[3].c_str(), NULL, 10);
            VLOG(2) << "Setting videoRtxSsrc to " << videoRtxSsrc;
          }
        }
      }
      // a=rtpmap:PT codec_name/clock_rate
      if(isRtpmap != std::string::npos){
        std::vector<std::string> parts = stringutil::splitOneOf(line, " :/\n", 4);
        RtpMap theMap;
        unsigned int PT = strtoul(parts[1].c_str(), NULL, 10);
        std::string codecname = parts[2];
        unsigned int clock = strtoul(parts[3].c_str(), NULL, 10);
        theMap.payloadType = PT;
        theMap.encodingName = codecname;
        theMap.clockRate = clock;
        theMap.mediaType = mtype;
        VLOG(2) << "theMAp PT: " << PT << ", name " << codecname.c_str() << ", clock " << clock;

        bool found = false;
        for (unsigned int it = 0; it < internalPayloadVector_.size(); it++) {
          const RtpMap& rtp = internalPayloadVector_[it];
          if (rtp.encodingName == codecname && rtp.clockRate == clock) {
            outInPTMap[PT] = rtp.payloadType;
            inOutPTMap[rtp.payloadType] = PT;
            theMap.channels = rtp.channels;

            // Set the codec in answer, and the codec is same with the server.
            // So the client send packet as the codec in answer, we need not to
            // fix the payload when the server receive the packet. In this case,
            // we should reset the payload when the server relay the packet to
            // client.
            theMap.payloadType = rtp.payloadType;
            
            found = true;
            VLOG(2) << "Mapping " << codecname.c_str() << "/" << clock << ":" <<  PT << " to " << rtp.encodingName.c_str() << "/" << rtp.clockRate << ":" << rtp.payloadType;
          }
        }
        if (found) {
          if(theMap.mediaType == VIDEO_TYPE)
            videoCodecs++;
          else
            audioCodecs++;

          payloadVector.push_back(theMap);
        }
      }

      if(isFeedback != std::string::npos){
        tmpFeedbackVector.push_back(line);
      }

      if (isFmtp != std::string::npos){
        std::vector<std::string> parts = stringutil::splitOneOf(line, " :=", 4);        
        if (parts.size() >= 4){
          unsigned int PT = strtoul(parts[2].c_str(), NULL, 10);
          std::string option = "none";
          std::string value = "none";
          if (parts.size() == 4){
            value = parts[3].c_str();
          }else{
            option = parts[3].c_str();
            value = parts[4].c_str();
          }
          VLOG(2) << "Parsing fmtp to PT " << PT << ", option " << option.c_str() << ", value " << value.c_str();
          for (unsigned int it = 0; it < payloadVector.size(); it++){
            RtpMap& rtp = payloadVector[it];
            if (rtp.payloadType == PT){
              VLOG(2) << "Saving fmtp to PT " << PT << ", option " << option.c_str() << ", value " << value.c_str();
              rtp.formatParameters[option] = value;
            }
          }
        } else if (parts.size()==4){


        }
      }

      if (isBandwidth != std::string::npos){
        if (mtype == VIDEO_TYPE){
          std::vector<std::string> parts = stringutil::splitOneOf(line,":",2);
          if (parts.size()>=2){
            videoBandwidth = strtoul(parts[1].c_str(), NULL, 10);
            VLOG(2) << "Bandwidth for video detected " << videoBandwidth;
          }
        }
      }

    }
    // If there is no video or audio credentials we use the ones we have
    if (iceVideoUsername_.empty() && iceAudioUsername_.empty()){
      VLOG(2) << "No valid credentials for ICE";
      return false;
    }else if (iceVideoUsername_.empty()){
      VLOG(2) << "Video credentials empty, setting the audio ones";
      iceVideoUsername_ = iceAudioUsername_;
      iceVideoPassword_ = iceAudioPassword_;
    }else if (iceAudioUsername_.empty()){
      VLOG(2) << "Audio credentials empty, setting the video ones";
      iceAudioUsername_ = iceVideoUsername_;
      iceAudioPassword_ = iceVideoPassword_;
    }

    for (unsigned int i = 0; i < candidateVector_.size(); i++) {
      CandidateInfo& c = candidateVector_[i];
      c.isBundle = isBundle;
      if (c.mediaType == VIDEO_TYPE){
        c.username = iceVideoUsername_;
        c.password = iceVideoPassword_;
      }else{
        c.username = iceAudioUsername_;
        c.password = iceAudioPassword_;
      }
    }

    // Map the RTCP Feedback after we have built the payload vector
    for (unsigned int fbi = 0; fbi < tmpFeedbackVector.size(); fbi++){
      std::string line = tmpFeedbackVector[fbi];
      std::vector<std::string> parts = stringutil::splitOneOf(line, " :", 2);
      unsigned int PT = strtoul(parts[1].c_str(), NULL, 10);
      std::string feedback = parts[2];
      for (unsigned int it = 0; it < payloadVector.size(); it++){
        RtpMap& rtp = payloadVector[it];
        if (rtp.payloadType == PT){
          VLOG(2) << "Adding " << feedback.c_str() << " feedback to pt " << PT;
          rtp.feedbackTypes.push_back(feedback);
        }
      }
    }

    return true;
  }    

  std::vector<CandidateInfo>& SdpInfo::getCandidateInfos() {
    return candidateVector_;
  }

  void SdpInfo::copyCandidateInfos(std::vector<CandidateInfo> *vec) const {
    if (vec) {
      *vec = candidateVector_;
    } else {
      LOG(ERROR) << "NULL Pointer";
    }
  }

  std::vector<CryptoInfo>& SdpInfo::getCryptoInfos() {
    return cryptoVector_;
  }

  const std::vector<RtpMap>& SdpInfo::getPayloadInfos(){
    return payloadVector;
  }

  int SdpInfo::getAudioInternalPT(int externalPT) {
    // should use separate mapping for video and audio at the very least
    // standard requires separate mappings for each media, even!
    std::map<int, int>::iterator found = outInPTMap.find(externalPT);
    if (found != outInPTMap.end()) {
      return found->second;
    }
    return externalPT;
  }

  int SdpInfo::getVideoInternalPT(int externalPT) {
    // WARNING
    // should use separate mapping for video and audio at the very least
    // standard requires separate mappings for each media, even!
    return getAudioInternalPT(externalPT);
  }

  int SdpInfo::getAudioExternalPT(int internalPT) {
    // should use separate mapping for video and audio at the very least
    // standard requires separate mappings for each media, even!
    std::map<int, int>::iterator found = inOutPTMap.find(internalPT);
    if (found != inOutPTMap.end()) {
      return found->second;
    }
    return internalPT;
  }

  int SdpInfo::getVideoExternalPT(int internalPT) {
    // WARNING
    // should use separate mapping for video and audio at the very least
    // standard requires separate mappings for each media, even!
    return getAudioExternalPT(internalPT);
  }

  bool SdpInfo::processCandidate(std::vector<std::string>& pieces, MediaType mediaType) {

    CandidateInfo cand;
    static const char* types_str[] = { "host", "srflx", "prflx", "relay" };
    cand.mediaType = mediaType;
    cand.foundation = pieces[1];
    cand.componentId = (unsigned int) strtoul(pieces[2].c_str(), NULL, 10);

    cand.netProtocol = pieces[3];
    // libnice does not support tcp candidates, we ignore them
    VLOG(2) << "cand.netProtocol=" << cand.netProtocol.c_str();
    if (cand.netProtocol.compare("UDP") && cand.netProtocol.compare("udp")) {
      return false;
    }
    //	a=candidate:0 1 udp 2130706432 1383 52314 typ host  generation 0
    //		        0 1 2    3            4          5     6  7    8          9
    // 
    // a=candidate:1367696781 1 udp 33562367 138. 49462 typ relay raddr 138.4 rport 53531 generation 0
    cand.priority = (unsigned int) strtoul(pieces[4].c_str(), NULL, 10);
    cand.hostAddress = pieces[5];
    cand.hostPort = (unsigned int) strtoul(pieces[6].c_str(), NULL, 10);
    if (pieces[7] != "typ") {
      return false;
    }
    unsigned int type = 1111;
    int p;
    for (p = 0; p < 4; p++) {
      if (pieces[8] == types_str[p]) {
        type = p;
      }
    }
    switch (type) {
      case 0:
        cand.hostType = HOST;
        break;
      case 1:
        cand.hostType = SRFLX;
        break;
      case 2:
        cand.hostType = PRFLX;
        break;
      case 3:
        cand.hostType = RELAY;
        break;
      default:
        cand.hostType = HOST;
        break;
    }

    /*string tmp;
    StringPrintf( "Candidate Info: foundation=%s, componentId=%u, netProtocol=%s, "
        "priority=%u, hostAddress=%s, hostPort=%u, hostType=%u",
        cand.foundation.c_str(),
        cand.componentId,
        cand.netProtocol.c_str(),
        cand.priority,
        cand.hostAddress.c_str(),
        cand.hostPort,
        cand.hostType);
    */

    if (cand.hostType == SRFLX || cand.hostType==RELAY) {
      cand.rAddress = pieces[10];
      cand.rPort = (unsigned int) strtoul(pieces[12].c_str(), NULL, 10);
      VLOG(2) << "Parsing raddr srlfx or relay " << cand.rAddress.c_str() << ", " << cand.rPort;
    }
    candidateVector_.push_back(cand);
    return true;
  }

  void SdpInfo::gen_random(char *s, const int len) {
    static const char alphanum[] =
      "0123456789"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz";

    for (int i = 0; i < len; ++i) {
      s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    s[len] = 0;
  }

}/* namespace orbit */

