/*	$OpenBSD: rf_freelist.h,v 1.3 2002/12/16 07:01:04 tdeval Exp $	*/
/*	$NetBSD: rf_freelist.h,v 1.3 1999/02/05 00:06:11 oster Exp $	*/

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
 * rf_freelist.h -- Code to manage counted freelists.
 *
 * Keep an arena of fixed-size objects. When a new object is needed,
 * allocate it as necessary. When an object is freed, either put it
 * in the arena, or really free it, depending on the maximum arena
 * size.
 */

#ifndef	_RF__RF_FREELIST_H_
#define	_RF__RF_FREELIST_H_

#include "rf_types.h"
#include "rf_debugMem.h"
#include "rf_general.h"
#include "rf_threadstuff.h"

#define	RF_FREELIST_STATS	0

#if	RF_FREELIST_STATS > 0
typedef struct RF_FreeListStats_s {
	char	*file;
	int	 line;
	int	 allocations;
	int	 frees;
	int	 max_free;
	int	 grows;
	int	 outstanding;
	int	 max_outstanding;
} RF_FreeListStats_t;

#define	RF_FREELIST_STAT_INIT(_fl_)					\
do {									\
	bzero((char *)&((_fl_)->stats), sizeof(RF_FreeListStats_t));	\
	(_fl_)->stats.file = __FILE__;					\
	(_fl_)->stats.line = __LINE__;					\
} while (0)

#define	RF_FREELIST_STAT_ALLOC(_fl_)					\
do {									\
	(_fl_)->stats.allocations++;					\
	(_fl_)->stats.outstanding++;					\
	if ((_fl_)->stats.outstanding > (_fl_)->stats.max_outstanding)	\
		(_fl_)->stats.max_outstanding =				\
		    (_fl_)->stats.outstanding;				\
} while (0)

#define	RF_FREELIST_STAT_FREE_UPDATE(_fl_)				\
do {									\
	if ((_fl_)->free_cnt > (_fl_)->stats.max_free)			\
		(_fl_)->stats.max_free = (_fl_)->free_cnt;		\
} while (0)

#define	RF_FREELIST_STAT_FREE(_fl_)					\
do {									\
	(_fl_)->stats.frees++;						\
	(_fl_)->stats.outstanding--;					\
	RF_FREELIST_STAT_FREE_UPDATE(_fl_);				\
} while (0)

#define	RF_FREELIST_STAT_GROW(_fl_)					\
do {									\
	(_fl_)->stats.grows++;						\
	RF_FREELIST_STAT_FREE_UPDATE(_fl_);				\
} while (0)

#define	RF_FREELIST_STAT_REPORT(_fl_)					\
do {									\
	printf("Freelist at %s %d (%s)\n", (_fl_)->stats.file,		\
	    (_fl_)->stats.line, RF_STRING(_fl_));			\
	printf("  %d allocations, %d frees\n",				\
	    (_fl_)->stats.allocations, (_fl_)->stats.frees);		\
	printf("  %d grows\n", (_fl_)->stats.grows);			\
	printf("  %d outstanding\n", (_fl_)->stats.outstanding);	\
	printf("  %d free (max)\n", (_fl_)->stats.max_free);		\
	printf("  %d outstanding (max)\n",				\
	    (_fl_)->stats.max_outstanding);				\
} while (0)

#else	/* RF_FREELIST_STATS > 0 */

#define	RF_FREELIST_STAT_INIT(_fl_)
#define	RF_FREELIST_STAT_ALLOC(_fl_)
#define	RF_FREELIST_STAT_FREE_UPDATE(_fl_)
#define	RF_FREELIST_STAT_FREE(_fl_)
#define	RF_FREELIST_STAT_GROW(_fl_)
#define	RF_FREELIST_STAT_REPORT(_fl_)

#endif	/* RF_FREELIST_STATS > 0 */

