/*	$OpenBSD: rf_freelist.h,v 1.1 1999/01/11 14:29:23 niklas Exp $	*/
/*	$NetBSD: rf_freelist.h,v 1.1 1998/11/13 04:20:30 oster Exp $	*/
/*
 * rf_freelist.h
 */
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Jim Zelenka
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */
/*
 * :  
 * Log: rf_freelist.h,v 
 * Revision 1.13  1996/06/10 12:50:57  jimz
 * Add counters to freelists to track number of allocations, frees,
 * grows, max size, etc. Adjust a couple sets of PRIME params based
 * on the results.
 *
 * Revision 1.12  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.11  1996/06/05  18:06:02  jimz
 * Major code cleanup. The Great Renaming is now done.
 * Better modularity. Better typing. Fixed a bunch of
 * synchronization bugs. Made a lot of global stuff
 * per-desc or per-array. Removed dead code.
 *
 * Revision 1.10  1996/06/02  17:31:48  jimz
 * Moved a lot of global stuff into array structure, where it belongs.
 * Fixed up paritylogging, pss modules in this manner. Some general
 * code cleanup. Removed lots of dead code, some dead files.
 *
 * Revision 1.9  1996/05/31  22:26:54  jimz
 * fix a lot of mapping problems, memory allocation problems
 * found some weird lock issues, fixed 'em
 * more code cleanup
 *
 * Revision 1.8  1996/05/30  11:29:41  jimz
 * Numerous bug fixes. Stripe lock release code disagreed with the taking code
 * about when stripes should be locked (I made it consistent: no parity, no lock)
 * There was a lot of extra serialization of I/Os which I've removed- a lot of
 * it was to calculate values for the cache code, which is no longer with us.
 * More types, function, macro cleanup. Added code to properly quiesce the array
 * on shutdown. Made a lot of stuff array-specific which was (bogusly) general
 * before. Fixed memory allocation, freeing bugs.
 *
 * Revision 1.7  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.6  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.5  1996/05/20  16:16:12  jimz
 * switch to rf_{mutex,cond}_{init,destroy}
 *
 * Revision 1.4  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.3  1996/05/16  16:04:52  jimz
 * allow init func to fail for FREELIST ops
 *
 * Revision 1.2  1996/05/16  14:54:08  jimz
 * added _INIT and _CLEAN versions of ops for objects with
 * internal allocations
 *
 * Revision 1.1  1996/05/15  23:37:53  jimz
 * Initial revision
 *
 */
/*
 * rf_freelist.h -- code to manage counted freelists
 *
 * Keep an arena of fixed-size objects. When a new object is needed,
 * allocate it as necessary. When an object is freed, either put it
 * in the arena, or really free it, depending on the maximum arena
 * size.
 */

#ifndef _RF__RF_FREELIST_H_
#define _RF__RF_FREELIST_H_

#include "rf_types.h"
#include "rf_debugMem.h"
#include "rf_general.h"
#include "rf_threadstuff.h"

#define RF_FREELIST_STATS 0

#if RF_FREELIST_STATS > 0
typedef struct RF_FreeListStats_s {
  char  *file;
  int    line;
  int    allocations;
  int    frees;
  int    max_free;
  int    grows;
  int    outstanding;
  int    max_outstanding;
} RF_FreeListStats_t;

#define RF_FREELIST_STAT_INIT(_fl_) { \
	bzero((char *)&((_fl_)->stats), sizeof(RF_FreeListStats_t)); \
	(_fl_)->stats.file = __FILE__; \
	(_fl_)->stats.line = __LINE__; \
}

#define RF_FREELIST_STAT_ALLOC(_fl_) { \
	(_fl_)->stats.allocations++; \
	(_fl_)->stats.outstanding++; \
	if ((_fl_)->stats.outstanding > (_fl_)->stats.max_outstanding) \
		(_fl_)->stats.max_outstanding = (_fl_)->stats.outstanding; \
}

