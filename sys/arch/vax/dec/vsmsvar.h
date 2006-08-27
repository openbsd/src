/*	$OpenBSD: vsmsvar.h,v 1.1 2006/08/27 16:52:15 miod Exp $	*/
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
 * Command characters for the mouse.
 */
#define MOUSE_SELF_TEST		'T'
#define MOUSE_INCREMENTAL	'R'

/*
 * Mouse output bits.
 *
 *     	MOUSE_START_FRAME	Start of report frame bit.
 *	MOUSE_X_SIGN		Sign bit for X.
 *	MOUSE_Y_SIGN		Sign bit for Y.
 *	MOUSE_X_OFFSET		X offset to start cursor at.
 *	MOUSE_Y_OFFSET		Y offset to start cursor at.
 */
#define MOUSE_START_FRAME	0x80
#define MOUSE_X_SIGN		0x10
#define MOUSE_Y_SIGN		0x08

/*
 * Definitions for mouse buttons
 */
#define RIGHT_BUTTON		0x01
#define MIDDLE_BUTTON		0x02
#define LEFT_BUTTON		0x04

struct lkms_softc {		/* driver status information */
	struct	device dzms_dev;	/* required first: base device */

	int sc_enabled;		/* input enabled? */
	int sc_selftest;

	int inputstate;
	u_int buttons;
	int dx, dy;

	struct device *sc_wsmousedev;
};

int	lkms_ioctl(void *, u_long, caddr_t, int, struct proc *);
int	lkms_input(void *, int);
