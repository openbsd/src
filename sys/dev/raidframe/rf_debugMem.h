/*	$OpenBSD: rf_debugMem.h,v 1.2 1999/02/16 00:02:34 niklas Exp $	*/
/*	$NetBSD: rf_debugMem.h,v 1.4 1999/02/05 00:06:08 oster Exp $	*/
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
 */

#ifndef _RF__RF_DEBUGMEM_H_
#define _RF__RF_DEBUGMEM_H_

#include "rf_archs.h"
#include "rf_alloclist.h"
#include "rf_options.h"

#ifndef _KERNEL

#if !defined(__NetBSD__) && !defined(__OpenBSD__)
void   *malloc(), *calloc();
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
#else				/* RF_MEMORY_REDZONES > 0 */
#define rf_redzone_malloc(_p_,_size_)        _p_ = malloc(_size_)
#define rf_redzone_calloc(_p_,_nel_,_size_)  _p_ = calloc(_nel_,_size_)
#define rf_redzone_free(_ptr_)             free(_ptr_)
#endif				/* RF_MEMORY_REDZONES > 0 */

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

#else				/* KERNEL */

#include <sys/types.h>
typedef u_int32_t U32;
#include <sys/malloc.h>



#define RF_Malloc(_p_, _size_, _cast_)                                      \
  {                                                                      \
     _p_ = _cast_ malloc((u_long)_size_, M_RAIDFRAME, M_WAITOK); \
     bzero((char *)_p_, _size_); \
     if (rf_memDebug) rf_record_malloc(_p_, _size_, __LINE__, __FILE__);       \
  }

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
	free((void *)(_p_), M_RAIDFRAME);                                      \
    if (rf_memDebug) rf_unrecord_malloc(_p_, (U32) (_sz_));                     \
  }

#endif				/* _KERNEL */

#ifndef _KERNEL
void   *rf_real_redzone_malloc(int size);
void   *rf_real_redzone_calloc(int n, int size);
void    rf_real_redzone_free(char *p, int line, char *filen);
char   *rf_real_Malloc(int size, int line, char *file);
char   *rf_real_Calloc(int nel, int elsz, int line, char *file);
void    rf_real_Free(void *p, int sz, int line, char *file);
void    rf_validate_mh_table(void);
#if RF_UTILITY == 0
char   *rf_real_MallocAndAdd(int size, RF_AllocListElem_t * alist, int line, char *file);
char   *rf_real_CallocAndAdd(int nel, int elsz, RF_AllocListElem_t * alist, int line, char *file);
#endif				/* RF_UTILITY == 0 */
#endif				/* !KERNEL */

void    rf_record_malloc(void *p, int size, int line, char *filen);
void    rf_unrecord_malloc(void *p, int sz);
void    rf_print_unfreed(void);
int     rf_ConfigureDebugMem(RF_ShutdownList_t ** listp);
void    rf_ReportMaxMem(void);

#endif				/* !_RF__RF_DEBUGMEM_H_ */