#define RF_FREELIST_STAT_FREE_UPDATE(_fl_) { \
	if ((_fl_)->free_cnt > (_fl_)->stats.max_free) \
		(_fl_)->stats.max_free = (_fl_)->free_cnt; \
}

#define RF_FREELIST_STAT_FREE(_fl_) { \
	(_fl_)->stats.frees++; \
	(_fl_)->stats.outstanding--; \
	RF_FREELIST_STAT_FREE_UPDATE(_fl_); \
}

#define RF_FREELIST_STAT_GROW(_fl_) { \
	(_fl_)->stats.grows++; \
	RF_FREELIST_STAT_FREE_UPDATE(_fl_); \
}

#define RF_FREELIST_STAT_REPORT(_fl_) { \
	printf("Freelist at %s %d (%s)\n", (_fl_)->stats.file, (_fl_)->stats.line, RF_STRING(_fl_)); \
	printf("  %d allocations, %d frees\n", (_fl_)->stats.allocations, (_fl_)->stats.frees); \
	printf("  %d grows\n", (_fl_)->stats.grows); \
	printf("  %d outstanding\n", (_fl_)->stats.outstanding); \
	printf("  %d free (max)\n", (_fl_)->stats.max_free); \
	printf("  %d outstanding (max)\n", (_fl_)->stats.max_outstanding); \
}

#else /* RF_FREELIST_STATS > 0 */

#define RF_FREELIST_STAT_INIT(_fl_)
#define RF_FREELIST_STAT_ALLOC(_fl_)
#define RF_FREELIST_STAT_FREE_UPDATE(_fl_)
#define RF_FREELIST_STAT_FREE(_fl_)
#define RF_FREELIST_STAT_GROW(_fl_)
#define RF_FREELIST_STAT_REPORT(_fl_)

#endif /* RF_FREELIST_STATS > 0 */

struct RF_FreeList_s {
	void  *objlist;      /* list of free obj */
	int    free_cnt;     /* how many free obj */
	int    max_free_cnt; /* max free arena size */
	int    obj_inc;      /* how many to allocate at a time */
	int    obj_size;     /* size of objects */
	RF_DECLARE_MUTEX(lock)
#if RF_FREELIST_STATS > 0
	RF_FreeListStats_t  stats;  /* statistics */
#endif /* RF_FREELIST_STATS > 0 */
};

/*
 * fl     = freelist
 * maxcnt = max number of items in arena
 * inc    = how many to allocate at a time
 * size   = size of object
 */
#define RF_FREELIST_CREATE(_fl_,_maxcnt_,_inc_,_size_) { \
	int rc; \
	RF_ASSERT((_inc_) > 0); \
	RF_Malloc(_fl_, sizeof(RF_FreeList_t), (RF_FreeList_t *)); \
	(_fl_)->objlist = NULL; \
	(_fl_)->free_cnt = 0; \
	(_fl_)->max_free_cnt = _maxcnt_; \
	(_fl_)->obj_inc = _inc_; \
	(_fl_)->obj_size = _size_; \
	rc = rf_mutex_init(&(_fl_)->lock); \
	if (rc) { \
		RF_Free(_fl_, sizeof(RF_FreeList_t)); \
		_fl_ = NULL; \
	} \
	RF_FREELIST_STAT_INIT(_fl_); \
}

/*
 * fl    = freelist
 * cnt   = number to prime with
 * nextp = name of "next" pointer in obj
 * cast  = object cast
 */
#define RF_FREELIST_PRIME(_fl_,_cnt_,_nextp_,_cast_) { \
	void *_p; \
	int _i; \
	RF_LOCK_MUTEX((_fl_)->lock); \
	for(_i=0;_i<(_cnt_);_i++) { \
		RF_Calloc(_p,1,(_fl_)->obj_size,(void *)); \
		if (_p) { \
			(_cast_(_p))->_nextp_ = (_fl_)->objlist; \
			(_fl_)->objlist = _p; \
			(_fl_)->free_cnt++; \
		} \
		else { \
			break; \
		} \
	} \
	RF_FREELIST_STAT_FREE_UPDATE(_fl_); \
	RF_UNLOCK_MUTEX((_fl_)->lock); \
}

