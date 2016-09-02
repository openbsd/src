/*
 * Copyright (c) 2014 Philip Guenther <guenther@openbsd.org>
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
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include "thread_private.h"

void
_thread_set_callbacks(const struct thread_callbacks *cb, size_t len)
{
	sigset_t allmask, omask;

	if (sizeof(*cb) != len) {
		fprintf(stderr, "library mismatch: libc expected %zu but"
		    " libpthread gave %zu\n", sizeof(*cb), len);
		fflush(stderr);
		_exit(44);
	}

	sigfillset(&allmask);
	if (sigprocmask(SIG_BLOCK, &allmask, &omask) == 0) {
		/* mprotect RW */
		memcpy(&_thread_cb, cb, sizeof(_thread_cb));
		/* mprotect RO | LOCKPERM | NOUNMAP */
		sigprocmask(SIG_SETMASK, &omask, NULL);
	}
}
