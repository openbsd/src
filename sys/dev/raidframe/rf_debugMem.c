/*	$OpenBSD: rf_debugMem.c,v 1.1 1999/01/11 14:29:12 niklas Exp $	*/
/*	$NetBSD: rf_debugMem.c,v 1.1 1998/11/13 04:20:28 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Daniel Stodolsky, Mark Holland, Jim Zelenka
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

/* debugMem.c:  memory usage debugging stuff.
 * Malloc, Calloc, and Free are #defined everywhere 
 * to do_malloc, do_calloc, and do_free.
 *
 * if RF_UTILITY is nonzero, it means were compiling one of the
 * raidframe utility programs, such as rfctrl or smd.  In this
 * case, we eliminate all references to the threads package
 * and to the allocation list stuff.
 */

/* :  
 * Log: rf_debugMem.c,v 
 * Revision 1.38  1996/08/20 14:45:43  jimz
 * add debugging to track memory allocated (amount only, w/out
 * excessive sanity checking)
 *
 * Revision 1.37  1996/07/27  23:36:08  jimz
 * Solaris port of simulator
 *
 * Revision 1.36  1996/07/18  22:57:14  jimz
 * port simulator to AIX
 *
 * Revision 1.35  1996/06/13  08:55:38  jimz
 * make error messages refer to file, line of original
 * allocation
 *
 * Revision 1.34  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.33  1996/06/09  02:36:46  jimz
 * lots of little crufty cleanup- fixup whitespace
 * issues, comment #ifdefs, improve typing in some
 * places (esp size-related)
 *
 * Revision 1.32  1996/06/05  18:06:02  jimz
 * Major code cleanup. The Great Renaming is now done.
 * Better modularity. Better typing. Fixed a bunch of
 * synchronization bugs. Made a lot of global stuff
 * per-desc or per-array. Removed dead code.
 *
 * Revision 1.31  1996/05/30  23:22:16  jimz
 * bugfixes of serialization, timing problems
 * more cleanup
 *
 * Revision 1.30  1996/05/30  11:29:41  jimz
 * Numerous bug fixes. Stripe lock release code disagreed with the taking code
 * about when stripes should be locked (I made it consistent: no parity, no lock)
 * There was a lot of extra serialization of I/Os which I've removed- a lot of
 * it was to calculate values for the cache code, which is no longer with us.
 * More types, function, macro cleanup. Added code to properly quiesce the array
 * on shutdown. Made a lot of stuff array-specific which was (bogusly) general
 * before. Fixed memory allocation, freeing bugs.
 *
 * Revision 1.29  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.28  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.27  1996/05/23  00:33:23  jimz
 * code cleanup: move all debug decls to rf_options.c, all extern
 * debug decls to rf_options.h, all debug vars preceded by rf_
 *
 * Revision 1.26  1996/05/21  18:53:46  jimz
 * return NULL for failed allocations, not panic
 *
 * Revision 1.25  1996/05/20  16:14:19  jimz
 * switch to rf_{mutex,cond}_{init,destroy}
 *
 * Revision 1.24  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.23  1996/05/17  12:42:35  jimz
 * wrap get_threadid stuff in #ifndef UTILITY for utils which use
 * redzone allocation stuff
 *
 * Revision 1.22  1996/05/16  23:06:09  jimz
 * don't warn about NULL alists
 *
 * Revision 1.21  1996/05/16  22:25:02  jimz
 * show allocations for [MC]allocAndAdd
 *
 * Revision 1.20  1996/05/15  18:30:22  jimz
 * print memory allocation as well as frees if memDebug > 1
 *
 * Revision 1.19  1996/05/07  17:41:17  jimz
 * add "level 2" for memDebug, which will print freed address ranges
 *
 * Revision 1.18  1996/05/02  20:41:53  jimz
 * really fix malloc problem out-of-kernel in memory_hash_insert()
 *
 * Revision 1.17  1996/05/02  20:04:29  jimz
 * fixed malloc deadlock previous change introduced
 *
 * Revision 1.16  1996/05/01  16:27:26  jimz
 * get rid of ALLOCMH
 * stop using ccmn_ memory management
 *
 * Revision 1.15  1995/12/12  18:10:06  jimz
 * MIN -> RF_MIN, MAX -> RF_MAX, ASSERT -> RF_ASSERT
 * fix 80-column brain damage in comments
 *
 * Revision 1.14  1995/12/01  15:56:17  root
 * added copyright info
 *
 */

#include "rf_types.h"
#include "rf_sys.h"

#if RF_UTILITY == 0
#include "rf_threadstuff.h"
#include "rf_threadid.h"
#include "rf_options.h"
#else /* RF_UTILITY == 0 */
#include "rf_utility.h"
#endif /* RF_UTILITY == 0 */