#define RF_FREELIST_MUTEX_OF(_fl_) ((_fl_)->lock)

#define RF_FREELIST_DO_UNLOCK(_fl_) { \
	RF_UNLOCK_MUTEX((_fl_)->lock); \
}

#define RF_FREELIST_DO_LOCK(_fl_) { \
	RF_LOCK_MUTEX((_fl_)->lock); \
}

/*
 * fl    = freelist
 * cnt   = number to prime with
 * nextp = name of "next" pointer in obj
 * cast  = object cast
 * init  = func to call to init obj
 */
#define RF_FREELIST_PRIME_INIT(_fl_,_cnt_,_nextp_,_cast_,_init_) { \
	void *_p; \
	int _i; \
	RF_LOCK_MUTEX((_fl_)->lock); \
	for(_i=0;_i<(_cnt_);_i++) { \
		RF_Calloc(_p,1,(_fl_)->obj_size,(void *)); \
		if (_init_ (_cast_ _p)) { \
			RF_Free(_p,(_fl_)->obj_size); \
			_p = NULL; \
		} \
		if (_p) { \
			(_cast_(_p))->_nextp_ = (_fl_)->objlist; \
			(_fl_)->objlist = _p; \
			(_fl_)->free_cnt++; \
		} \
		else { \
			break; \
		} \
	} \
	RF_FREELIST_STAT_FREE_UPDATE(_fl_); \
	RF_UNLOCK_MUTEX((_fl_)->lock); \
}

/*
 * fl    = freelist
 * cnt   = number to prime with
 * nextp = name of "next" pointer in obj
 * cast  = object cast
 * init  = func to call to init obj
 * arg   = arg to init obj func
 */
#define RF_FREELIST_PRIME_INIT_ARG(_fl_,_cnt_,_nextp_,_cast_,_init_,_arg_) { \
	void *_p; \
	int _i; \
	RF_LOCK_MUTEX((_fl_)->lock); \
	for(_i=0;_i<(_cnt_);_i++) { \
		RF_Calloc(_p,1,(_fl_)->obj_size,(void *)); \
		if (_init_ (_cast_ _p,_arg_)) { \
			RF_Free(_p,(_fl_)->obj_size); \
			_p = NULL; \
		} \
		if (_p) { \
			(_cast_(_p))->_nextp_ = (_fl_)->objlist; \
			(_fl_)->objlist = _p; \
			(_fl_)->free_cnt++; \
		} \
		else { \
			break; \
		} \
	} \
	RF_FREELIST_STAT_FREE_UPDATE(_fl_); \
	RF_UNLOCK_MUTEX((_fl_)->lock); \
}

/*
 * fl    = freelist
 * obj   = object to allocate
 * nextp = name of "next" pointer in obj
 * cast  = cast of obj assignment
 * init  = init obj func
 */
