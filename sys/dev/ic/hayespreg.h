/*	$NetBSD: hayespreg.h,v 1.1 1996/02/10 20:23:40 christos Exp $	*/

/*-
 * Copyright (c) 1995  Sean E. Fagin, John M Vinopal.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _HAYESPREG_H_
#define	_HAYESPREG_H_

/*
 * Definitions for Hayes ESP serial cards.
 */

/*
 * CMD1 and CMD2 are the command ports, offsets from <hayesp_iobase>.
 */
#define	HAYESP_CMD1	4
#define	HAYESP_CMD2	5

/*
 * STAT1 and STAT2 are to get return values and status bytes
 */
#define	HAYESP_STATUS1	HAYESP_CMD1
#define	HAYESP_STATUS2	HAYESP_CMD2

/*
 * Commands.  Commands are given by writing the command value to
 * HAYESP_CMD1 and then writing or reading some number of bytes from
 * HAYESP_CMD2 or HAYESP_STATUS2.
 */
#define	HAYESP_GETTEST		0x01	/* self-test command (1b+extras) */
#define	HAYESP_GETDIPS		0x02	/* get on-board DIP switches (1b) */
#define	HAYESP_SETFLOWTYPE	0x08	/* set type of flow-control (2b) */
#define	HAYESP_SETRXFLOW	0x0a	/* set Rx FIFO " levels (4b) */
#define	HAYESP_SETMODE		0x10	/* set board mode (1b) */

/* Mode bits (HAYESP_SETMODE). */
#define	HAYESP_MODE_FIFO	0x02	/* act like a 16550 (compat mode) */
#define	HAYESP_MODE_RTS		0x04	/* use RTS hardware flow control */
#define	HAYESP_MODE_SCALE	0x80	/* scale FIFO trigger levels */

/* Flow control type bits (HAYESP_SETFLOWTYPE). */
#define	HAYESP_FLOW_RTS	0x04	/* cmd1: local Rx sends RTS flow control */
#define	HAYESP_FLOW_CTS	0x10	/* cmd2: local transmitter responds to CTS */

/* Used by HAYESP_SETRXFLOW. */
#define	HAYESP_RXHIWMARK	768
#define	HAYESP_RXLOWMARK	512
#define	HAYESP_HIBYTE(w)	(((w) >> 8) & 0xff)
#define	HAYESP_LOBYTE(w)	((w) & 0xff)

#endif /* !_HAYESPREG_H_ */
