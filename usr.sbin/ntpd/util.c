/*	$OpenBSD: util.c,v 1.10 2004/12/08 15:47:38 mickey Exp $ */

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

#include <sys/time.h>
#include <limits.h>

#include "ntpd.h"

double
gettime(void)
{
	struct timeval	tv;

	if (gettimeofday(&tv, NULL) == -1)
		fatal("gettimeofday");

	return (tv.tv_sec + JAN_1970 + 1.0e-6 * tv.tv_usec);
}


void
d_to_tv(double d, struct timeval *tv)
{
	tv->tv_sec = (long)d;
	tv->tv_usec = (d - tv->tv_sec) * 1000000;
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