#define RF_FREELIST_GET_INIT(_fl_,_obj_,_nextp_,_cast_,_init_) { \
	void *_p; \
	int _i; \
	RF_LOCK_MUTEX((_fl_)->lock); \
	RF_ASSERT(sizeof(*(_obj_))==((_fl_)->obj_size)); \
	if (_fl_->objlist) { \
		_obj_ = _cast_((_fl_)->objlist); \
		(_fl_)->objlist = (void *)((_obj_)->_nextp_); \
		(_fl_)->free_cnt--; \
	} \
	else { \
		/* \
		 * Allocate one at a time so we can free \
		 * one at a time without cleverness when arena \
		 * is full. \
		 */ \
		RF_Calloc(_obj_,1,(_fl_)->obj_size,_cast_); \
		if (_obj_) { \
			if (_init_ (_obj_)) { \
				RF_Free(_obj_,(_fl_)->obj_size); \
				_obj_ = NULL; \
			} \
			else { \
				for(_i=1;_i<(_fl_)->obj_inc;_i++) { \
					RF_Calloc(_p,1,(_fl_)->obj_size,(void *)); \
					if (_p) { \
						if (_init_ (_p)) { \
							RF_Free(_p,(_fl_)->obj_size); \
							_p = NULL; \
							break; \
						} \
						(_cast_(_p))->_nextp_ = (_fl_)->objlist; \
						(_fl_)->objlist = _p; \
					} \
					else { \
						break; \
					} \
				} \
			} \
		} \
		RF_FREELIST_STAT_GROW(_fl_); \
	} \
	RF_FREELIST_STAT_ALLOC(_fl_); \
	RF_UNLOCK_MUTEX((_fl_)->lock); \
}

/*
 * fl    = freelist
 * obj   = object to allocate
 * nextp = name of "next" pointer in obj
 * cast  = cast of obj assignment
 * init  = init obj func
 * arg   = arg to init obj func
 */
#define RF_FREELIST_GET_INIT_ARG(_fl_,_obj_,_nextp_,_cast_,_init_,_arg_) { \
	void *_p; \
	int _i; \
	RF_LOCK_MUTEX((_fl_)->lock); \
	RF_ASSERT(sizeof(*(_obj_))==((_fl_)->obj_size)); \
	if (_fl_->objlist) { \
		_obj_ = _cast_((_fl_)->objlist); \
		(_fl_)->objlist = (void *)((_obj_)->_nextp_); \
		(_fl_)->free_cnt--; \
	} \
	else { \
		/* \
		 * Allocate one at a time so we can free \
		 * one at a time without cleverness when arena \
		 * is full. \
		 */ \
		RF_Calloc(_obj_,1,(_fl_)->obj_size,_cast_); \
		if (_obj_) { \
			if (_init_ (_obj_,_arg_)) { \
				RF_Free(_obj_,(_fl_)->obj_size); \
				_obj_ = NULL; \
			} \
			else { \
				for(_i=1;_i<(_fl_)->obj_inc;_i++) { \
					RF_Calloc(_p,1,(_fl_)->obj_size,(void *)); \
					if (_p) { \
						if (_init_ (_p,_arg_)) { \
							RF_Free(_p,(_fl_)->obj_size); \
							_p = NULL; \
							break; \
						} \
						(_cast_(_p))->_nextp_ = (_fl_)->objlist; \
						(_fl_)->objlist = _p; \
					} \
					else { \
						break; \
					} \
				} \
			} \
		} \
		RF_FREELIST_STAT_GROW(_fl_); \
	} \
	RF_FREELIST_STAT_ALLOC(_fl_); \
	RF_UNLOCK_MUTEX((_fl_)->lock); \
}

/*
 * fl    = freelist
 * obj   = object to allocate
 * nextp = name of "next" pointer in obj
 * cast  = cast of obj assignment
 * init  = init obj func
 */