struct RF_FreeList_s {
	void	*objlist;	/* List of free obj. */
	int	 free_cnt;	/* How many free obj. */
	int	 max_free_cnt;	/* Max free arena size. */
	int	 obj_inc;	/* How many to allocate at a time. */
	int	 obj_size;	/* Size of objects. */
	RF_DECLARE_MUTEX(lock);
#if	RF_FREELIST_STATS > 0
	RF_FreeListStats_t stats;	/* Statistics. */
#endif	/* RF_FREELIST_STATS > 0 */
};

/*
 * fl	  = FreeList.
 * maxcnt = Max number of items in arena.
 * inc	  = How many to allocate at a time.
 * size	  = Size of object.
 */
#define	RF_FREELIST_CREATE(_fl_,_maxcnt_,_inc_,_size_)			\
do {									\
	int rc;								\
	RF_ASSERT((_inc_) > 0);						\
	RF_Malloc(_fl_, sizeof(RF_FreeList_t), (RF_FreeList_t *));	\
	(_fl_)->objlist = NULL;						\
	(_fl_)->free_cnt = 0;						\
	(_fl_)->max_free_cnt = _maxcnt_;				\
	(_fl_)->obj_inc = _inc_;					\
	(_fl_)->obj_size = _size_;					\
	rc = rf_mutex_init(&(_fl_)->lock);				\
	if (rc) {							\
		RF_Free(_fl_, sizeof(RF_FreeList_t));			\
		_fl_ = NULL;						\
	}								\
	RF_FREELIST_STAT_INIT(_fl_);					\
} while (0)

/*
 * fl	 = FreeList.
 * cnt	 = Number to prime with.
 * nextp = Name of "next" pointer in obj.
 * cast	 = Object cast.
 */
#define	RF_FREELIST_PRIME(_fl_,_cnt_,_nextp_,_cast_)			\
do {									\
	void *_p;							\
	int _i;								\
	RF_LOCK_MUTEX((_fl_)->lock);					\
	for (_i = 0; _i < (_cnt_); _i++) {				\
		RF_Calloc(_p, 1, (_fl_)->obj_size, (void *));		\
		if (_p) {						\
			(_cast_(_p))->_nextp_ = (_fl_)->objlist;	\
			(_fl_)->objlist = _p;				\
			(_fl_)->free_cnt++;				\
		}							\
		else {							\
			break;						\
		}							\
	}								\
	RF_FREELIST_STAT_FREE_UPDATE(_fl_);				\
	RF_UNLOCK_MUTEX((_fl_)->lock);					\
} while (0)

#define	RF_FREELIST_MUTEX_OF(_fl_)	((_fl_)->lock)

#define	RF_FREELIST_DO_UNLOCK(_fl_)	RF_UNLOCK_MUTEX((_fl_)->lock)

#define	RF_FREELIST_DO_LOCK(_fl_)	RF_LOCK_MUTEX((_fl_)->lock)

/*
 * fl	 = FreeList.
 * cnt	 = Number to prime with.
 * nextp = Name of "next" pointer in obj.
 * cast	 = Object cast.
 * init	 = Func to call to init obj.
 */
#define	RF_FREELIST_PRIME_INIT(_fl_,_cnt_,_nextp_,_cast_,_init_)	\
do {									\
	void *_p;							\
	int _i;								\
	RF_LOCK_MUTEX((_fl_)->lock);					\
	for (_i = 0; _i < (_cnt_); _i++) {				\
		RF_Calloc(_p, 1, (_fl_)->obj_size, (void *));		\
		if (_init_(_cast_ _p)) {				\
			RF_Free(_p, (_fl_)->obj_size);			\
			_p = NULL;					\
		}							\
		if (_p) {						\
			(_cast_(_p))->_nextp_ = (_fl_)->objlist;	\
			(_fl_)->objlist = _p;				\
			(_fl_)->free_cnt++;				\
		}							\
		else {							\
			break;						\
		}							\
	}								\
	RF_FREELIST_STAT_FREE_UPDATE(_fl_);				\
	RF_UNLOCK_MUTEX((_fl_)->lock);					\
} while (0)

