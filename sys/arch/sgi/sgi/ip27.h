/*	$OpenBSD: ip27.h,v 1.2 2009/11/07 14:49:01 miod Exp $	*/

/*
 * Copyright (c) 2009 Miodrag Vallat.
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
 * Non-XBow related IP27 and IP35 definitions
 */

/* NMI register save areas */

#define	IP27_NMI_KREGS_BASE		0x11400
#define	IP27_NMI_KREGS_SIZE		0x200	/* per CPU */
#define	IP27_NMI_EFRAME_BASE		0x11800

#define	IP35_NMI_KREGS_BASE		0x9000
#define	IP35_NMI_KREGS_SIZE		0x400	/* per CPU */
#define	IP35_NMI_EFRAME_BASE		0xa000

/* IP35 Brick types */

#define	IP35_O350	0x02
#define	IP35_FUEL	0x04
#define	IP35_O300	0x08
