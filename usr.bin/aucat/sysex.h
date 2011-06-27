/*	$OpenBSD: sysex.h,v 1.2 2011/06/27 07:17:44 ratchov Exp $	*/
/*
 * Copyright (c) 2011 Alexandre Ratchov <alex@caoua.org>
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
#ifndef AUCAT_SYSEX_H
#define AUCAT_SYSEX_H

#include <stdint.h>

/*
 * start and end markers
 */
#define SYSEX_START		0xf0
#define SYSEX_END		0xf7

/*
 * type/vendor namespace IDs we use
 */
#define SYSEX_TYPE_RT		0x7f	/* real-time universal */
#define SYSEX_TYPE_EDU		0x7d	/* non-comercial */

/*
 * realtime messages in the "universal real-time" namespace
 */
#define SYSEX_MTC		0x01		/* mtc messages */
#define   SYSEX_MTC_FULL	0x01		/* mtc full frame message */
#define SYSEX_MMC		0x06
#define   SYSEX_MMC_STOP	0x01
#define   SYSEX_MMC_START	0x02
#define   SYSEX_MMC_LOC		0x44
#define   SYSEX_MMC_LOC_LEN	0x06
#define   SYSEX_MMC_LOC_CMD	0x01

/*
 * aucat-specific messages, in the "edu" namespace
 */
#define SYSEX_AUCAT		0x23		/* aucat-specific */
#define   SYSEX_AUCAT_MIXINFO	0x01		/* mixer info */
#define   SYSEX_AUCAT_DUMPREQ	0x02		/* dump request */
#define   SYSEX_AUCAT_DUMPEND	0x03		/* end of dump */

/*
 * minimum size of sysex message we accept
 */
#define SYSEX_SIZE(m)	(5 + sizeof(struct sysex_ ## m))

/*
 * all possible system exclusive messages we support. For aucat-specific
 * messages we use the same header as real-time messages to simplify the
 * message parser
 */
struct sysex {
	uint8_t start;
	uint8_t type;				/* type or vendor id */
	uint8_t dev;				/* device or product id */
	uint8_t id0;				/* message id */
	uint8_t id1;				/* sub-id */
	union sysex_all {
		struct sysex_empty {
			uint8_t end;
		} empty;
		struct sysex_start {
			uint8_t end;
		} start;
		struct sysex_stop {
			uint8_t end;
		} stop;
		struct sysex_loc {
			uint8_t len;
			uint8_t cmd;
			uint8_t hr;
			uint8_t min;
			uint8_t sec;
			uint8_t fr;
			uint8_t cent;
			uint8_t end;
		} loc;
		struct sysex_full {
			uint8_t hr;
			uint8_t min;
			uint8_t sec;
			uint8_t fr;
			uint8_t end;
		} full;
		struct sysex_mixinfo {
			uint8_t chan;			/* channel */
			uint8_t vol;			/* current volume */
#define SYSEX_NAMELEN	10				/* \0 included */
			uint8_t name[SYSEX_NAMELEN];	/* stream name */
			uint8_t end;
		} mixinfo;
		struct sysex_dumpreq {
			uint8_t end;
		} dumpreq;
		struct sysex_dumpend {
			uint8_t end;
		} dumpend;
	} u;
};

#endif /* !defined(AUCAT_SYSEX_H) */