/*
 * fl	 = FreeList.
 * cnt	 = Number to prime with.
 * nextp = Name of "next" pointer in obj.
 * cast	 = Object cast.
 * init	 = Func to call to init obj.
 * arg	 = Arg to init obj func.
 */
#define	RF_FREELIST_PRIME_INIT_ARG(_fl_,_cnt_,_nextp_,_cast_,_init_,_arg_) \
do {									\
	void *_p;							\
	int _i;								\
	RF_LOCK_MUTEX((_fl_)->lock);					\
	for (_i = 0; _i < (_cnt_); _i++) {				\
		RF_Calloc(_p, 1, (_fl_)->obj_size, (void *));		\
		if (_init_(_cast_ _p, _arg_)) {				\
			RF_Free(_p, (_fl_)->obj_size);			\
			_p = NULL;					\
		}							\
		if (_p) {						\
			(_cast_(_p))->_nextp_ = (_fl_)->objlist;	\
			(_fl_)->objlist = _p;				\
			(_fl_)->free_cnt++;				\
		}							\
		else {							\
			break;						\
		}							\
	}								\
	RF_FREELIST_STAT_FREE_UPDATE(_fl_);				\
	RF_UNLOCK_MUTEX((_fl_)->lock);					\
} while (0)

/*
 * fl	 = FreeList.
 * obj	 = Object to allocate.
 * nextp = Name of "next" pointer in obj.
 * cast	 = Cast of obj assignment.
 * init	 = Init obj func.
 */
#define	RF_FREELIST_GET_INIT(_fl_,_obj_,_nextp_,_cast_,_init_)		\
do {									\
	void *_p;							\
	int _i;								\
	RF_LOCK_MUTEX((_fl_)->lock);					\
	RF_ASSERT(sizeof(*(_obj_)) == ((_fl_)->obj_size));		\
	if (_fl_->objlist) {						\
		_obj_ = _cast_((_fl_)->objlist);			\
		(_fl_)->objlist = (void *)((_obj_)->_nextp_);		\
		(_fl_)->free_cnt--;					\
	}								\
	else {								\
		/*							\
		 * Allocate one at a time so we can free		\
		 * one at a time without cleverness when arena		\
		 * is full.						\
		 */							\
		RF_Calloc(_obj_, 1, (_fl_)->obj_size, _cast_);		\
		if (_obj_) {						\
			if (_init_(_obj_)) {				\
				RF_Free(_obj_, (_fl_)->obj_size);	\
				_obj_ = NULL;				\
			}						\
			else {						\
				for (_i = 1; _i < (_fl_)->obj_inc;	\
				     _i++) {				\
					RF_Calloc(_p, 1,		\
					    (_fl_)->obj_size,		\
					    (void *));			\
					if (_p) {			\
						if (_init_(_p)) {	\
							RF_Free(_p,	\
						  (_fl_)->obj_size);	\
							_p = NULL;	\
							break;		\
						}			\
						(_cast_(_p))->_nextp_ =	\
						    (_fl_)->objlist;	\
						(_fl_)->objlist = _p;	\
					}				\
					else {				\
						break;			\
					}				\
				}					\
			}						\
		} 							\
		RF_FREELIST_STAT_GROW(_fl_);				\
	}								\
	RF_FREELIST_STAT_ALLOC(_fl_);					\
	RF_UNLOCK_MUTEX((_fl_)->lock);					\
} while (0)

/*
 * fl	 = FreeList.
 * obj	 = Object to allocate.
 * nextp = Name of "next" pointer in obj.
 * cast	 = Cast of obj assignment.
 * init	 = Init obj func.
 * arg	 = Arg to init obj func.
 */
