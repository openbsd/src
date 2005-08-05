/*	$OpenBSD: apsreg.h,v 1.1 2005/08/05 03:52:32 jsg Exp $	*/
/*
 * Copyright (c) 2005 Jonathan Gray <jsg@openbsd.org>
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

#define APS_ACCEL_STATE		0x04
#define APS_INIT		0x10
#define APS_STATE		0x11
#define	APS_XACCEL		0x12
#define APS_YACCEL		0x14
#define APS_TEMP		0x16
#define	APS_XVAR		0x17
#define APS_YVAR		0x19
#define APS_TEMP2		0x1b
#define APS_UNKNOWN		0x1c
#define APS_INPUT		0x1d
#define APS_CMD			0x1f

#define	APS_STATE_NEWDATA	0x50

#define APS_CMD_START		0x01

#define APS_INPUT_KB		(1 << 5)
#define APS_INPUT_MS		(1 << 6)
#define APS_INPUT_LIDOPEN	(1 << 7)

#define APS_ADDR_SIZE		0x1f

struct sensor_rec {
	u_int8_t	state;
	u_int16_t	x_accel;
	u_int16_t	y_accel;
	u_int8_t	temp1;
	u_int16_t	x_var;
	u_int16_t	y_var;
	u_int8_t	temp2;
	u_int8_t	unk;
	u_int8_t	input;
};
