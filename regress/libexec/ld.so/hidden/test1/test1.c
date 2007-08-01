/*      $OpenBSD: test1.c,v 1.2 2007/08/01 12:53:28 kurt Exp $       */

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

int
main()
{
	void *libaa, *libab;
	void (*hidden_test)();
	int i;

	libaa = dlopen(LIBAA, RTLD_LAZY);
	libab = dlopen(LIBAB, RTLD_LAZY);
	if (libaa == NULL)
               	errx(1, "dlopen(%s, RTLD_LAZY) FAILED\n", LIBAA);
	if (libab == NULL)
               	errx(1, "dlopen(%s, RTLD_LAZY) FAILED\n", LIBAB);

       	hidden_test = (void (*)())dlsym(libaa, "test_aa");
	if (hidden_test == NULL)
		errx(1, "dlsym(libaa, \"test_aa\") FAILED\n");

	(*hidden_test)();

       	hidden_test = (void (*)())dlsym(libab, "test_ab");
	if (hidden_test == NULL)
		errx(1, "dlsym(libab, \"test_ab\") FAILED\n");

	(*hidden_test)();

	if (hidden_check != NULL)
		errx(1, "hidden_check != NULL in main prog\n");

	if (libaa_hidden_val == NULL || libab_hidden_val == NULL ||
	    libaa_hidden_val == libab_hidden_val)
		errx(1, "incorrect hidden_check detected in libs\n");

	dlclose(libaa);
	dlclose(libab);

	return (0);
}
