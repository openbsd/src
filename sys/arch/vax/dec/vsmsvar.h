/*	$OpenBSD: vsmsvar.h,v 1.2 2008/08/22 21:05:07 miod Exp $	*/
/*
 * Copyright (c) 2008 Miodrag Vallat.
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
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ms.c	8.1 (Berkeley) 6/11/93
 */

/*
 * Command characters
 */
#define	VS_B9600		'B'	/* T only: switch to 9600 bps */
#define	VS_REQUEST_POINT	'D'	/* stop incremental position reports */
#define	VS_FREQ_55		'K'	/* T only: 55Hz report rate */
#define	VS_FREQ_72		'L'	/* T only: 72Hz report rate */
#define	VS_FREQ_120		'M'	/* T only: 120Hz report rate, 9600bps */
#define	VS_REQUEST_POSITION	'P'	/* request position (in point mode) */
#define VS_INCREMENTAL		'R'	/* incremental position reports */
#define VS_SELF_TEST		'T'	/* reset and self test */

/*
 * Data frame types
 */

#define	FRAME_MASK		0x80
#define	FRAME_TYPE_MASK		0xe0
#define	FRAME_MOUSE		0x80	/* 1 0 0 - mouse 3 byte packet */
#define	FRAME_SELFTEST		0xa0	/* 1 0 1 - selftest 4 byte packet */
#define	FRAME_TABLET		0xc0	/* 1 1 0 - tablet 5 byte packet */

/*
 * Selftest frame layout
 *	byte 0: frame type and device revision
 *	byte 1: manufacturing location code and device type
 *	byte 2: self test result
 *	byte 3: button mask (if result == button error)
 */

/* byte 0 */
#define	FRAME_ST_REV_MASK		0x0f	/* device revision */

/* byte 1 */
#define	FRAME_ST_LOCATION_MASK		0x70
#define	FRAME_ST_DEVICE_MASK		0x0f
#define	FRAME_ST_DEVICE_MOUSE		0x02
#define	FRAME_ST_DEVICE_TABLET		0x04

/* status test error codes */
#define	ERROR_OK			0x00
#define	ERROR_TABLET_STYLUS		0x11	/* stylus only, no puck */
#define	ERROR_TABLET_NO_POINTER		0x13	/* neither stylus nor puck */
#define	ERROR_FATAL			0x20	/* fatal errors from here */
#define	ERROR_TABLET_LINK		0x3a	/* tablet internal error */
#define	ERROR_BUTTON_ERROR		0x3d	/* button malfunction */
#define	ERROR_MEMORY_CKSUM_ERROR	0x3e	/* firmware malfunction */

/*
 * Mouse frame layout
 *	byte 0: frame type, delta signs, button mask
 *	byte 1: unsigned X delta
 *	byte 2: unsigned Y delta
 */
#define	FRAME_MS_X_SIGN			0x10	/* set if positive */
#define	FRAME_MS_Y_SIGN			0x08	/* set if positive */
#define	FRAME_MS_B3			0x04	/* left button */
#define	FRAME_MS_B2			0x02	/* middle button */
#define	FRAME_MS_B1			0x01	/* right button */

/*
 * Tablet frame layout
 *	byte 0: frame type, button and proximity sensor mask
 *	byte 1: low 6 bits of absolute X position
 *	byte 2: high 6 bits of absolute X position
 *	byte 3: low 6 bits of absolute Y position
 *	byte 4: high 6 bits of absolute Y position
 */
#define	FRAME_T_B4			0x10	/* puck bottom button */
#define	FRAME_T_B3			0x08	/* puck right button */
#define	FRAME_T_B2			0x04	/* puck top / stylus tip */
#define	FRAME_T_B1			0x02	/* puck left / stylus barrel */
#define	FRAME_T_PR			0x01	/* stylus proximity (if zero) */

struct lkms_softc {		/* driver status information */
	struct	device dzms_dev;	/* required first: base device */

	int	sc_flags;
#define	MS_ENABLED		0x01	/* input enabled */
#define	MS_SELFTEST		0x02	/* selftest in progress */
#define	MS_TABLET		0x04	/* device is a tablet */
#define	MS_STYLUS		0x08	/* tablet has a stylus, not a puck */

	int	sc_frametype;		/* frame type being processed */
	u_int	sc_framepos;		/* position in the frame */
	int	sc_error;		/* selftest error result */

	u_int	buttons;
	int	dx, dy;

	struct device *sc_wsmousedev;
};

int	lkms_ioctl(void *, u_long, caddr_t, int, struct proc *);
int	lkms_input(void *, int);
