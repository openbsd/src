/*	$OpenBSD: rf_debugMem.h,v 1.1 1999/01/11 14:29:12 niklas Exp $	*/
/*	$NetBSD: rf_debugMem.h,v 1.1 1998/11/13 04:20:28 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Daniel Stodolsky, Mark Holland
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
 * rf_debugMem.h -- memory leak debugging module
 *
 * IMPORTANT:  if you put the lock/unlock mutex stuff back in here, you
 *             need to take it out of the routines in debugMem.c
 *
 * Log: rf_debugMem.h,v 
 * Revision 1.27  1996/07/18 22:57:14  jimz
 * port simulator to AIX
 *
 * Revision 1.26  1996/06/11  13:46:43  jimz
 * make bracing consistent around memory allocation macros
 *
 * Revision 1.25  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.24  1996/06/05  18:06:02  jimz
 * Major code cleanup. The Great Renaming is now done.
 * Better modularity. Better typing. Fixed a bunch of
 * synchronization bugs. Made a lot of global stuff
 * per-desc or per-array. Removed dead code.
 *
 * Revision 1.23  1996/05/30  11:29:41  jimz
 * Numerous bug fixes. Stripe lock release code disagreed with the taking code
 * about when stripes should be locked (I made it consistent: no parity, no lock)
 * There was a lot of extra serialization of I/Os which I've removed- a lot of
 * it was to calculate values for the cache code, which is no longer with us.
 * More types, function, macro cleanup. Added code to properly quiesce the array
 * on shutdown. Made a lot of stuff array-specific which was (bogusly) general
 * before. Fixed memory allocation, freeing bugs.
 *
 * Revision 1.22  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.21  1996/05/23  22:17:40  jimz
 * fix alloclist macro names for kernel
 *
 * Revision 1.20  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.19  1996/05/23  13:18:23  jimz
 * include rf_options.h
 *
 * Revision 1.18  1996/05/23  00:33:23  jimz
 * code cleanup: move all debug decls to rf_options.c, all extern
 * debug decls to rf_options.h, all debug vars preceded by rf_
 *
 * Revision 1.17  1996/05/21  18:51:54  jimz
 * cleaned up macro args
 *
 * Revision 1.16  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.15  1996/05/01  16:26:22  jimz
 * get rid of old ccmn stuff
 *
 * Revision 1.14  1995/12/01  15:58:09  root
 * added copyright info
 *
 * Revision 1.13  1995/10/11  15:26:03  jimz
 * zero memory after allocation in kernel (hide effects
 * of uninitialized structs)
 *
 * Revision 1.12  1995/10/06  17:04:15  jimz
 * make Malloc and Free in kernel use kernel malloc package, not cam
 * dbufs (which is gross, and was exhausting cam zalloc limit)
 *
 * Revision 1.11  1995/05/01  13:28:00  holland
 * parity range locks, locking disk requests, recon+parityscan in kernel, etc.
 *
 * Revision 1.10  1995/04/24  13:25:51  holland
 * rewrite to move disk queues, recon, & atomic RMW to kernel
 *
 * Revision 1.9  1995/02/17  19:39:56  holland
 * added size param to all calls to Free().
 * this is ignored at user level, but necessary in the kernel.
 *
 * Revision 1.8  1995/02/10  17:34:10  holland
 * kernelization changes
 *
 * Revision 1.7  1995/02/03  22:31:36  holland
 * many changes related to kernelization
 *
 * Revision 1.6  1995/02/01  15:13:05  holland
 * moved #include of general.h out of raid.h and into each file
 *
 * Revision 1.5  1995/02/01  14:25:19  holland
 * began changes for kernelization:
 *      changed all instances of mutex_t and cond_t to DECLARE macros
 *      converted configuration code to use config structure
 *
 * Revision 1.4  1995/01/11  19:27:02  holland
 * many changes related to performance tuning
 *
 * Revision 1.3  1994/11/29  21:34:56  danner
 * Changed type of redzone_calloc and malloc to void *.
 *
 * Revision 1.2  1994/11/28  22:13:23  danner
 * Many macros converted to functions.
 *
 */

#ifndef _RF__RF_DEBUGMEM_H_
#define _RF__RF_DEBUGMEM_H_

#include "rf_archs.h"
#include "rf_alloclist.h"
#include "rf_options.h"

#ifndef KERNEL

#if !defined(__NetBSD__) && !defined(__OpenBSD__)
void *malloc(), *calloc();
#endif
RF_DECLARE_EXTERN_MUTEX(rf_debug_mem_mutex)

/*
 * redzone malloc, calloc, and free allocate an extra 16 bytes on each
 * malloc/calloc call to allow tracking of overflows on free.
 */

