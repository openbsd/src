/*	$OpenBSD: linux_time.h,v 1.4 2013/10/25 04:51:39 guenther Exp $	*/
/*
 * Copyright (c) 2011 Paul Irofti <pirofti@openbsd.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
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

#ifndef _LINUX_TIME_H_
#define _LINUX_TIME_H_

/* BSD to linux can fail with EOVERFLOW */
int	bsd_to_linux_timespec(struct linux_timespec *,
	    const struct timespec *);
int	bsd_to_linux_itimerval(struct linux_itimerval *,
	    const struct itimerval *);

/* linux to BSD can't fail for time_t-based stuff, but can for clockid_t */
void	linux_to_bsd_timespec(struct timespec *,
	    const struct linux_timespec *);
void	linux_to_bsd_itimerval(struct itimerval *,
	    const struct linux_itimerval *);
int	linux_to_bsd_clockid(clockid_t *, clockid_t);


/* the timespec conversion functions also handle timeval */
static inline int
bsd_to_linux_timeval(struct linux_timeval *ltp, const struct timeval *ntp)
{
	return (bsd_to_linux_timespec((void *)ltp, (const void *)ntp));
}
static inline void
linux_to_bsd_timeval(struct timeval *ntp, const struct linux_timeval *ltp)
{
	linux_to_bsd_timespec((void *)ntp, (const void *)ltp);
}


#endif
