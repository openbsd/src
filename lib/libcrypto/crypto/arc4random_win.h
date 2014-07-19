/*	$OpenBSD: arc4random_win.h,v 1.2 2014/07/19 00:08:43 deraadt Exp $	*/

/*
 * Copyright (c) 1996, David Mazieres <dm@uun.org>
 * Copyright (c) 2008, Damien Miller <djm@openbsd.org>
 * Copyright (c) 2013, Markus Friedl <markus@openbsd.org>
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
 * Stub functions for portability.
 */

static inline int
_rs_allocate(struct _rs **rsp, struct _rsx **rsxp)
{
	*rsp = calloc(1, sizeof(**rsp));
	if (*rsp == NULL)
		return (-1);

	*rsxp = calloc(1, sizeof(**rsxp));
	if (*rsxp == NULL) {
		free(*rsp);
		return (-1);
	}
	return (0);
}

static inline void
_rs_forkhandler(void)
{
}

static inline void
_rs_forkdetect(void)
{
}
