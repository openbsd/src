/*	$OpenBSD: pesach.c,v 1.4 2015/03/15 00:41:28 millert Exp $	*/

/*
 * Copyright (c) 2004 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>

#include "calendar.h"

/* Calculate the Julian date of Pesach using the Gauss formula */

#define	T	(33. + 14. / 24.)
#define	L	((1.  + 485. / 1080.) / 24. / 19.)
#define	K	((29. + (12. + 793. / 1080.) / 24. ) / 19.)

int
pesach(int R)
{
	int a, b, y, cumdays;
	double d;

	y = R + 3760;

	a = (12 * y + 17) % 19;
	b = y % 4;
	d = (T - 10 * K + L + 14) + K * a +  b / 4. - L * y;
	cumdays = d;

	/* the postponement */
	switch ((int)(cumdays + 3 * y + 5 * b + 5) % 7) {
	case 1:
		if (a > 6 && d - cumdays >= (15. + 204. / 1080.) / 24.)
			cumdays += 2;
		break;

	case 0:
		if (a <= 11 || d - cumdays < (21. + 589. / 1080.) / 24.)
			break;
		/* FALLTHROUGH */
	case 2:
	case 4:
	case 6:
		cumdays++;
		break;
	}

	if (R > 1582)
		cumdays += R / 100 - R /400 - 2;

	return (31 + 28 + cumdays + (isleap(R)? 1 : 0));
}
