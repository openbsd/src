/*	$OpenBSD: rf_heap.c,v 1.1 1999/01/11 14:29:25 niklas Exp $	*/
/*	$NetBSD: rf_heap.c,v 1.1 1998/11/13 04:20:30 oster Exp $	*/
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

/* We manage a heap of data,key pairs, where the key a simple data type
 * and the data is any singular data type. We allow the caller to add
 * pairs, remote pairs, peek at the top pair, and do delete/add combinations.
 * The latter are efficient because we only reheap once.
 *
 * David Kotz 1990? and 1993
 *
 * Modify the heap to work with events, with the smallest time  on the top.
 * Song Bac Toh, 1994
 */

/* :  
 * Log: rf_heap.c,v 
 * Revision 1.8  1996/07/28 20:31:39  jimz
 * i386netbsd port
 * true/false fixup
 *
 * Revision 1.7  1996/06/09  02:36:46  jimz
 * lots of little crufty cleanup- fixup whitespace
 * issues, comment #ifdefs, improve typing in some
 * places (esp size-related)
 *
 * Revision 1.6  1996/05/30  11:29:41  jimz
 * Numerous bug fixes. Stripe lock release code disagreed with the taking code
 * about when stripes should be locked (I made it consistent: no parity, no lock)
 * There was a lot of extra serialization of I/Os which I've removed- a lot of
 * it was to calculate values for the cache code, which is no longer with us.
 * More types, function, macro cleanup. Added code to properly quiesce the array
 * on shutdown. Made a lot of stuff array-specific which was (bogusly) general
 * before. Fixed memory allocation, freeing bugs.
 *
 * Revision 1.5  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.4  1995/12/01  19:03:58  root
 * added copyright info
 *
 */

#include "rf_types.h"
#include "rf_heap.h"
#include "rf_general.h"

/* return RF_TRUE if the two requests in the heap match */
#define Matching_REQUESTS(HeapData1, HeapData2)       \
((HeapData1->disk == HeapData2->disk) &&              \
 (HeapData1->req_code == HeapData2->req_code))

/* getting around in the heap */
/* we don't use the 0th element of the array */
#define ROOT 1
#define LCHILD(p) (2 * (p))
#define RCHILD(p) (2 * (p) + 1)
#define PARENT(c) ((c) / 2)

/* @SUBTITLE "Debugging macros" */
/* The following are used for debugging our callers 
 * as well as internal stuff 
 */

#define CHECK_INVARIANTS 1

#ifdef CHECK_INVARIANTS
#define INVARIANT2(x, y) \
{ \
    if (!(x)) { \
		  fprintf(stderr, "INVARIANT false: in \"%s\", line %d\n", \
	       __FILE__, __LINE__); \
		  fprintf(stderr, (y)); \
		  exit(1); \
	      } \
}

/*
#define INVARIANT3(x, y, z) \
  { \
    if (!(x)) { \
       fprintf(stderr, "INVARIANT false: in \"%s\", line %d\n", \
	       __FILE__, __LINE__); \
       fprintf(stderr, (y), (z)); \
       exit(1); \
    } \
  }
  */
#else /* CHECK_INVARIANTS */
/* #define INVARIANT2(x, y) */
/* #define INVARIANT3(x, y, z) already defined in modularize.h */
#endif /* CHECK_INVARIANTS */

/**** Rachad, must add to general debug structure */


/* @SUBTITLE "InitHeap: Allocate a new heap" */
/* might return NULL if no free memory */
RF_Heap_t rf_InitHeap(int maxsize)
{
  RF_Heap_t hp;

  RF_ASSERT(maxsize > 0);
  RF_Malloc(hp, sizeof(struct RF_Heap_s),(RF_Heap_t));
  if (hp == NULL) {
    fprintf(stderr, "InitHeap: No memory for heap\n");
    return(NULL);
  }

  RF_Malloc(hp->heap,sizeof(RF_HeapEntry_t)*(maxsize+1),(RF_HeapEntry_t *));
  if (hp->heap == NULL) {
    fprintf(stderr, "InitHeap: No memory for heap of %d elements\n", 
	    maxsize);
    RF_Free(hp,-1);     /* -1 means don't cause an error if the size does not match */
	return(NULL);
  }

  hp->numheap = 0;
  hp->maxsize = maxsize;

  return(hp);
}

