/*      $OpenBSD: test2.c,v 1.2 2007/08/01 12:53:28 kurt Exp $       */

/*
 * Copyright (c) 2007 Kurt Miller <kurt@openbsd.org>
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

#include <dlfcn.h>
#include <err.h>
#include <stdio.h>

void *hidden_check = NULL;
__asm(".hidden hidden_check");

void *libaa_hidden_val = NULL;
void *libab_hidden_val = NULL;

extern	void test_aa(void);
extern	void test_bb(void);

int
main()
{
	test_aa();
	test_ab();

	if (hidden_check != NULL)
		errx(1, "hidden_check != NULL in main prog\n");

	if (libaa_hidden_val == NULL || libab_hidden_val == NULL ||
	    libaa_hidden_val == libab_hidden_val)
		errx(1, "incorrect hidden_check detected in libs\n");

	return (0);
}
