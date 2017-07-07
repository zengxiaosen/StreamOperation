/*
 * Copyright (C) 2016 Orangelab Inc. All Rights Reserved.
 * Author: cheng@orangelab.cn (Cheng Xu)
 *
 * audio_energy.cc
 * ---------------------------------------------------------------------------
 * Defines the implementation of audio energy and related stuff.
 * ---------------------------------------------------------------------------
 */

#include "audio_energy.h"

namespace orbit {
/* for Energy Computing */
#define WEBRTC_SPL_MUL(a, b) \
    ((opus_int32) ((opus_int32)(a) * (opus_int32)(b)))

opus_int16 WebRtcSpl_NormW32(opus_int32 a) {
	opus_int16 zeros;

	if (a == 0) {
		return 0;
	} else if (a < 0) {
		a = ~a;
	}

	if (!(0xFFFF8000 & a)) {
		zeros = 16;
	} else {
		zeros = 0;
	}
	if (!(0xFF800000 & (a << zeros))) zeros += 8;
	if (!(0xF8000000 & (a << zeros))) zeros += 4;
	if (!(0xE0000000 & (a << zeros))) zeros += 2;
	if (!(0xC0000000 & (a << zeros))) zeros += 1;

	return zeros;
}
opus_int16 WebRtcSpl_GetSizeInBits(opus_uint32 n) {
	opus_int16 bits;

	if (0xFFFF0000 & n) {
		bits = 16;
	} else {
		bits = 0;
	}
	if (0x0000FF00 & (n >> bits)) bits += 8;
	if (0x000000F0 & (n >> bits)) bits += 4;
	if (0x0000000C & (n >> bits)) bits += 2;
	if (0x00000002 & (n >> bits)) bits += 1;
	if (0x00000001 & (n >> bits)) bits += 1;

	return bits;
}
opus_int16 WebRtcSpl_GetScalingSquare(opus_int16 *in_vector, int in_vector_length, int times) {
	opus_int16 nbits = WebRtcSpl_GetSizeInBits((opus_uint32)times);
    int i;
    opus_int16 smax = -1;
    opus_int16 sabs;
    opus_int16 *sptr = in_vector;
    opus_int16 t;
    int looptimes = in_vector_length;

    for (i = looptimes; i > 0; i--) {
        sabs = (*sptr > 0 ? *sptr++ : -*sptr++);
        smax = (sabs > smax ? sabs : smax);
    }
    t = WebRtcSpl_NormW32(WEBRTC_SPL_MUL(smax, smax));

    if (smax == 0) {
        return 0; // Since norm(0) returns 0
    } else {
        return (t > nbits) ? 0 : nbits - t;
    }
}

opus_int32  WebRtcSpl_Energy(opus_int16 *_buffer, int _length, int *scale_factor) {
    opus_int32 energy = 0;
    *scale_factor = WebRtcSpl_GetScalingSquare(_buffer, _length, _length);

    opus_int16 *p = _buffer;
    int i;
    for (i = 0; i < _length; i++) {
        energy += (*p * *p) >> (*scale_factor);
        p++;
    }
    return energy;
}


}  // namespace orbit
