/*	$OpenBSD: arc4random_win.h,v 1.5 2015/01/15 06:57:18 deraadt Exp $	*/

/*
 * Copyright (c) 1996, David Mazieres <dm@uun.org>
 * Copyright (c) 2008, Damien Miller <djm@openbsd.org>
 * Copyright (c) 2013, Markus Friedl <markus@openbsd.org>
 * Copyright (c) 2014, Theo de Raadt <deraadt@openbsd.org>
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

/*
 * Stub functions for portability.
 */

#include <windows.h>

static volatile HANDLE arc4random_mtx = NULL;

/*
 * Initialize the mutex on the first lock attempt. On collision, each thread
 * will attempt to allocate a mutex and compare-and-swap it into place as the
 * global mutex. On failure to swap in the global mutex, the mutex is closed.
 */
#define _ARC4_LOCK() { \
	if (!arc4random_mtx) { \
		HANDLE p = CreateMutex(NULL, FALSE, NULL); \
		if (InterlockedCompareExchangePointer((void **)&arc4random_mtx, (void *)p, NULL)) \
			CloseHandle(p); \
	} \
	WaitForSingleObject(arc4random_mtx, INFINITE); \
} \

#define _ARC4_UNLOCK() ReleaseMutex(arc4random_mtx)

static inline void
_getentropy_fail(void)
{
	TerminateProcess(GetCurrentProcess(), 0);
}

static inline int
_rs_allocate(struct _rs **rsp, struct _rsx **rsxp)
{
	*rsp = calloc(1, sizeof(**rsp));
	if (*rsp == NULL)
		return (-1);

	*rsxp = calloc(1, sizeof(**rsxp));
	if (*rsxp == NULL) {
		free(*rsp);
		return (-1);
	}
	return (0);
}

static inline void
_rs_forkhandler(void)
{
}

static inline void
_rs_forkdetect(void)
{
}