/* @SUBTITLE "FreeHeap: delete a heap" */
void rf_FreeHeap(RF_Heap_t hp)
{
	if (hp != NULL) {
		RF_Free(hp->heap,sizeof(RF_HeapEntry_t)*(hp->maxsize+1));
		RF_Free(hp,sizeof(struct RF_Heap_s));
	}
}

/* @SUBTITLE "AddHeap: Add an element to the heap" */
void rf_AddHeap(RF_Heap_t hp, RF_HeapData_t *data, RF_HeapKey_t key)
{
    int node;

    INVARIANT2(hp != NULL, "AddHeap: NULL heap\n");
    INVARIANT2((hp->numheap < RF_HEAP_MAX), "AddHeap: Heap overflowed\n");

    /* use new space end of heap */
    node = ++(hp->numheap);

    /* and reheap */
    while (node != ROOT && hp->heap[PARENT(node)].key > key) {
      hp->heap[node] = hp->heap[PARENT(node)];
      node = PARENT(node);
    }

    hp->heap[node].data = data;
    hp->heap[node].key = key;
}

/* @SUBTITLE "TopHeap: Return top element of heap" */
int rf_TopHeap(RF_Heap_t hp, RF_HeapData_t **data, RF_HeapKey_t *key)
{
  INVARIANT2(hp != NULL, "TopHeap: NULL heap\n");

  if (hp->numheap > 0) {
    if (data)
      *data = hp->heap[ROOT].data;
    if (key)
      *key = hp->heap[ROOT].key;
    return(RF_HEAP_FOUND);
  }
  else {
    return(RF_HEAP_NONE);
  }
}

/* @SUBTITLE "RepHeap: Replace top of heap with given element and reheap" */
/* note that hp->numheap does not change, and should already be > 0 */
void rf_RepHeap(RF_Heap_t hp, RF_HeapData_t *data, RF_HeapKey_t key)
{
    int node;             /* node in heap */
    int lchild, rchild;   /* left and right children of node */
    int left, right;      /* left and right children exist? */
    int swapped;          /* swap was made? */
    RF_HeapEntry_t *heap; /* pointer to the base of this heap array */

    INVARIANT2(hp != NULL, "RepHeap: NULL heap\n");

    /* If heap is empty just add this element */
    /* if used properly this case should never come up */
    if (hp->numheap == 0) {
	rf_AddHeap(hp, data, key);

	return;
    }

    heap = hp->heap;	      /* cache the heap base pointer */

    node = ROOT;

    do {
	lchild = LCHILD(node);
	rchild = RCHILD(node);
	left = (lchild <= hp->numheap);
	right = (rchild <= hp->numheap);

	/* Both children exist: which is smaller? */
	if (left && right)
	  if (heap[lchild].key < heap[rchild].key)
	    right = RF_HEAP_NONE;
	  else
	    left = RF_HEAP_NONE;

	/* Now only one of left and right is true. compare it with us */
	if (left && heap[lchild].key < key) {
	    /* swap with left child */
	    heap[node] = heap[lchild];
	    node = lchild;
	    swapped = RF_HEAP_FOUND;
	} else if (right && heap[rchild].key < key) {
	    /* swap with right child */
	    heap[node] = heap[rchild];
	    node = rchild;
	    swapped = RF_HEAP_FOUND;
	} else
	  swapped = RF_HEAP_NONE;
    } while (swapped);

    /* final resting place for new element */
    heap[node].key = key;
    heap[node].data = data;
}

/* @SUBTITLE "RemHeap: Remove top element and reheap" */
int rf_RemHeap(RF_Heap_t hp, RF_HeapData_t **data, RF_HeapKey_t *key)
{
    int node;

    /* we don't check hp's validity because TopHeap will do it for us */

    /* get the top element into data and key, if any */
    if (rf_TopHeap(hp, data, key)) {
	/* there was something there, so replace top with last element */
	node = hp->numheap--;
	if (hp->numheap > 0)
	  rf_RepHeap(hp, hp->heap[node].data, hp->heap[node].key);

	return(RF_HEAP_FOUND);
    } else{
      return(RF_HEAP_NONE);
    }
}

