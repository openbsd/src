/*	$OpenBSD: lm75reg.h,v 1.1 2004/05/23 18:12:37 grange Exp $	*/
/*	$NetBSD: lm75reg.h,v 1.1 2003/09/30 00:35:31 thorpej Exp $	*/
/*
 * Copyright (c) 2004 Alexander Yurchenko <grange@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Copyright (c) 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _DEV_I2C_LM75REG_H_
#define _DEV_I2C_LM75REG_H_

#define	LM_MODEL_LM75	1
#define	LM_MODEL_LM77	2

#define LM_POLLTIME	(hz * 2)	/* 2s */

/*
 * LM75 temperature sensor I2C address:
 *
 *	100 1xxx
 */
#define	LM75_ADDRMASK		0x78
#define	LM75_ADDR		0x48

/*
 * LM77 temperature sensor I2C address:
 *
 *	100 10xx
 */
#define	LM77_ADDRMASK		0x7c
#define	LM77_ADDR		0x48

/*
 * Temperature on the LM75 is represented by a 9-bit two's complement
 * integer in steps of 0.5C.  The following examples are taken from
 * the LM75 data sheet:
 *
 *	+125C	0 1111 1010	0x0fa
 *	+25C	0 0011 0010	0x032
 *	+0.5C	0 0000 0001	0x001
 *	0C	0 0000 0000	0x000
 *	-0.5C	1 1111 1111	0x1ff
 *	-25C	1 1100 1110	0x1ce
 *	-55C	1 1001 0010	0x192
 *
 * Temperature on the LM77 is represented by a 10-bit two's complement
 * integer in steps of 0.5C:
 *
 *	+130C	01 0000 0100	0x104
 *	+125C	00 1111 1010	0x0fa
 *	+25C	00 0011 0010	0x032
 *	+0.5C	00 0000 0001	0x001
 *	0C	00 0000 0000	0x000
 *	-0.5C	11 1111 1111	0x3ff
 *	-25C	11 1100 1110	0x3ce
 *	-55C	11 1001 0010	0x392
 */

#define	LM75_REG_TEMP			0x00
#define	LM75_REG_CONFIG			0x01
#define	LM75_REG_THYST_SET_POINT	0x02
#define	LM75_REG_TOS_SET_POINT		0x03

#define	LM77_REG_TLOW			0x04
#define	LM77_REG_THIGH			0x05

#define	LM77_TEMP_ST_LOW		0x01
#define	LM77_TEMP_ST_HIGH		0x02
#define	LM77_TEMP_ST_CRIT		0x04

#define	LM75_TEMP_LEN			2	/* 2 data bytes */

#define	LM75_CONFIG_SHUTDOWN		0x01
#define	LM75_CONFIG_CMPINT		0x02
#define	LM75_CONFIG_OSPOLARITY		0x04
#define	LM75_CONFIG_FAULT_QUEUE_1	(0 << 3)
#define	LM75_CONFIG_FAULT_QUEUE_2	(1 << 3)
#define	LM75_CONFIG_FAULT_QUEUE_4	(2 << 3)
#define	LM75_CONFIG_FAULT_QUEUE_6	(3 << 3)

#define	LM77_CONFIG_INTPOLARITY		0x08
#define	LM77_CONFIG_FAULT_QUEUE_4	0x10

/*
 * LM75 temperature word:
 *
 * MSB Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0 X X X X X X X
 * 15  14   13   12   11   10   9    8    7    6 5 4 3 2 1 0
 *
 *
 * LM77 temperature word:
 *
 * Sign Sign Sign Sign MSB Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0 Status bits
 * 15   14   13   12   11  10   9    8    7    6    5    4    3    2 1 0
 */

static __inline int
lm75_wordtotemp(u_int16_t word)
{
	return ((int16_t)word / 128);
}

static __inline u_int16_t
lm75_temptoword(int temp)
{
	int sign = 0;

	if (temp < 0) {
		sign = 1;
		temp = -temp;
	};

	return ((sign << 15 ) | ((temp & 0xff) << 7));
}

static __inline int
lm77_wordtotemp(u_int16_t word)
{
	return ((int16_t)word / 8);
}

static __inline u_int16_t
lm77_temptoword(int temp)
{
	int sign = 0;

	if (temp < 0) {
		sign = 0xf;
		temp = -temp;
	};

	return ((sign << 12 ) | ((temp & 0x1ff) << 3));
}

#endif /* _DEV_I2C_LM75REG_H_ */
