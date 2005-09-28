/*	$OpenBSD: bb.c,v 1.3 2005/09/28 14:57:10 kurt Exp $	*/

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

/*
 * bbTest1 checks dlsym symbol visibility from a single dlopened object
 * group that looks like libbb -> libcc. This was opened from libaa in
 * the main object group. So the default search order will be prog2 ->
 * libaa -> libbb -> libcc. only dlsym(RTLD_DEFAULT,...) should see
 * symbols in prog2 and libaa. the rest are limited to libbb and libcc.
 */
int
bbTest1(void *libbb)
{
	int ret = 0;

	/* check RTLD_DEFAULT can see symbols in main object group */
	if (dlsym(RTLD_DEFAULT, "mainSymbol") == NULL) {
		printf("dlsym(RTLD_DEFAULT, \"mainSymbol\") == NULL\n");
		ret = 1;
	}

	/* check RTLD_DEFAULT can see symbols in the libbb object group */
	if (dlsym(RTLD_DEFAULT, "bbSymbol") == NULL) {
		printf("dlsym(RTLD_DEFAULT, \"bbSymbol\") == NULL\n");
		ret = 1;
	}

	/* check RTLD_SELF can *not* see symbols in main object group */
	if (dlsym(RTLD_SELF, "aaSymbol") != NULL) {
		printf("dlsym(RTLD_SELF, \"aaSymbol\") != NULL\n");
		ret = 1;
	}

	/* check RTLD_NEXT can *not* see symbols in main object group */
	if (dlsym(RTLD_NEXT, "aaSymbol") != NULL) {
		printf("dlsym(RTLD_NEXT, \"aaSymbol\") != NULL\n");
		ret = 1;
	}

	/* check NULL can *not* see symbols in main object group */
	if (dlsym(NULL, "aaSymbol") != NULL) {
		printf("dlsym(NULL, \"aaSymbol\") != NULL\n");
		ret = 1;
	}

	/* check RTLD_SELF can see symbols in local object group */
	if (dlsym(RTLD_SELF, "ccSymbol") == NULL) {
		printf("dlsym(RTLD_SELF, \"ccSymbol\") == NULL\n");
		ret = 1;
	}

	/* check RTLD_NEXT can see symbols in local object group */
	if (dlsym(RTLD_NEXT, "ccSymbol") == NULL) {
		printf("dlsym(RTLD_NEXT, \"ccSymbol\") == NULL\n");
		ret = 1;
	}

	/* check NULL can see symbols in local object group */
	if (dlsym(NULL, "ccSymbol") == NULL) {
		printf("dlsym(NULL, \"ccSymbol\") == NULL\n");
		ret = 1;
	}

	/* check RTLD_NEXT skips libbb and can't find bbSymbol */
	if (dlsym(RTLD_NEXT, "bbSymbol") != NULL) {
		printf("dlsym(RTLD_NEXT, \"bbSymbol\") != NULL\n");
		ret = 1;
	}

	/* check dlsym(libbb,..) can *not* see symbols in libaa */
	if (dlsym(libbb, "aaSymbol") != NULL) {
		printf("dlsym(libbb, \"aaSymbol\") != NULL\n");
		ret = 1;
	}

	/* check dlsym(libbb,..) can see symbols in libcc */
	if (dlsym(libbb, "ccSymbol") == NULL) {
		printf("dlsym(libbb, \"ccSymbol\") == NULL\n");
		ret = 1;
	}

	return (ret);
}
