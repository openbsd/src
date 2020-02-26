/*	$OpenBSD: defs.h,v 1.5 2020/02/26 13:53:58 ratchov Exp $	*/
/*
 * Copyright (c) 2008-2012 Alexandre Ratchov <alex@caoua.org>
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
#ifndef DEFS_H
#define DEFS_H

/*
 * MIDI buffer size
 */
#define MIDI_BUFSZ		3125	/* 1 second at 31.25kbit/s */

/*
 * units used for MTC clock. Must allow a quarter of frame to be
 * represented at any of the standard 24, 25, or 30 fps.
 */
#define MTC_SEC			2400	/* 1 second is 2400 ticks */

/*
 * device or sub-device mode, must be a superset of corresponding SIO_
 * and MIO_ constants
 */
#define MODE_PLAY	0x01	/* allowed to play */
#define MODE_REC	0x02	/* allowed to rec */
#define MODE_MIDIOUT	0x04	/* allowed to read midi */
#define MODE_MIDIIN	0x08	/* allowed to write midi */
#define MODE_MON	0x10	/* allowed to monitor */
#define MODE_CTLREAD	0x100	/* allowed to read controls */
#define MODE_CTLWRITE	0x200	/* allowed to change controls */
#define MODE_RECMASK	(MODE_REC | MODE_MON)
#define MODE_AUDIOMASK	(MODE_PLAY | MODE_REC | MODE_MON)
#define MODE_MIDIMASK	(MODE_MIDIIN | MODE_MIDIOUT)
#define MODE_CTLMASK	(MODE_CTLREAD | MODE_CTLWRITE)

/*
 * underrun/overrun policies, must be the same as SIO_ constants
 */
#define XRUN_IGNORE	0	/* on xrun silently insert/discard samples */
#define XRUN_SYNC	1	/* catchup to sync to the mix/sub */
#define XRUN_ERROR	2	/* xruns are errors, eof/hup buffer */

/*
 * limits
 */
#define NCHAN_MAX	64		/* max channel in a stream */
#define RATE_MIN	4000		/* min sample rate */
#define RATE_MAX	192000		/* max sample rate */
#define BITS_MIN	1		/* min bits per sample */
#define BITS_MAX	32		/* max bits per sample */

#endif /* !defined(DEFS_H) */
