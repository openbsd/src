/*	$OpenBSD: refcnt.h,v 1.1 2015/09/11 19:13:22 dlg Exp $ */

/*
 * Copyright (c) 2015 David Gwynne <dlg@openbsd.org>
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

#ifndef _SYS_REFCNT_H_
#define _SYS_REFCNT_H_

#include <sys/atomic.h>

struct refcnt {
	unsigned int refs;
};

#ifdef _KERNEL

#define REFCNT_INITIALIZER()	{ .refs = 1 }

#define refcnt_init(_r)		do { (_r)->refs = 1; } while (0)
#define refcnt_take(_r)		atomic_inc_int(&(_r)->refs)
#define refcnt_rele(_r)		(atomic_dec_int_nv(&(_r)->refs) == 0)

void	refcnt_rele_wake(struct refcnt *);
void	refcnt_finalize(struct refcnt *, const char *);

#endif /* _KERNEL */

#endif /* _SYS_REFCNT_H_ */
