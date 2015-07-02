/*	$OpenBSD: srp.h,v 1.1 2015/07/02 01:34:00 dlg Exp $ */

/*
 * Copyright (c) 2014 Jonathan Matthew <jmatthew@openbsd.org>
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

#ifndef _SYS_SRP_H_
#define _SYS_SRP_H_

struct srp {
	void			*ref;
};

struct srp_hazard {
	struct srp		*sh_p;
	void			*sh_v;
};

#define SRP_HAZARD_NUM 16
	
struct srp_gc {
	void			(*srp_gc_dtor)(void *, void *);
	void			*srp_gc_cookie;
	u_int			srp_gc_refcount;
};

#ifdef _KERNEL

#define SRP_INITIALIZER() { NULL }
#define SRP_GC_INITIALIZER(_d, _c) { (_d), (_c), 1 }

void		 srp_startup(void);
void		 srp_gc_init(struct srp_gc *, void (*)(void *, void *), void *);
void		 srp_update_locked(struct srp_gc *, struct srp *, void *);
void		*srp_get_locked(struct srp *);
void		 srp_finalize(struct srp_gc *);

void		 srp_init(struct srp *);

#ifdef MULTIPROCESSOR
void		 srp_update(struct srp_gc *, struct srp *, void *);
void		*srp_enter(struct srp *);
void		 srp_leave(struct srp *, void *);
#else /* MULTIPROCESSOR */
#define srp_update(_gc, _srp, _v)	srp_update_locked((_gc), (_srp), (_v))
#define srp_enter(_srp)			((_srp)->ref)
#define srp_leave(_srp, _v)		do { } while (0)
#endif /* MULTIPROCESSOR */

#endif /* _KERNEL */

#endif /* _SYS_SRP_H_ */
