/*	$OpenBSD: bb.c,v 1.2 2010/06/20 17:57:09 phessler Exp $	*/

/*
 * Copyright (c) 2005 Kurt Miller <kurt@openbsd.org>
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

#include <stdio.h>

int weakFunction1(void) __attribute__((weak));
int weakFunction2(void) __attribute__((weak));

/* this function should be called */
int
weakFunction1()
{
	return 0;
}

/* this function should be not be called, the one it libaa should */
int
weakFunction2()
{
	return 1;
}

int
bbTest1()
{
	int ret = 0;

	/*
	 * make sure weakFunction1 from libaa (undefined weak) doesn't
	 * interfere with calling weakFunction1 here
	 */
	if (weakFunction1) {
		weakFunction1();
	} else {
		printf("weakFunction1() overwritten by undefined weak "
		    "in libaa\n");
		ret = 1;
	}

	/*
	 * make sure weakFunction2 from libaa is called instead of the
	 * one here
	 */
	if (weakFunction2()) {
		printf("wrong weakFunction2() called\n");
		ret = 1;
	}

	return (ret);
}