#define	RF_FREELIST_GET_INIT_ARG(_fl_,_obj_,_nextp_,_cast_,_init_,_arg_) \
do {									\
	void *_p;							\
	int _i;								\
	RF_LOCK_MUTEX((_fl_)->lock);					\
	RF_ASSERT(sizeof(*(_obj_)) == ((_fl_)->obj_size));		\
	if (_fl_->objlist) {						\
		_obj_ = _cast_((_fl_)->objlist);			\
		(_fl_)->objlist = (void *)((_obj_)->_nextp_);		\
		(_fl_)->free_cnt--;					\
	}								\
	else {								\
		/*							\
		 * Allocate one at a time so we can free		\
		 * one at a time without cleverness when arena		\
		 * is full.						\
		 */							\
		RF_Calloc(_obj_, 1, (_fl_)->obj_size, _cast_);		\
		if (_obj_) {						\
			if (_init_(_obj_, _arg_)) {			\
				RF_Free(_obj_, (_fl_)->obj_size);	\
				_obj_ = NULL;				\
			}						\
			else {						\
				for (_i = 1; _i < (_fl_)->obj_inc;	\
				     _i++) {				\
					RF_Calloc(_p, 1,		\
					   (_fl_)->obj_size, (void *));	\
					if (_p) {			\
						if (_init_(_p, _arg_))	\
						{			\
							RF_Free(_p,	\
						    (_fl_)->obj_size);	\
							_p = NULL;	\
							break;		\
						}			\
						(_cast_(_p))->_nextp_ =	\
						    (_fl_)->objlist;	\
						(_fl_)->objlist = _p;	\
					}				\
					else {				\
						break;			\
					}				\
				}					\
			}						\
		}							\
		RF_FREELIST_STAT_GROW(_fl_);				\
	}								\
	RF_FREELIST_STAT_ALLOC(_fl_);					\
	RF_UNLOCK_MUTEX((_fl_)->lock);					\
} while (0)

/*
 * fl	 = FreeList.
 * obj	 = Object to allocate.
 * nextp = Name of "next" pointer in obj.
 * cast	 = Cast of obj assignment.
 * init	 = Init obj func.
 */
#define	RF_FREELIST_GET_INIT_NOUNLOCK(_fl_,_obj_,_nextp_,_cast_,_init_)	\
do {									\
	void *_p;							\
	int _i;								\
	RF_LOCK_MUTEX((_fl_)->lock);					\
	RF_ASSERT(sizeof(*(_obj_)) == ((_fl_)->obj_size));		\
	if (_fl_->objlist) {						\
		_obj_ = _cast_((_fl_)->objlist);			\
		(_fl_)->objlist = (void *)((_obj_)->_nextp_);		\
		(_fl_)->free_cnt--;					\
	}								\
	else {								\
		/*							\
		 * Allocate one at a time so we can free		\
		 * one at a time without cleverness when arena		\
		 * is full.						\
		 */							\
		RF_Calloc(_obj_, 1, (_fl_)->obj_size, _cast_);		\
		if (_obj_) {						\
			if (_init_(_obj_)) {				\
				RF_Free(_obj_, (_fl_)->obj_size);	\
				_obj_ = NULL;				\
			}						\
			else {						\
				for (_i = 1; _i < (_fl_)->obj_inc;	\
				     _i++) {				\
					RF_Calloc(_p, 1,		\
					    (_fl_)->obj_size,		\
					    (void *));			\
					if (_p) {			\
						if (_init_(_p)) {	\
							RF_Free(_p,	\
						    (_fl_)->obj_size);	\
							_p = NULL;	\
							break;		\
						}			\
						(_cast_(_p))->_nextp_ =	\
						    (_fl_)->objlist;	\
						(_fl_)->objlist = _p;	\
					}				\
					else {				\
						break;			\
					}				\
				}					\
			}						\
		}							\
		RF_FREELIST_STAT_GROW(_fl_);				\
	}								\
	RF_FREELIST_STAT_ALLOC(_fl_);					\
} while (0)

