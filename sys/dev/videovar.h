/*	$OpenBSD: videovar.h,v 1.7 2010/07/14 21:24:33 jakemsr Exp $	*/
/*
 * Copyright (c) 2008 Robert Nagy <robert@openbsd.org>
 * Copyright (c) 2008 Marcus Glocker <mglocker@openbsd.org>
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
	struct device		 dev;
	void			*hw_hdl;	/* hardware driver handle */
	struct device		*sc_dev;	/* hardware device struct */
	struct video_hw_if	*hw_if;		/* hardware interface */
	char			 sc_dying;	/* device detached */
#define VIDEO_OPEN	0x01
	char			 sc_open;

	int			 sc_fsize;
	uint8_t			*sc_fbuffer;
	int			 sc_vidmode;	/* access mode */
#define		VIDMODE_NONE	0
#define		VIDMODE_MMAP	1
#define		VIDMODE_READ	2
	int			 sc_frames_ready;

	struct selinfo		 sc_rsel;	/* read selector */
};

#endif /* _SYS_DEV_VIDEOVAR_H */
