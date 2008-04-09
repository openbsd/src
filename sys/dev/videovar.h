/*	$OpenBSD: videovar.h,v 1.1 2008/04/09 19:49:55 robert Exp $	*/
/*
 * Copyright (c) 2008 Robert Nagy <robert@openbsd.org>
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

#ifndef _SYS_DEV_VIDEOVAR_H
#define _SYS_DEV_VIDEOVAR_H

struct video_softc {
	struct device	dev;
	void		*hw_hdl;	/* hardware driver handle */
	struct device	*sc_dev;	/* hardware device struct */
	struct video_hw_if *hw_if;	/* hardware interface */
	char		sc_dying;	/* device detached */
};

#endif /* _SYS_DEV_VIDEOVAR_H */
