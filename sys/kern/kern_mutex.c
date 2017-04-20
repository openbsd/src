/*	$OpenBSD: kern_mutex.c,v 1.1 2017/04/20 13:57:30 visa Exp $	*/

/*
 * Copyright (c) 2017 Visa Hankala
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mutex.h>
#include <sys/witness.h>

void
_mtx_init_flags(struct mutex *m, int ipl, const char *name, int flags,
    struct lock_type *type)
{
	struct lock_object *lo = MUTEX_LOCK_OBJECT(m);

	lo->lo_flags = MTX_LO_FLAGS(flags);
	if (name != NULL)
		lo->lo_name = name;
	else
		lo->lo_name = type->lt_name;
	WITNESS_INIT(lo, type);

	_mtx_init(m, ipl);
}

void
_mtx_enter(struct mutex *m, const char *file, int line)
{
	struct lock_object *lo = MUTEX_LOCK_OBJECT(m);

	WITNESS_CHECKORDER(lo, LOP_EXCLUSIVE | LOP_NEWORDER, file, line, NULL);
	__mtx_enter(m);
	WITNESS_LOCK(lo, LOP_EXCLUSIVE, file, line);
}

int
_mtx_enter_try(struct mutex *m, const char *file, int line)
{
	struct lock_object *lo = MUTEX_LOCK_OBJECT(m);

	if (__mtx_enter_try(m)) {
		WITNESS_LOCK(lo, LOP_EXCLUSIVE, file, line);
		return 1;
	}
	return 0;
}

void
_mtx_leave(struct mutex *m, const char *file, int line)
{
	struct lock_object *lo = MUTEX_LOCK_OBJECT(m);

	WITNESS_UNLOCK(lo, LOP_EXCLUSIVE, file, line);
	__mtx_leave(m);
}