#ifndef KERNEL
#include <stdio.h>
#include <assert.h>
#endif /* !KERNEL */
#include "rf_debugMem.h"
#include "rf_general.h"

static long tot_mem_in_use = 0, max_mem = 0;

/* Hash table of information about memory allocations */
#define RF_MH_TABLESIZE 1000

struct mh_struct {
    void              *address;
    int                size;
    int                line;
    char              *filen;
    char               allocated;
    struct mh_struct  *next;
};
static struct mh_struct *mh_table[RF_MH_TABLESIZE];
RF_DECLARE_MUTEX(rf_debug_mem_mutex)
static int mh_table_initialized=0;

static void memory_hash_insert(void *addr, int size, int line, char *filen);
static int memory_hash_remove(void *addr, int sz);

#ifndef KERNEL                    /* no redzones or "real_" routines in the kernel */

static void rf_redzone_free_failed(void *ptr, int size, int line, char *file);

void *rf_real_redzone_malloc(_size_)
  int  _size_;
{
  char *p;

  rf_validate_mh_table();
  p = malloc((_size_)+16);
  if (p == NULL)
    return(p);
  RF_ASSERT (p);
  *((long *) p) = (_size_) ;
  ((char *) p)[(_size_)+8] = '!';
  ((char *) p)[(_size_)+15] = '!';
  p += 8;
  return(p);
}

void *rf_real_redzone_calloc(_n_,_size_)
int _n_,_size_;
{
  char *p;
  int _sz_;
  
  rf_validate_mh_table();
  _sz_  = (_n_) * (_size_);
  p = malloc((_sz_)+16);
  if (p == NULL)
    return(p);
  bzero(p,(_sz_)+16);
  *((long *) p) = (_sz_) ;
  ((char *) p)[(_sz_)+8] = '!';
  ((char *) p)[(_sz_)+15] = '!';
  p += 8;
  return(p);
}

void rf_real_redzone_free(p, line, filen)
char *p;
int line;
char *filen;
{
  unsigned long _size_;
  
  rf_validate_mh_table();
  p -= 8;
  _size_ = *((long *) p);
  if  ((((char *) p)[(_size_)+8] != '!') || (((char *) p)[(_size_)+15] != '!'))
    rf_redzone_free_failed(p,(_size_),line,filen);
  free(p);
}

unsigned long rf_mem_alloc = 0;

char *rf_real_Malloc(size, line, file)
  int    size;
  int    line;
  char  *file;
{
  void *pp;
  char *p;
  int tid;

  RF_LOCK_MUTEX(rf_debug_mem_mutex);
  rf_redzone_malloc(pp, size);
  p = pp;
  if (p == NULL) {
    RF_ERRORMSG3("Unable to malloc %d bytes at line %d file %s\n", size,
      line, file);
  }
  if (rf_memAmtDebug) {
    rf_mem_alloc += size;
    printf("%lu    size %d %s:%d\n", rf_mem_alloc, size, file, line);
  }
#if RF_UTILITY == 0
  if (rf_memDebug > 1) {
    rf_get_threadid(tid);
    printf("[%d] malloc 0x%lx - 0x%lx (%d) %s %d\n", tid, p, p+size, size,
        file, line);
  }
#endif /* RF_UTILITY == 0 */
  if (rf_memDebug)
    rf_record_malloc(p, size, line, file);
  RF_UNLOCK_MUTEX(rf_debug_mem_mutex);
  return(p);
}

#if RF_UTILITY == 0
char *rf_real_MallocAndAdd(size, alist, line, file)
  int                  size;
  RF_AllocListElem_t  *alist;
  int                  line;
  char                *file;
{
	void *pp;
	char *p;
	int tid;

	RF_LOCK_MUTEX(rf_debug_mem_mutex);
	rf_redzone_malloc(pp, size);
	p = pp;
	if (p == NULL) {
		RF_ERRORMSG3("Unable to malloc %d bytes at line %d file %s\n", size,
			line, file);
	}
	if (rf_memAmtDebug) {
		rf_mem_alloc += size;
		printf("%lu    size %d %s:%d\n", rf_mem_alloc, size, file, line);
	}
	if (rf_memDebug > 1) {
		rf_get_threadid(tid);
		printf("[%d] malloc+add 0x%lx - 0x%lx (%d) %s %d\n", tid, p, p+size,
			size, file, line);
	}
	if (alist) {
		rf_real_AddToAllocList(alist, pp, size, 0);
	}
	if (rf_memDebug)
		rf_record_malloc(p, size, line, file);
	RF_UNLOCK_MUTEX(rf_debug_mem_mutex);
	return(p);
}
#endif /* RF_UTILITY == 0 */

