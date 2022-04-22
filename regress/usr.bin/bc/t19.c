/*	$OpenBSD: t19.c,v 1.6 2022/04/22 18:05:29 otto Exp $	*/

/*
 * Copyright (c) 2012 Otto Moerbeek <otto@drijf.net>
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
#include <float.h>
#include <math.h>
#include <stdio.h>

#define PI 3.141592653589793238462643383279502884L
int scale[] = { 1, 2, 3, 10, 15, 20, 30};
long double num[] = {-10.0L, -PI, -3.0L, -2.0L, -1.0L, -0.5L, -0.01L, 0.0L,
  0.01L, 0.5L, 1.0L, 2.0L, 3.0L, PI, 10.0L};

struct func {const char *name; long double (*f)(long double);};

struct func funcs[] = { {"s", sinl},
			{"c", cosl},
			{"e", expl},
			{"l", logl},
			{"a", atanl}
};

#define nitems(_a)      (sizeof((_a)) / sizeof((_a)[0]))

int
main(void)
{
	int ret, si, ni, fi;
	int status = 0;

	for (si = 0; si < nitems(scale); si++) {
		for (ni = 0; ni < nitems(num); ni++) {
			for (fi = 0; fi < nitems(funcs); fi++) {
				char cmd[100];
				FILE *fp;
				long double v, d1, d2, diff, prec;

				v = num[ni];
				if (v == 0.0 && funcs[fi].f == logl)
					continue;
				snprintf(cmd, sizeof(cmd),
				    "bc -l -e scale=%d -e '%s(%.36Lf)' -equit",
				    scale[si], funcs[fi].name, v);
				fp = popen(cmd, "r");
				ret = fscanf(fp, "%Lf", &d1);
				pclose(fp);
				d2 = funcs[fi].f(v);
				diff = fabsl(d1 - d2);
				prec = powl(10.0L, (long double)-scale[si]);
				if (prec < LDBL_EPSILON)
					prec = LDBL_EPSILON;
				prec *= 2;
				/* XXX cheating ? */
				if (funcs[fi].f == expl)
					prec *= 8;
				if (diff > prec) {
					printf("%s %d %Le %Le %Le %Le %Le\n",
					    funcs[fi].name, scale[si],
					    v, d1, d2, diff, prec);
					status = 1;
				}

			}
		}
	}
	return status;
}