/*
 * fl	 = FreeList.
 * obj	 = Object to allocate.
 * nextp = Name of "next" pointer in obj.
 * cast	 = Cast of obj assignment.
 */
#define	RF_FREELIST_GET(_fl_,_obj_,_nextp_,_cast_)			\
do {									\
	void *_p;							\
	int _i;								\
	RF_LOCK_MUTEX((_fl_)->lock);					\
	RF_ASSERT(sizeof(*(_obj_)) == ((_fl_)->obj_size));		\
	if (_fl_->objlist) {						\
		_obj_ = _cast_((_fl_)->objlist);			\
		(_fl_)->objlist = (void *)((_obj_)->_nextp_);		\
		(_fl_)->free_cnt--;					\
	}								\
	else {								\
		/*							\
		 * Allocate one at a time so we can free		\
		 * one at a time without cleverness when arena		\
		 * is full.						\
		 */							\
		RF_Calloc(_obj_, 1, (_fl_)->obj_size, _cast_);		\
		if (_obj_) {						\
			for (_i = 1; _i < (_fl_)->obj_inc; _i++) {	\
				RF_Calloc(_p, 1, (_fl_)->obj_size,	\
				    (void *));				\
				if (_p) {				\
					(_cast_(_p))->_nextp_ =		\
					    (_fl_)->objlist;		\
					(_fl_)->objlist = _p;		\
				}					\
				else {					\
					break;				\
				}					\
			}						\
		}							\
		RF_FREELIST_STAT_GROW(_fl_);				\
	}								\
	RF_FREELIST_STAT_ALLOC(_fl_);					\
	RF_UNLOCK_MUTEX((_fl_)->lock);					\
} while (0)

/*
 * fl	 = FreeList.
 * obj	 = Object to allocate.
 * nextp = Name of "next" pointer in obj.
 * cast	 = Cast of obj assignment.
 * num	 = Num objs to return.
 */
#define	RF_FREELIST_GET_N(_fl_,_obj_,_nextp_,_cast_,_num_)		\
do {									\
	void *_p, *_l, *_f;						\
	int _i, _n;							\
	_l = _f = NULL;							\
	_n = 0;								\
	RF_LOCK_MUTEX((_fl_)->lock);					\
	RF_ASSERT(sizeof(*(_obj_)) == ((_fl_)->obj_size));		\
	for (_n = 0; _n < _num_; _n++) {				\
		if (_fl_->objlist) {					\
			_obj_ = _cast_((_fl_)->objlist);		\
			(_fl_)->objlist = (void *)((_obj_)->_nextp_);	\
			(_fl_)->free_cnt--;				\
		}							\
		else {							\
			/*						\
			 * Allocate one at a time so we can free	\
			 * one at a time without cleverness when arena	\
			 * is full.					\
			 */						\
			RF_Calloc(_obj_, 1, (_fl_)->obj_size, _cast_);	\
			if (_obj_) {					\
				for (_i = 1; _i < (_fl_)->obj_inc;	\
				     _i++) {				\
					RF_Calloc(_p, 1,		\
					    (_fl_)->obj_size,		\
					    (void *));			\
					if (_p) {			\
						(_cast_(_p))->_nextp_ =	\
						    (_fl_)->objlist;	\
						(_fl_)->objlist = _p;	\
					}				\
					else {				\
						break;			\
					}				\
				}					\
			}						\
			RF_FREELIST_STAT_GROW(_fl_);			\
		}							\
		if (_f == NULL)						\
			_f = _obj_;					\
		if (_obj_) {						\
			(_cast_(_obj_))->_nextp_ = _l;			\
			_l = _obj_;					\
			RF_FREELIST_STAT_ALLOC(_fl_);			\
		}							\
		else {							\
			(_cast_(_f))->_nextp_ = (_fl_)->objlist;	\
			(_fl_)->objlist = _l;				\
			_n = _num_;					\
		}							\
	}								\
	RF_UNLOCK_MUTEX((_fl_)->lock);					\
} while (0)

