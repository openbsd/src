/*	$OpenBSD: video_if.h,v 1.2 2008/05/24 19:37:34 mglocker Exp $	*/
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

#ifndef _SYS_DEV_VIDEO_IF_H
#define _SYS_DEV_VIDEO_IF_H

/*
 * Generic interface to hardware driver
 */

#define VIDEOUNIT(x)	(minor(x))

struct video_hw_if {
	/* open hardware */
	int	(*open)(void *, int, int *, uint8_t *, void (*)(void *),
		    void *);

	/* close hardware */
	int	(*close)(void *);

	int	(*querycap)(void *, struct v4l2_capability *);
	int	(*s_fmt)(void *, struct v4l2_format *);
	int	(*g_fmt)(void *, struct v4l2_format *);
	int	(*reqbufs)(void *, struct v4l2_requestbuffers *);
	int	(*qbuf)(void *, struct v4l2_buffer *);
	int	(*dqbuf)(void *, struct v4l2_buffer *);
};

struct video_attach_args {
        void	*hwif;
        void	*hdl;
};

struct device  *video_attach_mi(struct video_hw_if *, void *, struct device *);

#endif /* _SYS_DEV_VIDEO_IF_H */
