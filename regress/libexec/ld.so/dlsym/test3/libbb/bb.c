/*	$OpenBSD: bb.c,v 1.2 2005/09/19 21:50:27 drahn Exp $	*/

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

#include <dlfcn.h>
#include <stdio.h>

int bbSymbol;
int commonSymbol;

/*
 * this test is setup where the main program group dlopen's libbb and libdd
 * without RTLD_GLOBAL. libcc is part of libbb load group. libee is part of
 * libdd load group.
 */
int
bbTest1(void *libbb, void *libdd)
{
	int ret = 0;

	/* check RTLD_DEFAULT can *not* see symbols in libdd object group */
	if (dlsym(RTLD_DEFAULT, "ddSymbol") != NULL) {
		printf("dlsym(RTLD_DEFAULT, \"ddSymbol\") != NULL\n");
		ret = 1;
	}

	/* check RTLD_SELF can *not* see symbols in libdd object group */
	if (dlsym(RTLD_SELF, "ddSymbol") != NULL) {
		printf("dlsym(RTLD_SELF, \"ddSymbol\") != NULL\n");
		ret = 1;
	}

	/* check RTLD_NEXT can *not* see symbols in libdd object group */
	if (dlsym(RTLD_NEXT, "ddSymbol") != NULL) {
		printf("dlsym(RTLD_NEXT, \"ddSymbol\") != NULL\n");
		ret = 1;
	}

	/* check NULL can *not* see symbols in libdd object group */
	if (dlsym(NULL, "ddSymbol") != NULL) {
		printf("dlsym(NULL, \"ddSymbol\") != NULL\n");
		ret = 1;
	}

	/* libbb should *not* see symbols in libdd or libee */
	if (dlsym(libbb, "ddSymbol") != NULL) {
		printf("dlsym(libbb, \"ddSymbol\") != NULL\n");
		ret = 1;
	}

	/* libdd should *not* see symbols in libbb or libcc */
	if (dlsym(libdd, "bbSymbol") != NULL) {
		printf("dlsym(libdd, \"bbSymbol\") != NULL\n");
		ret = 1;
	}

	/* libbb should see symbols in libbb and libcc */
	if (dlsym(libbb, "ccSymbol") == NULL) {
		printf("dlsym(libbb, \"ccSymbol\") == NULL\n");
		ret = 1;
	}

	/* libdd should see symbols in libbb and libee */
	if (dlsym(libdd, "eeSymbol") == NULL) {
		printf("dlsym(libdd, \"eeSymbol\") == NULL\n");
		ret = 1;
	}

	return (ret);
}

/*
 * this test is setup where the main program group dlopen's libbb and libdd.
 * libdd is opened with RTLD_GLOBAL. libcc is part of libbb load group. libee
 * is part of libdd load group.
 */
int
bbTest2()
{
	int ret = 0;

	/* check RTLD_DEFAULT can see symbols in libdd object group */
	if (dlsym(RTLD_DEFAULT, "eeSymbol") != NULL) {
		printf("dlsym(RTLD_DEFAULT, \"ddSymbol\") != NULL\n");
		ret = 1;
	}

	/* check RTLD_SELF can see symbols in libdd object group */
	if (dlsym(RTLD_SELF, "eeSymbol") != NULL) {
		printf("dlsym(RTLD_SELF, \"ddSymbol\") != NULL\n");
		ret = 1;
	}

	/* check RTLD_NEXT can see symbols in libdd object group */
	if (dlsym(RTLD_NEXT, "eeSymbol") != NULL) {
		printf("dlsym(RTLD_NEXT, \"ddSymbol\") != NULL\n");
		ret = 1;
	}

	return (ret);
}
