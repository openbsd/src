/*	$OpenBSD: show.h,v 1.3 2005/02/18 04:00:21 jaredy Exp $ */

/*
 * Copyright (c) 2004 Claudio Jeker <claudio@openbsd.org>
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

#ifndef __SHOW_H__
#define __SHOW_H__

void	 p_rttables(int, int);
char	*routename(struct sockaddr *);
char	*netname(struct sockaddr *, struct sockaddr *);
char	*any_ntoa(const struct sockaddr *);
char	*ns_print(struct sockaddr *);
char	*ipx_print(struct sockaddr *);
char	*link_print(struct sockaddr *);

extern int nflag;

#endif /* __SHOW_H__ */