char *rf_real_Calloc(nel, elsz, line, file)
  int    nel;
  int    elsz;
  int    line;
  char  *file;
{
  int tid, size;
  void *pp;
  char *p;
  
  size = nel * elsz;
  RF_LOCK_MUTEX(rf_debug_mem_mutex);
  rf_redzone_calloc(pp, nel, elsz);
  p = pp;
  if (p == NULL) {
    RF_ERRORMSG4("Unable to calloc %d objects of size %d at line %d file %s\n",
      nel, elsz, line, file);
    return(NULL);
  }
  if (rf_memAmtDebug) {
    rf_mem_alloc += size;
    printf("%lu    size %d %s:%d\n", rf_mem_alloc, size, file, line);
  }
#if RF_UTILITY == 0
  if (rf_memDebug > 1) {
    rf_get_threadid(tid);
    printf("[%d] calloc 0x%lx - 0x%lx (%d,%d) %s %d\n", tid, p, p+size, nel,
        elsz, file, line);
  }
#endif /* RF_UTILITY == 0 */
  if (rf_memDebug) {
    rf_record_malloc(p, size, line, file);
  }
  RF_UNLOCK_MUTEX(rf_debug_mem_mutex);
  return(p);
}

#if RF_UTILITY == 0
char *rf_real_CallocAndAdd(nel, elsz, alist, line, file)
  int                  nel;
  int                  elsz;
  RF_AllocListElem_t  *alist;
  int                  line;
  char                *file;
{
	int tid, size;
	void *pp;
	char *p;

	size = nel * elsz;
	RF_LOCK_MUTEX(rf_debug_mem_mutex);
	rf_redzone_calloc(pp, nel, elsz);
	p = pp;
	if (p == NULL) {
		RF_ERRORMSG4("Unable to calloc %d objs of size %d at line %d file %s\n",
			nel, elsz, line, file);
		return(NULL);
	}
	if (rf_memAmtDebug) {
		rf_mem_alloc += size;
		printf("%lu    size %d %s:%d\n", rf_mem_alloc, size, file, line);
	}
	if (rf_memDebug > 1) {
		rf_get_threadid(tid);
		printf("[%d] calloc+add 0x%lx - 0x%lx (%d,%d) %s %d\n", tid, p,
			p+size, nel, elsz, file, line);
	}
	if (alist) {
		rf_real_AddToAllocList(alist, pp, size, 0);
	}
	if (rf_memDebug)
		rf_record_malloc(p, size, line, file);
	RF_UNLOCK_MUTEX(rf_debug_mem_mutex);
	return(p);
}
#endif /* RF_UTILITY == 0 */

void rf_real_Free(p, sz, line, file)
  void  *p;
  int    sz;
  int    line;
  char  *file;
{
  int tid;

#if RF_UTILITY == 0
  if (rf_memDebug > 1) {
    rf_get_threadid(tid);
    printf("[%d] free 0x%lx - 0x%lx (%d) %s %d\n", tid, p, ((char *)p)+sz, sz,
      file, line);
  }
#endif /* RF_UTILITY == 0 */
  RF_LOCK_MUTEX(rf_debug_mem_mutex);
  if (rf_memAmtDebug) {
    rf_mem_alloc -= sz;
    printf("%lu  - size %d %s:%d\n", rf_mem_alloc, sz, file, line);
  }
  if (rf_memDebug) {
    rf_unrecord_malloc(p,sz);
  }
  rf_redzone_free(p);
  RF_UNLOCK_MUTEX(rf_debug_mem_mutex);
}

void rf_validate_mh_table()
{
  int i, size;
  struct mh_struct *p;
  char *cp;

  return;
  for (i=0; i<RF_MH_TABLESIZE; i++) {
    for (p=mh_table[i]; p; p=p->next) if (p->allocated) {
      cp = ((char *) p->address) - 8;
      size = *((long *) cp);
      if  ((((char *) cp)[(size)+8] != '!') || (((char *) cp)[(size)+15] != '!')) {
	rf_redzone_free_failed(cp,(size),__LINE__,__FILE__);
      }
    }
  }
}

static void rf_redzone_free_failed(ptr,size,line,file)
  void  *ptr;
  int    size;
  int    line;
  char  *file;
{
  RF_ERRORMSG4("Free of 0x%lx (recorded size %d) at %d of %s detected redzone overrun\n",ptr,size,line,file);
  RF_ASSERT(0);
}

#endif /* !KERNEL */

