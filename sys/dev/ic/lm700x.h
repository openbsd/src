/*	$OpenBSD: lm700x.h,v 1.2 2001/12/06 16:28:18 mickey Exp $	*/
/* $RuOBSD: lm700x.h,v 1.2 2001/10/18 16:51:36 pva Exp $ */

/*
 * Copyright (c) 2001 Vladimir Popov <jumbo@narod.ru>
 * All rights reserved.
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

#ifndef _LM700X_H_
#define _LM700X_H_

#include <sys/types.h>

#include <machine/bus.h>

#define LM700X_REGISTER_LENGTH	24

#define LM700X_DATA_MASK	0xFFC000
#define LM700X_FREQ_MASK	0x003FFF

#define LM700X_FREQ(x)		(x << 0)	/* 0x003FFF */
#define LM700X_LSI(x)		(x << 14)	/* 0x00C000 */ /* always zero */
#define LM700X_BAND(x)		(x << 16)	/* 0x070000 */
#define		LM700X_STEREO	LM700X_BAND(3)
#define		LM700X_MONO	LM700X_BAND(1)
#define LM700X_TIME_BASE(x)	(x << 19)	/* 0x080000 */ /* always zero */
#define LM700X_REF_FREQ(x)	(x << 20)	/* 0x700000 */
#define		LM700X_REF_100	LM700X_REF_FREQ(0)
#define		LM700X_REF_025	LM700X_REF_FREQ(2)
#define		LM700X_REF_050	LM700X_REF_FREQ(4)
/* The rest is for an AM band */

#define LM700X_DIVIDER_AM	(0 << 23)	/* 0x000000 */
#define LM700X_DIVIDER_FM	(1 << 23)	/* 0x800000 */

#define LM700X_WRITE_DELAY	6		/* 6 microseconds */

struct lm700x_t {
	bus_space_tag_t	iot;
	bus_space_handle_t	ioh;
	bus_size_t	offset;

	u_int32_t	wzcl;	/* write zero clock low */
	u_int32_t	wzch;	/* write zero clock high */
	u_int32_t	wocl;	/* write one clock low */
	u_int32_t	woch;	/* write one clock high */
	u_int32_t	initdata;
	u_int32_t	rsetdata;

	void (*init)(bus_space_tag_t, bus_space_handle_t, bus_size_t, u_int32_t);
	void (*rset)(bus_space_tag_t, bus_space_handle_t, bus_size_t, u_int32_t);
};

u_int32_t	lm700x_encode_freq(u_int32_t, u_int32_t);
u_int32_t	lm700x_encode_ref(u_int8_t);
u_int8_t	lm700x_decode_ref(u_int32_t);
void	lm700x_hardware_write(struct lm700x_t *, u_int32_t, u_int32_t);

#endif /* _LM700X_H_ */
