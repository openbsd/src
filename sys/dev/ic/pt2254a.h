/*	$OpenBSD: pt2254a.h,v 1.1 2002/04/25 04:56:59 mickey Exp $	*/
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

#ifndef _PT2254A_H_
#define _PT2254A_H_

#define PT2254A_MAX_ATTENUATION			68
#define PT2254A_ATTENUATION_STEPS		35

#define PT2254A_REGISTER_LENGTH			18

#define PT2254A_ATTENUATION_MAJOR_0dB		(1 << 0)
#define PT2254A_ATTENUATION_MAJOR_10dB		(1 << 1)
#define PT2254A_ATTENUATION_MAJOR_20dB		(1 << 2)
#define PT2254A_ATTENUATION_MAJOR_30dB		(1 << 3)
#define PT2254A_ATTENUATION_MAJOR_40dB		(1 << 4)
#define PT2254A_ATTENUATION_MAJOR_50dB		(1 << 5)
#define PT2254A_ATTENUATION_MAJOR_60dB		(1 << 6)
#define 	PT2254A_ATTENUATION_MAJOR(x)	(1 << x)

#define PT2254A_ATTENUATION_MINOR_0dB		(1 << 7)
#define PT2254A_ATTENUATION_MINOR_2dB		(1 << 8
#define PT2254A_ATTENUATION_MINOR_4dB		(1 << 9)
#define PT2254A_ATTENUATION_MINOR_6dB		(1 << 10)
#define PT2254A_ATTENUATION_MINOR_8dB		(1 << 11)
#define 	PT2254A_ATTENUATION_MINOR(x)	(1 << (7 + x / 2))

#define PT2254A_EMPTY_BIT			(0 << 12)

#define PT2254A_BOTH_CHANNELS			(3 << 13)
#define PT2254A_LEFT_CHANNEL			(1 << 13)
#define PT2254A_RIGHT_CHANNEL			(1 << 14)

#define PT2254A_ZERO_PADDING			(0 << 15)

#define USE_CHANNEL				1

u_int32_t	pt2254a_encode_volume(u_int8_t *, u_int8_t);
u_int32_t	pt2254a_compose_register(u_int32_t, u_int32_t, int, int);

#endif /* _PT2254A_H_ */
