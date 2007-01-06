/*	$OpenBSD: isabrreg.h,v 1.1 2007/01/06 20:17:43 miod Exp $	*/

/*
 * Copyright (c) 2007 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice, this permission notice, and the disclaimer below
 * appear in all copies.
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
 * Models 4xx ISA slot memory spaces
 */

#define	ISABR_IOMEM_BASE	(0x30000000 + IOM_BEGIN)
#define	ISABR_IOMEM_END		(0x30000000 + IOM_END)

#define	ISABR_IOPORT_BASE	0x30000000
#define	ISABR_IOPORT_END	0x30080000

/*
 * Ye mighty logic by which ports are interleaved.
 * The actual scheme is more complex, to allow ports over 0x400 to be mapped
 * in bits 3-11 of the address. But we don't support the multi-slot EISA
 * bridges yet.
 */
#define	ISABR_IOPORT_LINE	0x08
#define	ISAADDR(p) \
    ((((p) & ~(ISABR_IOPORT_LINE - 1)) << 9) | ((p) & (ISABR_IOPORT_LINE - 1)))
#define	ISAPORT(a) \
    ((((a) >> 9) & ~(ISABR_IOPORT_LINE - 1)) | ((a) & (ISABR_IOPORT_LINE - 1)))