#if RF_MEMORY_REDZONES > 0
#define rf_redzone_malloc(_p_,_size_)     _p_ = rf_real_redzone_malloc(_size_)
#define rf_redzone_calloc(_p_,_n_,_size_) _p_ = rf_real_redzone_calloc(_n_,_size_)
#define rf_redzone_free(_p_)              rf_real_redzone_free(_p_, __LINE__, __FILE__)
#else /* RF_MEMORY_REDZONES > 0 */
#define rf_redzone_malloc(_p_,_size_)        _p_ = malloc(_size_)
#define rf_redzone_calloc(_p_,_nel_,_size_)  _p_ = calloc(_nel_,_size_)
#define rf_redzone_free(_ptr_)             free(_ptr_)
#endif /* RF_MEMORY_REDZONES > 0 */

#define RF_Malloc(_p_, _size_, _cast_) { \
      _p_ = _cast_ rf_real_Malloc(_size_, __LINE__, __FILE__); \
}

#define RF_MallocAndAdd(_p_, _size_, _cast_, _alist_) { \
      _p_ = _cast_ rf_real_MallocAndAdd(_size_, _alist_, __LINE__, __FILE__); \
}

#define RF_Calloc(_p_, _nel_, _elsz_, _cast_) { \
      _p_ = _cast_ rf_real_Calloc(_nel_, _elsz_, __LINE__, __FILE__); \
}

#define RF_CallocAndAdd(_p_, _nel_, _elsz_, _cast_, _alist_) { \
      _p_ = _cast_ rf_real_CallocAndAdd(_nel_, _elsz_, _alist_, __LINE__, __FILE__); \
}

#define RF_Free(__p_, _sz_) { \
      rf_real_Free(__p_, _sz_, __LINE__, __FILE__); \
}

#else  /* KERNEL */

#include <sys/types.h>
#if defined(__NetBSD__) || defined(__OpenBSD__)
typedef u_int32_t U32;
#else
#include <io/common/iotypes.h>                   /* just to get defn of U32 */
#endif /* __NetBSD__ || __OpenBSD__ */
#include <sys/malloc.h>


#if defined(__NetBSD__) || defined(__OpenBSD__)

#define RF_Malloc(_p_, _size_, _cast_)                                      \
  {                                                                      \
     _p_ = _cast_ malloc((u_long)_size_, M_DEVBUF, M_WAITOK); \
     bzero((char *)_p_, _size_); \
     if (rf_memDebug) rf_record_malloc(_p_, _size_, __LINE__, __FILE__);       \
  }

#else

#define RF_Malloc(_p_, _size_, _cast_)                                      \
  {                                                                      \
     _p_ = _cast_ malloc((u_long)_size_, BUCKETINDEX(_size_), M_DEVBUF, M_WAITOK); \
     bzero((char *)_p_, _size_); \
     if (rf_memDebug) rf_record_malloc(_p_, _size_, __LINE__, __FILE__);       \
  }
#endif /* __NetBSD__ || __OpenBSD__ */

#define RF_MallocAndAdd(__p_, __size_, __cast_, __alist_)                   \
  {                                                                      \
     RF_Malloc(__p_, __size_, __cast_);                                     \
     if (__alist_) rf_AddToAllocList(__alist_, __p_, __size_);              \
  }

#define RF_Calloc(_p_, _nel_, _elsz_, _cast_)                               \
  {                                                                      \
     RF_Malloc( _p_, (_nel_) * (_elsz_), _cast_);                           \
     bzero( (_p_), (_nel_) * (_elsz_) );                                 \
   }

#define RF_CallocAndAdd(__p,__nel,__elsz,__cast,__alist)                     \
  {                                                                       \
     RF_Calloc(__p, __nel, __elsz, __cast);                                  \
     if (__alist) rf_AddToAllocList(__alist, __p, (__nel)*(__elsz));         \
  }

#define RF_Free(_p_, _sz_)                                                   \
  {                                                                       \
	free((void *)(_p_), M_DEVBUF);                                        \
    if (rf_memDebug) rf_unrecord_malloc(_p_, (U32) (_sz_));                     \
  }

#endif /* KERNEL */

#ifndef KERNEL
void *rf_real_redzone_malloc(int size);
void *rf_real_redzone_calloc(int n, int size);
void rf_real_redzone_free(char *p, int line, char *filen);
char *rf_real_Malloc(int size, int line, char *file);
char *rf_real_Calloc(int nel, int elsz, int line, char *file);
void rf_real_Free(void *p, int sz, int line, char *file);
void rf_validate_mh_table(void);
#if RF_UTILITY == 0
char *rf_real_MallocAndAdd(int size, RF_AllocListElem_t *alist, int line, char *file);
char *rf_real_CallocAndAdd(int nel, int elsz, RF_AllocListElem_t *alist, int line, char *file);
#endif /* RF_UTILITY == 0 */
#endif /* !KERNEL */

void rf_record_malloc(void *p, int size, int line, char *filen);
void rf_unrecord_malloc(void *p, int sz);
void rf_print_unfreed(void);
int rf_ConfigureDebugMem(RF_ShutdownList_t **listp);
void rf_ReportMaxMem(void);

#endif /* !_RF__RF_DEBUGMEM_H_ */