#define RF_FREELIST_GET_INIT_NOUNLOCK(_fl_,_obj_,_nextp_,_cast_,_init_) { \
	void *_p; \
	int _i; \
	RF_LOCK_MUTEX((_fl_)->lock); \
	RF_ASSERT(sizeof(*(_obj_))==((_fl_)->obj_size)); \
	if (_fl_->objlist) { \
		_obj_ = _cast_((_fl_)->objlist); \
		(_fl_)->objlist = (void *)((_obj_)->_nextp_); \
		(_fl_)->free_cnt--; \
	} \
	else { \
		/* \
		 * Allocate one at a time so we can free \
		 * one at a time without cleverness when arena \
		 * is full. \
		 */ \
		RF_Calloc(_obj_,1,(_fl_)->obj_size,_cast_); \
		if (_obj_) { \
			if (_init_ (_obj_)) { \
				RF_Free(_obj_,(_fl_)->obj_size); \
				_obj_ = NULL; \
			} \
			else { \
				for(_i=1;_i<(_fl_)->obj_inc;_i++) { \
					RF_Calloc(_p,1,(_fl_)->obj_size,(void *)); \
					if (_p) { \
						if (_init_ (_p)) { \
							RF_Free(_p,(_fl_)->obj_size); \
							_p = NULL; \
							break; \
						} \
						(_cast_(_p))->_nextp_ = (_fl_)->objlist; \
						(_fl_)->objlist = _p; \
					} \
					else { \
						break; \
					} \
				} \
			} \
		} \
		RF_FREELIST_STAT_GROW(_fl_); \
	} \
	RF_FREELIST_STAT_ALLOC(_fl_); \
}

/*
 * fl    = freelist
 * obj   = object to allocate
 * nextp = name of "next" pointer in obj
 * cast  = cast of obj assignment
 */
#define RF_FREELIST_GET(_fl_,_obj_,_nextp_,_cast_) { \
	void *_p; \
	int _i; \
	RF_LOCK_MUTEX((_fl_)->lock); \
	RF_ASSERT(sizeof(*(_obj_))==((_fl_)->obj_size)); \
	if (_fl_->objlist) { \
		_obj_ = _cast_((_fl_)->objlist); \
		(_fl_)->objlist = (void *)((_obj_)->_nextp_); \
		(_fl_)->free_cnt--; \
	} \
	else { \
		/* \
		 * Allocate one at a time so we can free \
		 * one at a time without cleverness when arena \
		 * is full. \
		 */ \
		RF_Calloc(_obj_,1,(_fl_)->obj_size,_cast_); \
		if (_obj_) { \
			for(_i=1;_i<(_fl_)->obj_inc;_i++) { \
				RF_Calloc(_p,1,(_fl_)->obj_size,(void *)); \
				if (_p) { \
					(_cast_(_p))->_nextp_ = (_fl_)->objlist; \
					(_fl_)->objlist = _p; \
				} \
				else { \
					break; \
				} \
			} \
		} \
		RF_FREELIST_STAT_GROW(_fl_); \
	} \
	RF_FREELIST_STAT_ALLOC(_fl_); \
	RF_UNLOCK_MUTEX((_fl_)->lock); \
}

/*
 * fl    = freelist
 * obj   = object to allocate
 * nextp = name of "next" pointer in obj
 * cast  = cast of obj assignment
 * num   = num objs to return
 */
#define RF_FREELIST_GET_N(_fl_,_obj_,_nextp_,_cast_,_num_) { \
	void *_p, *_l, *_f; \
	int _i, _n; \
	_l = _f = NULL; \
	_n = 0; \
	RF_LOCK_MUTEX((_fl_)->lock); \
	RF_ASSERT(sizeof(*(_obj_))==((_fl_)->obj_size)); \
	for(_n=0;_n<_num_;_n++) { \
		if (_fl_->objlist) { \
			_obj_ = _cast_((_fl_)->objlist); \
			(_fl_)->objlist = (void *)((_obj_)->_nextp_); \
			(_fl_)->free_cnt--; \
		} \
		else { \
			/* \
			 * Allocate one at a time so we can free \
			 * one at a time without cleverness when arena \
			 * is full. \
			 */ \
			RF_Calloc(_obj_,1,(_fl_)->obj_size,_cast_); \
			if (_obj_) { \
				for(_i=1;_i<(_fl_)->obj_inc;_i++) { \
					RF_Calloc(_p,1,(_fl_)->obj_size,(void *)); \
					if (_p) { \
						(_cast_(_p))->_nextp_ = (_fl_)->objlist; \
						(_fl_)->objlist = _p; \
					} \
					else { \
						break; \
					} \
				} \
			} \
			RF_FREELIST_STAT_GROW(_fl_); \
		} \
		if (_f == NULL) \
			_f = _obj_; \
		if (_obj_) { \
			(_cast_(_obj_))->_nextp_ = _l; \
			_l = _obj_; \
			RF_FREELIST_STAT_ALLOC(_fl_); \
		} \
		else { \
			(_cast_(_f))->_nextp_ = (_fl_)->objlist; \
			(_fl_)->objlist = _l; \
			_n = _num_; \
		} \
	} \
	RF_UNLOCK_MUTEX((_fl_)->lock); \
}

