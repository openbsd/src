/*	$OpenBSD: aa.c,v 1.3 2005/09/16 23:30:25 kurt Exp $	*/

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
 *
 */

#include <dlfcn.h>
#include <stdio.h>
#include "aa.h"

int aaSymbol;

void
sigprocmask() {
}

/*
 * aaTest verifies dlsym works as expected with a simple case of duplicate
 * symbols. prog1, libaa and libc all have the sigprocmask symbol and are
 * linked with prog1 with libaa before libc. Depending on how dlsym is called
 * the symbol for sigprocmask should come from prog1, libaa or libc.
 */
int
aaTest()
{
	int ret = 0;
	void *value;
	void *libaa_handle = dlopen("libaa.so", RTLD_LAZY);
	void *libc_handle = dlopen("libc.so", RTLD_LAZY);
	void *libaa_sigprocmask = dlsym(libaa_handle, "sigprocmask");
	void *libc_sigprocmask = dlsym(libc_handle, "sigprocmask");

	dlclose(libaa_handle);
	dlclose(libc_handle);

	/* basic sanity check */
	if (libaa_sigprocmask == &sigprocmask || libc_sigprocmask == &sigprocmask ||
	    libc_sigprocmask == libaa_sigprocmask || libaa_sigprocmask == NULL ||
	    libc_sigprocmask == NULL) {
		printf("dlsym(handle, ...)\n FAILED\n");
		printf("sigprocmask       == %p\n", &sigprocmask);
		printf("libaa_sigprocmask == %p\n", libaa_sigprocmask);
		printf("libc_sigprocmask  == %p\n", libc_sigprocmask);
		return (1);
	}

	value = dlsym(RTLD_DEFAULT, "sigprocmask");
	if (value != &sigprocmask) {
		printf("dlsym(RTLD_DEFAULT, \"sigprocmask\") == %p FAILED\n", value);
		printf("\twas expecting == %p (&sigprocmask)\n", &sigprocmask);
		ret = 1;
	}

	value = dlsym(RTLD_SELF, "sigprocmask");
	if (value != libaa_sigprocmask) {
		printf("dlsym(RTLD_SELF, \"sigprocmask\") == %p FAILED\n", value);
		printf("\twas expecting == %p (libaa_sigprocmask)\n", libaa_sigprocmask);
		printf("FAILED\n");
		ret = 1;
	}

	value = dlsym(RTLD_NEXT, "sigprocmask");
	if (value != libc_sigprocmask) {
		printf("dlsym(RTLD_NEXT, \"sigprocmask\") == %p FAILED\n", value);
		printf("\twas expecting == %p (libc_sigprocmask)\n", libc_sigprocmask);
		ret = 1;
	}

	value = dlsym(NULL, "sigprocmask");
	if (value != libaa_sigprocmask) {
		printf("dlsym(NULL, \"sigprocmask\") == %p FAILED\n", value);
		printf("\twas expecting == %p (libaa_sigprocmask)\n", libaa_sigprocmask);
		ret = 1;
	}

	return (ret);
}
