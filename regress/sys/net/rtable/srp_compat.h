
#ifndef _SRP_COMPAT_H_
#define _SRP_COMPAT_H_

#include <sys/srp.h>
#include <sys/queue.h>

/*
 * SRPL glue.
 */

#define srp_get_locked(_s)	((_s)->ref)
#define srp_enter(_sr, _s)	srp_get_locked(_s)
#define srp_leave(_sr)		((void)_sr)

#define srp_update_locked(_gc, _s, _v) do {				\
	void *ov;							\
									\
	ov = (_s)->ref;							\
	(_s)->ref = _v;							\
									\
	if (ov != NULL)							\
		((_gc)->srp_gc_dtor)((_gc)->srp_gc_cookie, ov);		\
} while (0)

#define SRPL_INIT(_sl)			SLIST_INIT(_sl)
#define SRPL_HEAD(name, entry)		SLIST_HEAD(name, entry)
#define SRPL_ENTRY(type)		SLIST_ENTRY(type)

#define SRPL_ENTER(_sr, _sl)		SLIST_FIRST(_sl);
#define SRPL_NEXT(_sr, _e, _ENTRY)	SLIST_NEXT(_e, _ENTRY)
#define SRPL_LEAVE(_sr)			((void)_sr)

#define SRPL_EMPTY_LOCKED(_sl)	SLIST_EMPTY(_sl)
#define SRPL_FOREACH_SAFE_LOCKED(_c, _sl, _ENTRY, _tc)			\
		SLIST_FOREACH_SAFE(_c, _sl, _ENTRY, _tc)

#define SRPL_INSERT_HEAD_LOCKED(_rc, _sl, _e, _ENTRY)			\
	do {								\
		(_rc)->srpl_ref((_rc)->srpl_cookie, _e);		\
		SLIST_INSERT_HEAD(_sl, _e, _ENTRY);			\
	} while (0)

#define SRPL_REMOVE_LOCKED(_rc, _sl, _e, _type, _ENTRY)			\
	do {								\
		SLIST_REMOVE(_sl, _e, _type, _ENTRY);			\
		((_rc)->srpl_gc.srp_gc_dtor)((_rc)->srpl_gc.srp_gc_cookie, _e);\
	} while (0)

#endif /* _SRP_COMPAT_H_ */
