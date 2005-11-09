/*	$OpenBSD: main.c,v 1.3 2005/11/09 16:36:06 kurt Exp $	*/

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

int mainSymbol;

/*
 * This tests that the main object group can see the symbols in
 * objects dlopened with RTLD_GLOBAL for RTLD_{DEFAULT,NEXT,SELF},
 * but not with NULL, or with a handle.
 */
int
main()
{
	int ret = 0;
	void *exe_handle = dlopen(NULL, RTLD_LAZY);
	void *libbb = dlopen("libbb.so", RTLD_LAZY|RTLD_GLOBAL);

	if (libbb == NULL) {
		printf("dlopen(\"libbb.so\", RTLD_LAZY|RTLD_GLOBAL) FAILED\n");
		return (1);
	}

	/* RTLD_DEFAULT should see bbSymbol */
	if (dlsym(RTLD_DEFAULT, "bbSymbol") == NULL) {
		printf("dlsym(RTLD_DEFAULT, \"bbSymbol\") == NULL\n");
		ret = 1;
	}

	/* RTLD_SELF should see bbSymbol (RTLD_GLOBAL load group) */
	if (dlsym(RTLD_SELF, "bbSymbol") == NULL) {
		printf("dlsym(RTLD_SELF, \"bbSymbol\") == NULL\n");
		ret = 1;
	}

	/* RTLD_NEXT should see bbSymbol (RTLD_GLOBAL load group) */
	if (dlsym(RTLD_NEXT, "bbSymbol") == NULL) {
		printf("dlsym(RTLD_NEXT, \"bbSymbol\") == NULL\n");
		ret = 1;
	}

	/* NULL should *not* see bbSymbol (different load group) */
	if (dlsym(NULL, "bbSymbol") != NULL) {
		printf("dlsym(NULL, \"bbSymbol\") != NULL\n");
		ret = 1;
	}

	/* exe handle should see bbSymbol (Same as RTLD_DEFAULT) */
	if (dlsym(exe_handle, "bbSymbol") == NULL) {
		printf("dlsym(exe_handle, \"bbSymbol\") == NULL\n");
		ret = 1;
	}

	dlclose(exe_handle);
	dlclose(libbb);

	return (ret);
}
