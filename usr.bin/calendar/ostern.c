/*	$OpenBSD: ostern.c,v 1.7 2009/10/27 23:59:36 deraadt Exp $	*/

/*
 * Copyright (c) 1996 Wolfram Schneider <wosch@FreeBSD.org>. Berlin.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * 	$Id: ostern.c,v 1.7 2009/10/27 23:59:36 deraadt Exp $
 */

#include <stdio.h>
#include <time.h>
#include <tzfile.h>

#include "calendar.h"

/* return year day for Easter */

int
easter(int year)	/* 0 ... abcd, NOT since 1900 */
{
	int	e_a, e_b, e_c, e_d, e_e,e_f, e_g, e_h, e_i, e_k;
	int	e_l, e_m, e_n, e_p, e_q;

	/* silly, but it works */
	e_a = year % 19;
	e_b = year / 100;
	e_c = year % 100;

	e_d = e_b / 4;
	e_e = e_b % 4;
	e_f = (e_b + 8) / 25;
	e_g = (e_b + 1 - e_f) / 3;
	e_h = ((19 * e_a) + 15 + e_b - (e_d + e_g)) % 30;
	e_i = e_c / 4;
	e_k = e_c % 4;
	e_l = (32 + 2 * e_e + 2 * e_i - (e_h + e_k)) % 7;
	e_m = (e_a + 11 * e_h + 22 * e_l) / 451;
	e_n = (e_h + e_l + 114 - (7 * e_m)) / 31;
	e_p = (e_h + e_l + 114 - (7 * e_m)) % 31;
	e_p = e_p + 1;

	e_q = 31 + 28 + e_p;
	if (isleap(year))
		e_q++;

	if (e_n == 4)
	e_q += 31;

#if DEBUG
	printf("%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
	    e_a, e_b, e_c, e_d, e_e, e_f, e_g, e_h,
	    e_i, e_k, e_l, e_m, e_n, e_p, e_q);
#endif

	return (e_q);
}
