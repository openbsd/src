/*	$OpenBSD: rf_alloclist.h,v 1.1 1999/01/11 14:28:59 niklas Exp $	*/
/*	$NetBSD: rf_alloclist.h,v 1.1 1998/11/13 04:20:26 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland
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

/****************************************************************************
 *
 * alloclist.h -- header file for alloclist.c
 *
 ***************************************************************************/

/* :  
 * Log: rf_alloclist.h,v 
 * Revision 1.11  1996/07/18 22:57:14  jimz
 * port simulator to AIX
 *
 * Revision 1.10  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.9  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.8  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.7  1995/11/30  16:27:13  wvcii
 * added copyright info
 *
 */

#ifndef _RF__RF_ALLOCLIST_H_
#define _RF__RF_ALLOCLIST_H_

#include "rf_types.h"

#define RF_POINTERS_PER_ALLOC_LIST_ELEMENT 20

struct RF_AllocListElem_s {
  void                *pointers[RF_POINTERS_PER_ALLOC_LIST_ELEMENT];
  int                  sizes[RF_POINTERS_PER_ALLOC_LIST_ELEMENT];
  int                  numPointers;
  RF_AllocListElem_t  *next;
};

#define rf_MakeAllocList(_ptr_) _ptr_ = rf_real_MakeAllocList(1);
#define rf_AddToAllocList(_l_,_ptr_,_sz_) rf_real_AddToAllocList((_l_), (_ptr_), (_sz_), 1)

int rf_ConfigureAllocList(RF_ShutdownList_t **listp);

#if RF_UTILITY == 0
void rf_real_AddToAllocList(RF_AllocListElem_t *l, void *p, int size, int lockflag);
void rf_FreeAllocList(RF_AllocListElem_t *l);
RF_AllocListElem_t *rf_real_MakeAllocList(int lockflag);
#endif /* RF_UTILITY == 0 */

#endif /* !_RF__RF_ALLOCLIST_H_ */