/*
 * fl	 = FreeList.
 * obj	 = Object to free.
 * nextp = Name of "next" pointer in obj.
 */
#define	RF_FREELIST_FREE(_fl_,_obj_,_nextp_)				\
do {									\
	RF_LOCK_MUTEX((_fl_)->lock);					\
	if ((_fl_)->free_cnt == (_fl_)->max_free_cnt) {			\
		RF_Free(_obj_, (_fl_)->obj_size);			\
	}								\
	else {								\
		RF_ASSERT((_fl_)->free_cnt < (_fl_)->max_free_cnt);	\
		(_obj_)->_nextp_ = (_fl_)->objlist;			\
		(_fl_)->objlist = (void *)(_obj_);			\
		(_fl_)->free_cnt++;					\
	}								\
	RF_FREELIST_STAT_FREE(_fl_);					\
	RF_UNLOCK_MUTEX((_fl_)->lock);					\
} while (0)

/*
 * fl	 = FreeList.
 * obj	 = Object to free.
 * nextp = Name of "next" pointer in obj.
 * num	 = Num to free (debugging).
 */
#define	RF_FREELIST_FREE_N(_fl_,_obj_,_nextp_,_cast_,_num_)		\
do {									\
	void *_no;							\
	int _n;								\
	_n = 0;								\
	RF_LOCK_MUTEX((_fl_)->lock);					\
	while(_obj_) {							\
		_no = (_cast_(_obj_))->_nextp_;				\
		if ((_fl_)->free_cnt == (_fl_)->max_free_cnt) {		\
			RF_Free(_obj_, (_fl_)->obj_size);		\
		}							\
		else {							\
			RF_ASSERT((_fl_)->free_cnt <			\
			    (_fl_)->max_free_cnt);			\
			(_obj_)->_nextp_ = (_fl_)->objlist;		\
			(_fl_)->objlist = (void *)(_obj_);		\
			(_fl_)->free_cnt++;				\
		}							\
		_n++;							\
		_obj_ = _no;						\
		RF_FREELIST_STAT_FREE(_fl_);				\
	}								\
	RF_ASSERT(_n==(_num_));						\
	RF_UNLOCK_MUTEX((_fl_)->lock);					\
} while (0)

/*
 * fl	 = FreeList.
 * obj	 = Object to free.
 * nextp = Name of "next" pointer in obj.
 * clean = Undo for init.
 */
#define	RF_FREELIST_FREE_CLEAN(_fl_,_obj_,_nextp_,_clean_)		\
do {									\
	RF_LOCK_MUTEX((_fl_)->lock);					\
	if ((_fl_)->free_cnt == (_fl_)->max_free_cnt) {			\
		_clean_(_obj_);					\
		RF_Free(_obj_, (_fl_)->obj_size);			\
	}								\
	else {								\
		RF_ASSERT((_fl_)->free_cnt < (_fl_)->max_free_cnt);	\
		(_obj_)->_nextp_ = (_fl_)->objlist;			\
		(_fl_)->objlist = (void *)(_obj_);			\
		(_fl_)->free_cnt++;					\
	}								\
	RF_FREELIST_STAT_FREE(_fl_);					\
	RF_UNLOCK_MUTEX((_fl_)->lock);					\
} while (0)

/*
 * fl	 = FreeList.
 * obj	 = Object to free.
 * nextp = Name of "next" pointer in obj.
 * clean = Undo for init.
 * arg	 = Arg for undo func.
 */
#define	RF_FREELIST_FREE_CLEAN_ARG(_fl_,_obj_,_nextp_,_clean_,_arg_)	\
do {									\
	RF_LOCK_MUTEX((_fl_)->lock);					\
	if ((_fl_)->free_cnt == (_fl_)->max_free_cnt) {			\
		_clean_(_obj_, _arg_);					\
		RF_Free(_obj_, (_fl_)->obj_size);			\
	}								\
	else {								\
		RF_ASSERT((_fl_)->free_cnt < (_fl_)->max_free_cnt);	\
		(_obj_)->_nextp_ = (_fl_)->objlist;			\
		(_fl_)->objlist = (void *)(_obj_);			\
		(_fl_)->free_cnt++;					\
	}								\
	RF_FREELIST_STAT_FREE(_fl_);					\
	RF_UNLOCK_MUTEX((_fl_)->lock);					\
} while (0)

