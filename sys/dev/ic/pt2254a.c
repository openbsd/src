/*	$OpenBSD: pt2254a.c,v 1.1 2002/04/25 04:56:59 mickey Exp $	*/
/*
 * Copyright (c) 2002 Vladimir Popov <jumbo@narod.ru>.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Princeton Technology Corp.'s Electronic Volume Controller IC PT2254A
 *  http://www.princeton.com.tw
 *
 *  PT2254A is an electronic volume controller IC utilizing CMOS Technology
 *  specially designed for use in audio equipment. It has two built-in
 *  channels making it highly suitable for mono and stereo sound applications.
 *  Through the specially designated signals that are applied externally to
 *  the data, clock and strobe input pins, PT2254A can control attenuation
 *  and channel balance.
 */

#include <sys/param.h>

#include <dev/ic/pt2254a.h>

u_int32_t
pt2254a_encode_volume(u_int8_t *current, u_int8_t max) {
	u_int32_t ret = 0ul;
	u_int8_t vol, tens, ones;
	int steps;

	/*
	 * Volume management is done nonlinear.
	 *
	 * Approximate the curve with three lines:
	 * low volume:		y = - (48 /(max/3)) * x + 68
	 * middle volume:	y = - (14 /(max/3)) * x + 34
	 * high volume:		y = - (6 /(max/3)) * x + 18
	 */
	if (*current > 0 && *current <= max / 3) {
		vol = PT2254A_MAX_ATTENUATION - (144 * (int)(*current) / max);
	} else if (*current > max / 3 && *current <= 2 * max / 3) {
		vol = 34 - (42 * (int)(*current) / max);
	} else if (*current > 2 * max / 3) {
		vol = 18 - (18 * (int)(*current) / max);
	} else vol = PT2254A_MAX_ATTENUATION;

	/* Report adjusted volume */
	/* *current = max - (vol * max / PT2254A_MAX_ATTENUATION); */
	steps = *current * PT2254A_MAX_ATTENUATION / max;
	*current = steps * max / PT2254A_MAX_ATTENUATION;

	tens = vol / 10;
	ones = vol - tens * 10;

	ret = PT2254A_ATTENUATION_MAJOR(tens) | PT2254A_ATTENUATION_MINOR(ones);

	return ret;
}

u_int32_t
pt2254a_compose_register(u_int32_t lvol, u_int32_t rvol, int left, int right) {
	u_int32_t ret = 0ul;

	if (left == USE_CHANNEL) {
		ret |= PT2254A_LEFT_CHANNEL;
		ret |= lvol;
	}
	if (right == USE_CHANNEL) {
		ret |= PT2254A_RIGHT_CHANNEL;
		ret |= rvol;
	}

	ret |= PT2254A_ZERO_PADDING;
	ret |= PT2254A_EMPTY_BIT;

	return ret;
}
