/*	$OpenBSD: rf_alloclist.c,v 1.1 1999/01/11 14:28:58 niklas Exp $	*/
/*	$NetBSD: rf_alloclist.c,v 1.1 1998/11/13 04:20:26 oster Exp $	*/
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

/*
 * Log: rf_alloclist.c,v 
 * Revision 1.28  1996/07/27 23:36:08  jimz
 * Solaris port of simulator
 *
 * Revision 1.27  1996/06/12  03:29:54  jimz
 * don't barf just because we can't create an alloclist
 *
 * Revision 1.26  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.25  1996/06/09  02:36:46  jimz
 * lots of little crufty cleanup- fixup whitespace
 * issues, comment #ifdefs, improve typing in some
 * places (esp size-related)
 *
 * Revision 1.24  1996/06/05  18:06:02  jimz
 * Major code cleanup. The Great Renaming is now done.
 * Better modularity. Better typing. Fixed a bunch of
 * synchronization bugs. Made a lot of global stuff
 * per-desc or per-array. Removed dead code.
 *
 * Revision 1.23  1996/05/30  23:22:16  jimz
 * bugfixes of serialization, timing problems
 * more cleanup
 *
 * Revision 1.22  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.21  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.20  1996/05/20  16:15:59  jimz
 * switch to rf_{mutex,cond}_{init,destroy}
 *
 * Revision 1.19  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.18  1996/05/16  22:27:45  jimz
 * get rid of surreal_MakeAllocList (what was that, anyway?)
 *
 * Revision 1.17  1995/12/12  18:10:06  jimz
 * MIN -> RF_MIN, MAX -> RF_MAX, ASSERT -> RF_ASSERT
 * fix 80-column brain damage in comments
 *
 * Revision 1.16  1995/11/30  16:27:07  wvcii
 * added copyright info
 *
 * Revision 1.15  1995/10/05  20:37:56  jimz
 * assert non-NULLness of pointer to FREE in FreeAllocList()
 *
 * Revision 1.14  1995/06/11  20:11:24  holland
 * changed fl_hist,miss_count from long to int to get around weird kernel bug
 *
 * Revision 1.13  1995/05/01  13:28:00  holland
 * parity range locks, locking disk requests, recon+parityscan in kernel, etc.
 *
 * Revision 1.12  1995/04/21  19:13:04  holland
 * minor change to avoid a syntax error on DO_FREE
 *
 * Revision 1.11  1995/02/17  19:39:56  holland
 * added size param to all calls to Free().
 * this is ignored at user level, but necessary in the kernel.
 *
 * Revision 1.10  1995/02/10  18:08:07  holland
 * added DO_FREE macro to fix what I broke during kernelization
 *
 * Revision 1.9  1995/02/10  17:34:10  holland
 * kernelization changes
 *
 * Revision 1.8  1995/02/03  22:31:36  holland
 * many changes related to kernelization
 *
 * Revision 1.7  1995/02/01  15:13:05  holland
 * moved #include of general.h out of raid.h and into each file
 *
 * Revision 1.6  1995/01/11  19:27:02  holland
 * many changes related to performance tuning
 *
 * Revision 1.5  1994/11/29  20:53:10  danner
 * Marks mods
 *
 * Revision 1.3  1994/11/19  21:01:07  danner
 * First merge with mark
 *
 * Revision 1.1.1.1  1994/11/19  20:23:38  danner
 * First PQ checkin
 *
 * Revision 1.2  1994/11/16  15:45:35  danner
 * fixed free bug in FreeAllocList
 *
 *
 */

/****************************************************************************
 *
 * Alloclist.c -- code to manipulate allocation lists
 *
 * an allocation list is just a list of AllocListElem structures.  Each
 * such structure contains a fixed-size array of pointers.  Calling
 * FreeAList() causes each pointer to be freed.
 *
 ***************************************************************************/

#include "rf_types.h"
#include "rf_threadstuff.h"
#include "rf_alloclist.h"
#include "rf_debugMem.h"
#include "rf_etimer.h"
#include "rf_general.h"
#include "rf_shutdown.h"
#include "rf_sys.h"

RF_DECLARE_STATIC_MUTEX(alist_mutex)
static unsigned int fl_hit_count, fl_miss_count;

static RF_AllocListElem_t *al_free_list=NULL;
static int al_free_list_count;

#define RF_AL_FREELIST_MAX 256

#ifndef KERNEL
#define DO_FREE(_p,_sz) free((_p))
#else /* !KERNEL */
#define DO_FREE(_p,_sz) RF_Free((_p),(_sz))
#endif /* !KERNEL */

static void rf_ShutdownAllocList(void *);

