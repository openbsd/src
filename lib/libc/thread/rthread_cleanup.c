/*	$OpenBSD: rthread_cleanup.c,v 1.1 2017/10/28 21:23:14 guenther Exp $ */
/*
 * Copyright (c) 2017 Philip Guenther <guenther@openbsd.org>
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

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <syslog.h>

#include "thread_private.h"

/* If this is static then clang will optimize it away */
uintptr_t cleanup_cookie[4] __dso_hidden
    __attribute__((section(".openbsd.randomdata")));

void
_thread_cleanup_push(void (*fn)(void *), void *arg,
    struct __thread_cleanup *tc)
{
	pthread_t self = pthread_self();

	tc->__tc_magic	= cleanup_cookie[0];
	tc->__tc_next	= cleanup_cookie[1] ^ (uintptr_t)self->cleanup_fns;
	tc->__tc_fn	= cleanup_cookie[2] ^ (uintptr_t)fn;
	tc->__tc_arg	= cleanup_cookie[3] ^ (uintptr_t)arg;
	self->cleanup_fns = tc;
}
DEF_STRONG(_thread_cleanup_push);

static const char bad_magic[] = ": corrupted buffer in pthread_cleanup_pop";
static const char bad_pop[] = ": non-LIFO pthread_cleanup_pop";

void
_thread_cleanup_pop(int execute, struct __thread_cleanup *tc)
{
	pthread_t self = pthread_self();

	if (__predict_false(tc->__tc_magic != cleanup_cookie[0] ||
	    tc != self->cleanup_fns)) {
		char buf[1024];
		const char *msg;
		size_t msgsiz;

		/* <10> is LOG_CRIT */
		strlcpy(buf, "<10>", sizeof buf);

		/* Make sure progname does not fill the whole buffer */
		if (tc->__tc_magic != cleanup_cookie[0]) {
			msgsiz = sizeof bad_magic;
			msg = bad_magic;
		} else {
			msgsiz = sizeof bad_pop;
			msg = bad_pop;
		}
		strlcat(buf, __progname, sizeof(buf) - msgsiz);
		strlcat(buf, msg, sizeof buf);

		sendsyslog(buf, strlen(buf), LOG_CONS);
		abort();
	}

	self->cleanup_fns = (void *)(cleanup_cookie[1] ^ tc->__tc_next);
	if (execute) {
		void (*fn)(void *), *arg;

		fn  = (void *)(cleanup_cookie[2] ^ tc->__tc_fn);
		arg = (void *)(cleanup_cookie[3] ^ tc->__tc_arg);
		fn(arg);
	}
}
DEF_STRONG(_thread_cleanup_pop);
