/*	$OpenBSD: rf_heap.h,v 1.1 1999/01/11 14:29:25 niklas Exp $	*/
/*	$NetBSD: rf_heap.h,v 1.1 1998/11/13 04:20:30 oster Exp $	*/
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

/* @TITLE "heap.h - interface to heap management implementation */
/* We manage a heap of data,key pairs, where the key could be any 
 * simple data type 
 * and the data is any pointer data type. We allow the caller to add
 * pairs, remote pairs, peek at the top pair, and do delete/add combinations.
 * The latter are efficient because we only reheap once.
 *
 * David Kotz 1990? and 1993
 */

/* :  
 * Log: rf_heap.h,v 
 * Revision 1.8  1996/05/30 11:29:41  jimz
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
 * Revision 1.5  1995/12/01  19:04:07  root
 * added copyright info
 *
 */

#ifndef _RF__RF_HEAP_H_
#define _RF__RF_HEAP_H_

#include "rf_types.h"
#include "rf_raid.h"
#include "rf_dag.h"
#include "rf_desc.h"

#define RF_HEAP_MAX 10240

#define RF_HEAP_FOUND 1
#define RF_HEAP_NONE  0

typedef RF_TICS_t RF_HeapKey_t;

typedef struct RF_HeapData_s   RF_HeapData_t;
typedef struct RF_Heap_s      *RF_Heap_t;
typedef struct RF_HeapEntry_s  RF_HeapEntry_t;

/* heap data */
struct RF_HeapData_s {
  RF_TICS_t    eventTime;
  int          disk;
  int        (*CompleteFunc)(); /* function to be called upon completion */
  void        *argument;        /* argument to be passed to CompleteFunc */
  int          owner;           /* which task is resposable for this request */
  int          row;
  int          col;             /* coordinates of disk */
  RF_Raid_t   *raidPtr;
  void        *diskid;
  /* Dag event */
  RF_RaidAccessDesc_t  *desc;  
};

struct RF_HeapEntry_s {
  RF_HeapData_t  *data; /* the arbitrary data */
  RF_HeapKey_t    key;  /* key for comparison */
};

struct RF_Heap_s {
  RF_HeapEntry_t  *heap;    /* the heap in use (an array) */
  int              numheap; /* number of elements in heap */
  int              maxsize;
};

/* set up heap to hold maxsize nodes */
RF_Heap_t rf_InitHeap(int maxsize);

/* delete a heap data structure */
void rf_FreeHeap(RF_Heap_t hp);

/* add the element to the heap */
void rf_AddHeap(RF_Heap_t hp, RF_HeapData_t *data, RF_HeapKey_t key); 

/* return top of the heap, without removing it from heap (FALSE if empty) */
int rf_TopHeap(RF_Heap_t hp, RF_HeapData_t **data, RF_HeapKey_t *key);

/* replace the heap's top item with a new item, and reheap */
void rf_RepHeap(RF_Heap_t hp, RF_HeapData_t *data, RF_HeapKey_t key);

/* remove the heap's top item, if any (FALSE if empty heap) */
int rf_RemHeap(RF_Heap_t hp, RF_HeapData_t **data, RF_HeapKey_t *key);

#endif /* !_RF__RF_HEAP_H_ */