/*
 * fl = freelist
 * obj   = object to free
 * nextp = name of "next" pointer in obj
 */
#define RF_FREELIST_FREE(_fl_,_obj_,_nextp_) { \
	RF_LOCK_MUTEX((_fl_)->lock); \
	if ((_fl_)->free_cnt == (_fl_)->max_free_cnt) { \
		RF_Free(_obj_,(_fl_)->obj_size); \
	} \
	else { \
		RF_ASSERT((_fl_)->free_cnt < (_fl_)->max_free_cnt); \
		(_obj_)->_nextp_ = (_fl_)->objlist; \
		(_fl_)->objlist = (void *)(_obj_); \
		(_fl_)->free_cnt++; \
	} \
	RF_FREELIST_STAT_FREE(_fl_); \
	RF_UNLOCK_MUTEX((_fl_)->lock); \
}

/*
 * fl    = freelist
 * obj   = object to free
 * nextp = name of "next" pointer in obj
 * num   = num to free (debugging)
 */
#define RF_FREELIST_FREE_N(_fl_,_obj_,_nextp_,_cast_,_num_) { \
	void *_no; \
	int _n; \
	_n = 0; \
	RF_LOCK_MUTEX((_fl_)->lock); \
	while(_obj_) { \
		_no = (_cast_(_obj_))->_nextp_; \
		if ((_fl_)->free_cnt == (_fl_)->max_free_cnt) { \
			RF_Free(_obj_,(_fl_)->obj_size); \
		} \
		else { \
			RF_ASSERT((_fl_)->free_cnt < (_fl_)->max_free_cnt); \
			(_obj_)->_nextp_ = (_fl_)->objlist; \
			(_fl_)->objlist = (void *)(_obj_); \
			(_fl_)->free_cnt++; \
		} \
		_n++; \
		_obj_ = _no; \
		RF_FREELIST_STAT_FREE(_fl_); \
	} \
	RF_ASSERT(_n==(_num_)); \
	RF_UNLOCK_MUTEX((_fl_)->lock); \
}

/*
 * fl    = freelist
 * obj   = object to free
 * nextp = name of "next" pointer in obj
 * clean = undo for init
 */
#define RF_FREELIST_FREE_CLEAN(_fl_,_obj_,_nextp_,_clean_) { \
	RF_LOCK_MUTEX((_fl_)->lock); \
	if ((_fl_)->free_cnt == (_fl_)->max_free_cnt) { \
		_clean_ (_obj_); \
		RF_Free(_obj_,(_fl_)->obj_size); \
	} \
	else { \
		RF_ASSERT((_fl_)->free_cnt < (_fl_)->max_free_cnt); \
		(_obj_)->_nextp_ = (_fl_)->objlist; \
		(_fl_)->objlist = (void *)(_obj_); \
		(_fl_)->free_cnt++; \
	} \
	RF_FREELIST_STAT_FREE(_fl_); \
	RF_UNLOCK_MUTEX((_fl_)->lock); \
}

/*
 * fl    = freelist
 * obj   = object to free
 * nextp = name of "next" pointer in obj
 * clean = undo for init
 * arg   = arg for undo func
 */
