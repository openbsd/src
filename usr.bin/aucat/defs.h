/*	$OpenBSD: defs.h,v 1.1 2015/01/21 08:43:55 ratchov Exp $	*/
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
 * units used for MTC clock.
 */
#define MTC_SEC			2400	/* 1 second is 2400 ticks */

/*
 * limits
 */
#define NCHAN_MAX	16		/* max channel in a stream */
#define RATE_MIN	4000		/* min sample rate */
#define RATE_MAX	192000		/* max sample rate */
#define BITS_MIN	1		/* min bits per sample */
#define BITS_MAX	32		/* max bits per sample */

#endif /* !defined(DEFS_H) */
