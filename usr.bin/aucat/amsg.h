/*	$OpenBSD: amsg.h,v 1.1 2008/10/26 08:49:43 ratchov Exp $	*/
/*
 * Copyright (c) 2008 Alexandre Ratchov <alex@caoua.org>
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
#ifndef SOCKET_H
#define SOCKET_H

#include <stdint.h>

/*
 * WARNING: since the protocol may be simultaneously used by static
 * binaries or by different versions of a shared library, we are not
 * allowed to change the packet binary representation in a backward
 * incompatible way.
 */
struct amsg {
#define AMSG_ACK	0	/* ack for START/STOP */
#define AMSG_GETPAR	1	/* get the current parameters */
#define AMSG_SETPAR	2	/* set the current parameters */
#define AMSG_START	3	/* request the server to start the stream */
#define AMSG_STOP	4	/* request the server to stop the stream */ 
#define AMSG_DATA	5	/* data block */
#define AMSG_MOVE	6	/* position changed */
#define AMSG_GETCAP	7	/* get capabilities */
	uint32_t cmd;
	uint32_t __pad;
	union {
		struct amsg_par {
#define AMSG_PLAY	1			/* will play */
#define AMSG_REC	2			/* will record */
			uint8_t mode;		/* a bitmap of above */
#define AMSG_IGNORE	0			/* loose sync */
#define AMSG_SYNC	1			/* resync after xrun */
#define AMSG_ERROR	2			/* kill the stream */
			uint8_t xrun;		/* one of above */
			uint8_t bps;		/* bytes per sample */
			uint8_t bits;		/* actually used bits */
			uint8_t msb;		/* 1 if MSB justified */
			uint8_t le;		/* 1 if little endian */
			uint8_t sig;		/* 1 if signed */
			uint8_t __pad1;
			uint16_t pchan;		/* play channels */
			uint16_t rchan;		/* record channels */
			uint32_t rate;		/* frames per second */
			uint32_t bufsz;		/* buffered frames */
			uint32_t round;
			uint32_t _reserved[2];	/* for future use */
		} par;
		struct amsg_cap {
			uint32_t rate;		/* native rate */
			uint32_t rate_div;	/* divisor of emul. rates */
			uint16_t rchan;		/* native rec channels */
			uint16_t pchan;		/* native play channels */
			uint8_t bits;		/* native bits per sample */
			uint8_t bps;		/* native ytes per sample */
			uint8_t _reserved[10];	/* for future use */
		} cap;
		struct amsg_data {
#define AMSG_DATAMAX	0x1000
			uint32_t size;
		} data;
		struct amsg_ts {
			int32_t delta;
		} ts;
	} u;
};

/*
 * initialize an amsg structure: fill all fields with 0xff, so the read
 * can test which fields were set
 */
#define AMSG_INIT(m) do { memset((m), 0xff, sizeof(struct amsg)); } while (0)

/*
 * since the structure is memset to 0xff, the MSB can be used to check
 * if any filed was set
 */
#define AMSG_ISSET(x) (((x) & (1 << (8 * sizeof(x) - 1))) == 0)

#endif /* !defined(SOCKET_H) */
