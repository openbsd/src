/* $OpenBSD: unithread_mutex.c,v 1.1 2007/06/05 18:11:48 kurt Exp $ */

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

#include <sys/types.h>
#include "thread_private.h"

WEAK_PROTOTYPE(_thread_mutex_lock);
WEAK_PROTOTYPE(_thread_mutex_unlock);
WEAK_PROTOTYPE(_thread_mutex_destroy);

WEAK_ALIAS(_thread_mutex_lock);
WEAK_ALIAS(_thread_mutex_unlock);
WEAK_ALIAS(_thread_mutex_destroy);

/* ARGSUSED */
void
WEAK_NAME(_thread_mutex_lock)(void **mutex)
{
	return;
}

/* ARGSUSED */
void
WEAK_NAME(_thread_mutex_unlock)(void **mutex)
{
	return;
}

/* ARGSUSED */
void
WEAK_NAME(_thread_mutex_destroy)(void **mutex)
{
	return;
}