static void rf_ShutdownAllocList(ignored)
  void  *ignored;
{
  RF_AllocListElem_t *p, *pt;

  for (p = al_free_list; p; ) {
    pt = p;
    p = p->next;
    DO_FREE(pt, sizeof(*pt));
  }
  rf_mutex_destroy(&alist_mutex);
  /*
  printf("Alloclist: Free list hit count %lu (%lu %%) miss count %lu (%lu %%)\n",
	 fl_hit_count, (100*fl_hit_count)/(fl_hit_count+fl_miss_count),
	 fl_miss_count, (100*fl_miss_count)/(fl_hit_count+fl_miss_count));
  */
}

int rf_ConfigureAllocList(listp)
  RF_ShutdownList_t  **listp;
{
  int rc;

  rc = rf_mutex_init(&alist_mutex);
  if (rc) {
    RF_ERRORMSG3("Unable to init mutex file %s line %d rc=%d\n", __FILE__,
      __LINE__, rc);
    return(rc);
  }
  al_free_list = NULL;
  fl_hit_count = fl_miss_count = al_free_list_count = 0;
  rc = rf_ShutdownCreate(listp, rf_ShutdownAllocList, NULL);
  if (rc) {
    RF_ERRORMSG3("Unable to add to shutdown list file %s line %d rc=%d\n",
      __FILE__, __LINE__, rc);
    rf_mutex_destroy(&alist_mutex);
    return(rc);
  }
  return(0);
}


/* we expect the lists to have at most one or two elements, so we're willing
 * to search for the end.  If you ever observe the lists growing longer,
 * increase POINTERS_PER_ALLOC_LIST_ELEMENT.
 */
void rf_real_AddToAllocList(l, p, size, lockflag)
  RF_AllocListElem_t  *l;
  void                *p;
  int                  size;
  int                  lockflag;
{
  RF_AllocListElem_t *newelem;

  for ( ; l->next; l=l->next) 
	  RF_ASSERT(l->numPointers == RF_POINTERS_PER_ALLOC_LIST_ELEMENT);  /* find end of list */
  
  RF_ASSERT(l->numPointers >= 0 && l->numPointers <= RF_POINTERS_PER_ALLOC_LIST_ELEMENT);
  if (l->numPointers == RF_POINTERS_PER_ALLOC_LIST_ELEMENT) {
    newelem = rf_real_MakeAllocList(lockflag);
    l->next = newelem;
    l = newelem;
  }
  l->pointers[ l->numPointers ] = p;
  l->sizes   [ l->numPointers ] = size;
  l->numPointers++;

}


/* we use the debug_mem_mutex here because we need to lock it anyway to call free.
 * this is probably a bug somewhere else in the code, but when I call malloc/free
 * outside of any lock I have endless trouble with malloc appearing to return the
 * same pointer twice.  Since we have to lock it anyway, we might as well use it
 * as the lock around the al_free_list.  Note that we can't call Free with the
 * debug_mem_mutex locked.
 */
void rf_FreeAllocList(l)
  RF_AllocListElem_t  *l;
{
  int i;
  RF_AllocListElem_t *temp, *p;

  for (p=l; p; p=p->next) {
    RF_ASSERT(p->numPointers >= 0 && p->numPointers <= RF_POINTERS_PER_ALLOC_LIST_ELEMENT);
    for (i=0; i<p->numPointers; i++) {
      RF_ASSERT(p->pointers[i]);
      RF_Free(p->pointers[i], p->sizes[i]);
    }
  }
#ifndef KERNEL
  RF_LOCK_MUTEX(rf_debug_mem_mutex);
#endif /* !KERNEL */
  while (l) {
    temp = l;
    l = l->next;
    if (al_free_list_count > RF_AL_FREELIST_MAX) {DO_FREE(temp, sizeof(*temp));}
    else {temp->next = al_free_list; al_free_list = temp; al_free_list_count++;}
  }
#ifndef KERNEL
  RF_UNLOCK_MUTEX(rf_debug_mem_mutex);
#endif /* !KERNEL */
}

RF_AllocListElem_t *rf_real_MakeAllocList(lockflag)
  int  lockflag;
{
  RF_AllocListElem_t *p;

#ifndef KERNEL
  if (lockflag) {  RF_LOCK_MUTEX(rf_debug_mem_mutex); }
#endif /* !KERNEL */
  if (al_free_list) {fl_hit_count++; p = al_free_list; al_free_list = p->next; al_free_list_count--;}
  else {
    fl_miss_count++;
#ifndef KERNEL
    p = (RF_AllocListElem_t *) malloc(sizeof(RF_AllocListElem_t));  /* can't use Malloc at user level b/c we already locked the mutex */
#else /* !KERNEL */
    RF_Malloc(p, sizeof(RF_AllocListElem_t), (RF_AllocListElem_t *));  /* no allocation locking in kernel, so this is fine */
#endif /* !KERNEL */
  }
#ifndef KERNEL
  if (lockflag) {  RF_UNLOCK_MUTEX(rf_debug_mem_mutex); }
#endif /* !KERNEL */
  if (p == NULL) {
    return(NULL);
  }
  bzero((char *)p, sizeof(RF_AllocListElem_t));
  return(p);
}
