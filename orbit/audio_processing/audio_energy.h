/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * audio_energy.h
 * ---------------------------------------------------------------------------
 * Defines the interface to manipulate the audio energy and related stuff.
 * ---------------------------------------------------------------------------
 */

#ifndef AUDIO_ENERGY_H__
#define AUDIO_ENERGY_H__

#include <opus/opus.h>

namespace orbit {

  // The code below are mostly referenced from WebRtc code base.
opus_int16 WebRtcSpl_NormW32(opus_int32 a);
opus_int16 WebRtcSpl_GetSizeInBits(opus_uint32 n);
opus_int16 WebRtcSpl_GetScalingSquare(opus_int16 *in_vector,
                                      int in_vector_length,
                                      int times);
opus_int32  WebRtcSpl_Energy(opus_int16 *_buffer, int _length, int *scale_factor);

}  // namespace orbit

#endif  // AUDIO_ENERGY_H__