void rf_record_malloc(p, size, line, filen)
void *p;
int size, line;
char *filen;
{
  RF_ASSERT(size != 0);
  
  /*RF_LOCK_MUTEX(rf_debug_mem_mutex);*/
  memory_hash_insert(p, size, line, filen);
  tot_mem_in_use += size;
  /*RF_UNLOCK_MUTEX(rf_debug_mem_mutex);*/
  if ( (long) p == rf_memDebugAddress) {
    printf("Allocate: debug address allocated from line %d file %s\n",line,filen); 
  }
}

void rf_unrecord_malloc(p, sz)
void *p;
int sz;
{
  int size;
  
  /*RF_LOCK_MUTEX(rf_debug_mem_mutex);*/
  size = memory_hash_remove(p, sz);
  tot_mem_in_use -= size;
  /*RF_UNLOCK_MUTEX(rf_debug_mem_mutex);*/
  if ( (long) p == rf_memDebugAddress) {
    printf("Free: Found debug address\n");         /* this is really only a flag line for gdb */
  }
}

void rf_print_unfreed()
{
  int i, foundone=0;
  struct mh_struct *p;

  for (i=0; i<RF_MH_TABLESIZE; i++) {
    for (p=mh_table[i]; p; p=p->next) if (p->allocated) {
      if (!foundone) printf("\n\nThere are unfreed memory locations at program shutdown:\n");
      foundone = 1;
      printf("Addr 0x%lx Size %d line %d file %s\n",
	     (long)p->address,p->size,p->line,p->filen);
    }
  }
  if (tot_mem_in_use) {
    printf("%ld total bytes in use\n", tot_mem_in_use);
  }
}

int rf_ConfigureDebugMem(listp)
  RF_ShutdownList_t  **listp;
{
  int i, rc;

  rc = rf_create_managed_mutex(listp, &rf_debug_mem_mutex);
  if (rc) {
    RF_ERRORMSG3("Unable to init mutex file %s line %d rc=%d\n", __FILE__,
      __LINE__, rc);
    return(rc);
  }
  if (rf_memDebug) {
    for (i=0; i<RF_MH_TABLESIZE; i++)
      mh_table[i] = NULL;
    mh_table_initialized=1;
  }
  return(0);
}

#define HASHADDR(_a_)      ( (((unsigned long) _a_)>>3) % RF_MH_TABLESIZE )

static void memory_hash_insert(addr, size, line, filen)
void *addr;
int size, line;
char *filen;
{
    unsigned long bucket = HASHADDR(addr);
    struct mh_struct *p;
    
    RF_ASSERT(mh_table_initialized);

    /* search for this address in the hash table */
    for (p=mh_table[bucket]; p && (p->address != addr); p=p->next);
    if (!p) {
#ifdef KERNEL
      RF_Malloc(p,sizeof(struct mh_struct),(struct mh_struct *));
#else /* KERNEL */
      p = (struct mh_struct *)malloc(sizeof(struct mh_struct));
#endif /* KERNEL */
      RF_ASSERT(p);
      p->next = mh_table[bucket];
      mh_table[bucket] = p;
      p->address = addr;
      p->allocated = 0;
    }
    if (p->allocated) {
      printf("ERROR:  reallocated address 0x%lx from line %d, file %s without intervening free\n",(long) addr, line, filen);
      printf("        last allocated from line %d file %s\n",p->line, p->filen);
      RF_ASSERT(0);
    }
    p->size = size; p->line = line; p->filen = filen;
    p->allocated = 1;
}

static int memory_hash_remove(addr, sz)
void *addr;
int sz;
{
    unsigned long bucket = HASHADDR(addr);
    struct mh_struct *p;
    
    RF_ASSERT(mh_table_initialized);
    for (p=mh_table[bucket]; p && (p->address != addr); p=p->next);
    if (!p) {
      printf("ERROR:  freeing never-allocated address 0x%lx\n",(long) addr);
      RF_PANIC();
    }
    if (!p->allocated) {
      printf("ERROR:  freeing unallocated address 0x%lx.  Last allocation line %d file %s\n",(long) addr, p->line, p->filen);
      RF_PANIC();
    }
    if (sz > 0 && p->size != sz) {         /* you can suppress this error by using a negative value as the size to free */
      printf("ERROR:  incorrect size at free for address 0x%lx: is %d should be %d.  Alloc at line %d of file %s\n",(unsigned long) addr, sz, p->size,p->line, p->filen);
      RF_PANIC();
    }
    p->allocated = 0;
    return(p->size);
}

void rf_ReportMaxMem()
{
    printf("Max memory used:  %d bytes\n",(int)max_mem);
#ifndef KERNEL
    fflush(stdout);
    fprintf(stderr,"Max memory used:  %d bytes\n",max_mem);
    fflush(stderr);
#endif /* !KERNEL */
}
