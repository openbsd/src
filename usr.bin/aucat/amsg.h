/*	$OpenBSD: amsg.h,v 1.16 2010/04/06 20:19:42 ratchov Exp $	*/
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
#ifndef AMSG_H
#define AMSG_H

#include <stdint.h>

/*
 * WARNING: since the protocol may be simultaneously used by static
 * binaries or by different versions of a shared library, we are not
 * allowed to change the packet binary representation in a backward
 * incompatible way.
 *
 * Especially, make sure the amsg_xxx structures are not larger
 * than 32 bytes.
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
#define AMSG_SETVOL	8	/* set volume */
#define AMSG_HELLO	9	/* say hello, check versions and so ... */
#define AMSG_BYE	10	/* ask server to drop connection */
	uint32_t cmd;
	uint32_t __pad;
	union {
		struct amsg_par {
			uint8_t legacy_mode;	/* compat for old libs */
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
			uint32_t bufsz;		/* total buffered frames */
			uint32_t round;
			uint32_t appbufsz;	/* client side bufsz */
			uint32_t _reserved[1];	/* for future use */
		} par;
		struct amsg_cap {
			uint32_t rate;		/* native rate */
			uint32_t _reserved2[1];	/* for future use */
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
		struct amsg_vol {
			uint32_t ctl;
		} vol;
		struct amsg_hello {
#define AMSG_PLAY	0x1			/* audio playback */
#define AMSG_REC	0x2			/* audio recording */
#define AMSG_MIDIIN	0x4			/* MIDI thru input */
#define AMSG_MIDIOUT	0x8			/* MIDI thru output */
#define AMSG_MON	0x10			/* audio monitoring */
#define AMSG_RECMASK	(AMSG_REC | AMSG_MON)	/* can record ? */
			uint16_t proto;		/* protocol type */
#define AMSG_VERSION	2
			uint8_t version;	/* protocol version */
			uint8_t reserved1[5];	/* for future use */
			char opt[12];		/* profile name */
			char who[12];		/* hint for leases */
		} hello;
	} u;
};

/*
 * Initialize an amsg structure: fill all fields with 0xff, so the read
 * can test which fields were set.
 */
#define AMSG_INIT(m) do { memset((m), 0xff, sizeof(struct amsg)); } while (0)

/*
 * Since the structure is memset to 0xff, the MSB can be used to check
 * if any field was set.
 */
#define AMSG_ISSET(x) (((x) & (1 << (8 * sizeof(x) - 1))) == 0)

#endif /* !defined(AMSG_H) */
