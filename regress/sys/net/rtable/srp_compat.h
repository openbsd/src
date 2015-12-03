
#ifndef _SRP_COMPAT_H_
#define _SRP_COMPAT_H_

#include <sys/srp.h>
#include <sys/queue.h>

/*
 * SRPL glue.
 */
#define REFCNT_INITIALIZER()	{ .refs = 1 }
#define SRP_GC_INITIALIZER(_d, _c) { (_d), (_c), REFCNT_INITIALIZER() }
#define SRPL_RC_INITIALIZER(_r, _u, _c) { _r, SRP_GC_INITIALIZER(_u, _c) }

#define srp_get_locked(_s)	((_s)->ref)
#define srp_enter(_s)		srp_get_locked(_s)
#define srp_leave(_s, _v)	/* nothing */

#define srp_update_locked(_gc, _s, _v) do {				\
	void *ov;							\
									\
	ov = (_s)->ref;							\
	(_s)->ref = (_v);						\
	if (ov != NULL)							\
		(*(_gc)->srp_gc_dtor)((_gc)->srp_gc_cookie, ov);	\
} while (0)

#define SRPL_INIT(_sl)			SLIST_INIT(_sl)
#define SRPL_HEAD(name, entry)		SLIST_HEAD(name, entry)
#define SRPL_ENTRY(type)		SLIST_ENTRY(type)

#define SRPL_ENTER(_sl, _si)		SLIST_FIRST(_sl);(void)_si
#define SRPL_NEXT(_si, _e, _ENTRY)	SLIST_NEXT(_e, _ENTRY)
#define SRPL_LEAVE(_si, _c)		/* nothing */

#define SRPL_FOREACH_SAFE_LOCKED(_c, _sl, _ENTRY, _tc)			\
		SLIST_FOREACH_SAFE(_c, _sl, _ENTRY, _tc)
#define SRPL_REMOVE_LOCKED(_rc, _sl, _e, _type, _ENTRY)			\
		SLIST_REMOVE(_sl, _e, _type, _ENTRY)
#define SRPL_INSERT_HEAD_LOCKED(_rc, _sl, _e, _ENTRY)			\
		SLIST_INSERT_HEAD(_sl, _e, _ENTRY)

#endif /* _SRP_COMPAT_H_ */
