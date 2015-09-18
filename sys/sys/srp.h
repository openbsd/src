/*	$OpenBSD: srp.h,v 1.5 2015/09/18 08:30:23 dlg Exp $ */

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

#include <sys/refcnt.h>

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
	struct refcnt		srp_gc_refcnt;
};

#ifdef _KERNEL

#define SRP_INITIALIZER() { NULL }
#define SRP_GC_INITIALIZER(_d, _c) { (_d), (_c), REFCNT_INITIALIZER() }

void		 srp_startup(void);
void		 srp_gc_init(struct srp_gc *, void (*)(void *, void *), void *);
void		 srp_update_locked(struct srp_gc *, struct srp *, void *);
void		*srp_get_locked(struct srp *);
void		 srp_finalize(struct srp_gc *);

void		 srp_init(struct srp *);

#ifdef MULTIPROCESSOR
void		 srp_update(struct srp_gc *, struct srp *, void *);
void		*srp_enter(struct srp *);
void		*srp_follow(struct srp *, void *, struct srp *);
void		 srp_leave(struct srp *, void *);
#else /* MULTIPROCESSOR */
#define srp_update(_gc, _srp, _v)	srp_update_locked((_gc), (_srp), (_v))
#define srp_enter(_srp)			((_srp)->ref)
#define srp_follow(_srp, _v, _next)	((_next)->ref)
#define srp_leave(_srp, _v)		do { } while (0)
#endif /* MULTIPROCESSOR */

#endif /* _KERNEL */

/*
 * singly linked list built by following srps
 */

struct srpl_rc {
	void			(*srpl_ref)(void *, void *);
	struct srp_gc		srpl_gc;
};
#define srpl_cookie		srpl_gc.srp_gc_cookie

struct srpl {
	struct srp		sl_head;
};

struct srpl_entry {
	struct srp		se_next;
};

struct srpl_iter {
	struct srp *		si_ref;
};

#ifdef _KERNEL

void		srpl_rc_init(struct srpl_rc *, void (*)(void *, void *),
		    void (*)(void *, void *), void *);

#define SRPL_RC_INITIALIZER(_r, _u, _c) { _r, SRP_GC_INITIALIZER(_u, _c) }

#define SRPL_INIT(_sl)			srp_init(&(_sl)->sl_head)

static inline void *
_srpl_enter(struct srpl *sl, struct srpl_iter *si)
{
	si->si_ref = &sl->sl_head;
	return (srp_enter(si->si_ref));
}

static inline void *
_srpl_next(struct srpl_iter *si, void *elm, struct srp *nref)
{
	void *n;

	n = srp_follow(si->si_ref, elm, nref);
	si->si_ref = nref;

	return (n);
}

#define SRPL_ENTER(_sl, _si)		_srpl_enter(_sl, _si)

#define SRPL_NEXT(_si, _e, _ENTRY)					\
	 _srpl_next(_si, _e, &(_e)->_ENTRY.se_next)

#define SRPL_FOREACH(_c, _sl, _si, _ENTRY)				\
	for ((_c) = SRPL_ENTER(_sl, _si);				\
	    (_c) != NULL; 						\
	    (_c) = SRPL_NEXT(_si, _c, _ENTRY))

#define SRPL_LEAVE(_si, _c)		srp_leave((_si)->si_ref, (_c))

#define SRPL_EMPTY_LOCKED(_sl)		(SRPL_FIRST_LOCKED(_sl) == NULL)
#define SRPL_FIRST_LOCKED(_sl)		srp_get_locked(&(_sl)->sl_head)

#define SRPL_NEXT_LOCKED(_e, _ENTRY)					\
    srp_get_locked(&(_e)->_ENTRY.se_next)

#define SRPL_FOREACH_LOCKED(_c, _sl, _ENTRY)				\
	for ((_c) = SRPL_FIRST_LOCKED(_sl);				\
	    (_c) != NULL;						\
	    (_c) = SRPL_NEXT_LOCKED((_c), _ENTRY))

#define SRPL_INSERT_HEAD_LOCKED(_rc, _sl, _e, _ENTRY) do {		\
	void *head;							\
									\
	srp_init(&(_e)->_ENTRY.se_next);				\
									\
	head = SRPL_FIRST_LOCKED(_sl);					\
	if (head != NULL) {						\
		(_rc)->srpl_ref(&(_rc)->srpl_cookie, head);		\
		srp_update_locked(&(_rc)->srpl_gc,			\
		    &(_e)->_ENTRY.se_next, head);	 		\
	}								\
									\
	(_rc)->srpl_ref(&(_rc)->srpl_cookie, _e);			\
	srp_update_locked(&(_rc)->srpl_gc, &(_sl)->sl_head, (_e));	\
} while (0)

#define SRPL_INSERT_AFTER_LOCKED(_rc, _se, _e, _ENTRY) do {		\
	void *next;							\
									\
	srp_init(&(_e)->_ENTRY.se_next);				\
									\
	next = SRPL_NEXT_LOCKED(_se, _ENTRY);				\
	if (next != NULL) {						\
		(_rc)->srpl_ref(&(_rc)->srpl_cookie, next);		\
		srp_update_locked(&(_rc)->srpl_gc,			\
		    &(_e)->_ENTRY.se_next, next);	 		\
	}								\
									\
	(_rc)->srpl_ref(&(_rc)->srpl_cookie, _e);			\
	srp_update_locked(&(_rc)->srpl_gc,				\
	    &(_se)->_ENTRY.se_next, (_e));				\
} while (0)

#define SRPL_REMOVE_LOCKED(_rc, _sl, _e, _type, _ENTRY) do {		\
	struct srp *ref;						\
	struct _type *c, *n;						\
									\
	ref = &(_sl)->sl_head;						\
	while ((c = srp_get_locked(ref)) != (_e))			\
		ref = &c->_ENTRY.se_next;				\
									\
	n = SRPL_NEXT_LOCKED(c, _ENTRY);				\
	if (n != NULL)							\
		(_rc)->srpl_ref(&(_rc)->srpl_cookie, n);		\
	srp_update_locked(&(_rc)->srpl_gc, ref, n);			\
	srp_update_locked(&(_rc)->srpl_gc, &c->_ENTRY.se_next, NULL);	\
} while (0)

#endif /* _KERNEL */

#endif /* _SYS_SRP_H_ */
