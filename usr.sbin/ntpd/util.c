/*	$OpenBSD: util.c,v 1.17 2015/02/10 06:40:08 reyk Exp $ */

/*
 * Copyright (c) 2004 Alexander Guy <alexander.guy@andern.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <limits.h>
#include <stdio.h>
#include <time.h>

#include "ntpd.h"

double
gettime_corrected(void)
{
	return (gettime() + getoffset());
}

double
getoffset(void)
{
	struct timeval	tv;
	if (adjtime(NULL, &tv) == -1)
		return (0.0);
	return (tv.tv_sec + 1.0e-6 * tv.tv_usec);
}

double
gettime(void)
{
	struct timeval	tv;

	if (gettimeofday(&tv, NULL) == -1)
		fatal("gettimeofday");

	return (tv.tv_sec + JAN_1970 + 1.0e-6 * tv.tv_usec);
}

double
gettime_from_timeval(struct timeval *tv)
{
	return (tv->tv_sec + JAN_1970 + 1.0e-6 * tv->tv_usec);
}

time_t
getmonotime(void)
{
	struct timespec	ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
		fatal("clock_gettime");

	return (ts.tv_sec);
}


void
d_to_tv(double d, struct timeval *tv)
{
	tv->tv_sec = d;
	tv->tv_usec = (d - tv->tv_sec) * 1000000;
	while (tv->tv_usec < 0) {
		tv->tv_usec += 1000000;
		tv->tv_sec -= 1;
	}
}

double
lfp_to_d(struct l_fixedpt lfp)
{
	double	ret;

	lfp.int_partl = ntohl(lfp.int_partl);
	lfp.fractionl = ntohl(lfp.fractionl);

	ret = (double)(lfp.int_partl) + ((double)lfp.fractionl / UINT_MAX);

	return (ret);
}

struct l_fixedpt
d_to_lfp(double d)
{
	struct l_fixedpt	lfp;

	lfp.int_partl = htonl((u_int32_t)d);
	lfp.fractionl = htonl((u_int32_t)((d - (u_int32_t)d) * UINT_MAX));

	return (lfp);
}

double
sfp_to_d(struct s_fixedpt sfp)
{
	double	ret;

	sfp.int_parts = ntohs(sfp.int_parts);
	sfp.fractions = ntohs(sfp.fractions);

	ret = (double)(sfp.int_parts) + ((double)sfp.fractions / USHRT_MAX);

	return (ret);
}

struct s_fixedpt
d_to_sfp(double d)
{
	struct s_fixedpt	sfp;

	sfp.int_parts = htons((u_int16_t)d);
	sfp.fractions = htons((u_int16_t)((d - (u_int16_t)d) * USHRT_MAX));

	return (sfp);
}

char *
print_rtable(int r)
{
	static char b[11];

	b[0] = 0;
	if (r > 0)
		snprintf(b, sizeof(b), "rtable %d", r);

	return(b);
}
