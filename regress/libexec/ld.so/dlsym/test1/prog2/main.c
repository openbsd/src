/*	$OpenBSD: main.c,v 1.2 2005/09/16 23:30:25 kurt Exp $	*/

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
#include "aa.h"

/*
 * Verifies dlsym works as expected when called from the main executable.
 * libaa and libc are in the main object group, so their symbols are visable.
 */
int
main()
{
	int ret = 0;
	void *exe_handle = dlopen(NULL, RTLD_LAZY);

	if (dlsym(RTLD_DEFAULT, "aaSymbol") == NULL) {
		printf("dlsym(RTLD_DEFAULT, \"aaSymbol\") FAILED\n");
		ret = 1;
	}

	if (dlsym(RTLD_SELF, "aaSymbol") == NULL) {
		printf("dlsym(RTLD_SELF, \"aaSymbol\") FAILED\n");
		ret = 1;
	}

	if (dlsym(RTLD_NEXT, "aaSymbol") == NULL) {
		printf("dlsym(RTLD_NEXT, \"aaSymbol\") FAILED\n");
		ret = 1;
	}

	if (dlsym(NULL, "aaSymbol") == NULL) {
		printf("dlsym(RTLD_NEXT, \"aaSymbol\") FAILED\n");
		ret = 1;
	}

	if (dlsym(exe_handle, "aaSymbol") == NULL) {
		printf("dlsym(exe_handle, \"aaSymbol\") FAILED\n");
		ret = 1;
	}

	dlclose(exe_handle);

	return (ret);
}
