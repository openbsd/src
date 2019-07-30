/*	$OpenBSD: thread_atexit.c,v 1.1 2017/12/16 20:06:56 guenther Exp $ */
/*
 * Copyright (c) 2017 Mark Kettenis <kettenis@openbsd.org>
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
#include <elf.h>
#pragma weak _DYNAMIC
#include <stdlib.h>
#include <tib.h>

#include "atexit.h"

typeof(dlctl) dlctl asm("_dlctl") __attribute__((weak));

__weak_alias(__cxa_thread_atexit, __cxa_thread_atexit_impl);

int
__cxa_thread_atexit_impl(void (*func)(void *), void *arg, void *dso)
{
	struct thread_atexit_fn *fnp;
	struct tib *tib = TIB_GET();

	fnp = calloc(1, sizeof(struct thread_atexit_fn));
	if (fnp == NULL)
		return -1;

	if (_DYNAMIC)
		dlctl(NULL, DL_REFERENCE, dso);

	fnp->func = func;
	fnp->arg = arg;
	fnp->next = tib->tib_atexit;
	tib->tib_atexit = fnp;

	return 0;
}