#define RF_FREELIST_FREE_CLEAN_ARG(_fl_,_obj_,_nextp_,_clean_,_arg_) { \
	RF_LOCK_MUTEX((_fl_)->lock); \
	if ((_fl_)->free_cnt == (_fl_)->max_free_cnt) { \
		_clean_ (_obj_,_arg_); \
		RF_Free(_obj_,(_fl_)->obj_size); \
	} \
	else { \
		RF_ASSERT((_fl_)->free_cnt < (_fl_)->max_free_cnt); \
		(_obj_)->_nextp_ = (_fl_)->objlist; \
		(_fl_)->objlist = (void *)(_obj_); \
		(_fl_)->free_cnt++; \
	} \
	RF_FREELIST_STAT_FREE(_fl_); \
	RF_UNLOCK_MUTEX((_fl_)->lock); \
}

/*
 * fl    = freelist
 * obj   = object to free
 * nextp = name of "next" pointer in obj
 * clean = undo for init
 */
#define RF_FREELIST_FREE_CLEAN_NOUNLOCK(_fl_,_obj_,_nextp_,_clean_) { \
	RF_LOCK_MUTEX((_fl_)->lock); \
	if ((_fl_)->free_cnt == (_fl_)->max_free_cnt) { \
		_clean_ (_obj_); \
		RF_Free(_obj_,(_fl_)->obj_size); \
	} \
	else { \
		RF_ASSERT((_fl_)->free_cnt < (_fl_)->max_free_cnt); \
		(_obj_)->_nextp_ = (_fl_)->objlist; \
		(_fl_)->objlist = (void *)(_obj_); \
		(_fl_)->free_cnt++; \
	} \
	RF_FREELIST_STAT_FREE(_fl_); \
}

/*
 * fl     = freelist
 * nextp = name of "next" pointer in obj
 * cast  = cast to object type
 */
#define RF_FREELIST_DESTROY(_fl_,_nextp_,_cast_) { \
	void *_cur, *_next; \
	RF_FREELIST_STAT_REPORT(_fl_); \
	rf_mutex_destroy(&((_fl_)->lock)); \
	for(_cur=(_fl_)->objlist;_cur;_cur=_next) { \
		_next = (_cast_ _cur)->_nextp_; \
		RF_Free(_cur,(_fl_)->obj_size); \
	} \
	RF_Free(_fl_,sizeof(RF_FreeList_t)); \
}

/*
 * fl    = freelist
 * nextp = name of "next" pointer in obj
 * cast  = cast to object type
 * clean = func to undo obj init
 */
#define RF_FREELIST_DESTROY_CLEAN(_fl_,_nextp_,_cast_,_clean_) { \
	void *_cur, *_next; \
	RF_FREELIST_STAT_REPORT(_fl_); \
	rf_mutex_destroy(&((_fl_)->lock)); \
	for(_cur=(_fl_)->objlist;_cur;_cur=_next) { \
		_next = (_cast_ _cur)->_nextp_; \
		_clean_ (_cur); \
		RF_Free(_cur,(_fl_)->obj_size); \
	} \
	RF_Free(_fl_,sizeof(RF_FreeList_t)); \
}

/*
 * fl    = freelist
 * nextp = name of "next" pointer in obj
 * cast  = cast to object type
 * clean = func to undo obj init
 * arg   = arg for undo func
 */
#define RF_FREELIST_DESTROY_CLEAN_ARG(_fl_,_nextp_,_cast_,_clean_,_arg_) { \
	void *_cur, *_next; \
	RF_FREELIST_STAT_REPORT(_fl_); \
	rf_mutex_destroy(&((_fl_)->lock)); \
	for(_cur=(_fl_)->objlist;_cur;_cur=_next) { \
		_next = (_cast_ _cur)->_nextp_; \
		_clean_ (_cur,_arg_); \
		RF_Free(_cur,(_fl_)->obj_size); \
	} \
	RF_Free(_fl_,sizeof(RF_FreeList_t)); \
}

#endif /* !_RF__RF_FREELIST_H_ */