/*
 * fl	 = FreeList.
 * obj	 = Object to free.
 * nextp = Name of "next" pointer in obj.
 * clean = Undo for init.
 */
#define	RF_FREELIST_FREE_CLEAN_NOUNLOCK(_fl_,_obj_,_nextp_,_clean_)	\
do {									\
	RF_LOCK_MUTEX((_fl_)->lock);					\
	if ((_fl_)->free_cnt == (_fl_)->max_free_cnt) {			\
		_clean_(_obj_);					\
		RF_Free(_obj_, (_fl_)->obj_size);			\
	}								\
	else {								\
		RF_ASSERT((_fl_)->free_cnt < (_fl_)->max_free_cnt);	\
		(_obj_)->_nextp_ = (_fl_)->objlist;			\
		(_fl_)->objlist = (void *)(_obj_);			\
		(_fl_)->free_cnt++;					\
	}								\
	RF_FREELIST_STAT_FREE(_fl_);					\
} while (0)

/*
 * fl	 = FreeList.
 * nextp = Name of "next" pointer in obj.
 * cast	 = Cast to object type.
 */
#define	RF_FREELIST_DESTROY(_fl_,_nextp_,_cast_)			\
do {									\
	void *_cur, *_next;						\
	RF_FREELIST_STAT_REPORT(_fl_);					\
	rf_mutex_destroy(&((_fl_)->lock));				\
	for (_cur = (_fl_)->objlist; _cur; _cur = _next) {		\
		_next = (_cast_ _cur)->_nextp_;				\
		RF_Free(_cur, (_fl_)->obj_size);			\
	}								\
	RF_Free(_fl_, sizeof(RF_FreeList_t));				\
} while (0)

/*
 * fl	 = FreeList.
 * nextp = Name of "next" pointer in obj.
 * cast	 = Cast to object type.
 * clean = Func to undo obj init.
 */
#define	RF_FREELIST_DESTROY_CLEAN(_fl_,_nextp_,_cast_,_clean_)		\
do {									\
	void *_cur, *_next;						\
	RF_FREELIST_STAT_REPORT(_fl_);					\
	rf_mutex_destroy(&((_fl_)->lock));				\
	for (_cur = (_fl_)->objlist; _cur; _cur = _next) {		\
		_next = (_cast_ _cur)->_nextp_;				\
		_clean_(_cur);						\
		RF_Free(_cur, (_fl_)->obj_size);			\
	}								\
	RF_Free(_fl_, sizeof(RF_FreeList_t));				\
} while (0)

/*
 * fl	 = FreeList.
 * nextp = Name of "next" pointer in obj.
 * cast	 = Cast to object type.
 * clean = Func to undo obj init.
 * arg	 = Arg for undo func.
 */
#define	RF_FREELIST_DESTROY_CLEAN_ARG(_fl_,_nextp_,_cast_,_clean_,_arg_) \
do {									\
	void *_cur, *_next;						\
	RF_FREELIST_STAT_REPORT(_fl_);					\
	rf_mutex_destroy(&((_fl_)->lock));				\
	for (_cur = (_fl_)->objlist; _cur; _cur = _next) {		\
		_next = (_cast_ _cur)->_nextp_;				\
		_clean_(_cur, _arg_);					\
		RF_Free(_cur, (_fl_)->obj_size);			\
	}								\
	RF_Free(_fl_, sizeof(RF_FreeList_t));				\
} while (0)

#endif	/* !_RF__RF_FREELIST_H_ */
