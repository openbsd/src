/*	$OpenBSD: rf_memchunk.c,v 1.1 1999/01/11 14:29:30 niklas Exp $	*/
/*	$NetBSD: rf_memchunk.c,v 1.1 1998/11/13 04:20:31 oster Exp $	*/
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

/*********************************************************************************
 * rf_memchunk.c
 *
 * experimental code.  I've found that the malloc and free calls in the DAG
 * creation code are very expensive.  Since for any given workload the DAGs
 * created for different accesses are likely to be similar to each other, the
 * amount of memory used for any given DAG data structure is likely to be one
 * of a small number of values.  For example, in UNIX, all reads and writes will
 * be less than 8k and will not span stripe unit boundaries.  Thus in the absence
 * of failure, the only DAGs that will ever get created are single-node reads
 * and single-stripe-unit atomic read-modify-writes.  So, I'm very likely to
 * be continually asking for chunks of memory equal to the sizes of these two
 * DAGs.
 *
 * This leads to the idea of holding on to these chunks of memory when the DAG is
 * freed and then, when a new DAG is created, trying to find such a chunk before
 * calling malloc.
 *
 * the "chunk list" is a list of lists.  Each header node contains a size value
 * and a pointer to a list of chunk descriptors, each of which holds a pointer
 * to a chunk of memory of the indicated size.
 *
 * There is currently no way to purge memory out of the chunk list.  My
 * initial thought on this is to have a low-priority thread that wakes up every
 * 1 or 2 seconds, purges all the chunks with low reuse counts, and sets all
 * the reuse counts to zero.
 *
 * This whole idea may be bad, since malloc may be able to do this more efficiently.
 * It's worth a try, though, and it can be turned off by setting useMemChunks to 0.
 *
 ********************************************************************************/

/* :  
 * Log: rf_memchunk.c,v 
 * Revision 1.17  1996/07/27 23:36:08  jimz
 * Solaris port of simulator
 *
 * Revision 1.16  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.15  1996/06/09  02:36:46  jimz
 * lots of little crufty cleanup- fixup whitespace
 * issues, comment #ifdefs, improve typing in some
 * places (esp size-related)
 *
 * Revision 1.14  1996/06/05  18:06:02  jimz
 * Major code cleanup. The Great Renaming is now done.
 * Better modularity. Better typing. Fixed a bunch of
 * synchronization bugs. Made a lot of global stuff
 * per-desc or per-array. Removed dead code.
 *
 * Revision 1.13  1996/06/02  17:31:48  jimz
 * Moved a lot of global stuff into array structure, where it belongs.
 * Fixed up paritylogging, pss modules in this manner. Some general
 * code cleanup. Removed lots of dead code, some dead files.
 *
 * Revision 1.12  1996/05/30  23:22:16  jimz
 * bugfixes of serialization, timing problems
 * more cleanup
 *
 * Revision 1.11  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.10  1996/05/23  00:33:23  jimz
 * code cleanup: move all debug decls to rf_options.c, all extern
 * debug decls to rf_options.h, all debug vars preceded by rf_
 *
 * Revision 1.9  1996/05/20  16:15:45  jimz
 * switch to rf_{mutex,cond}_{init,destroy}
 *
 * Revision 1.8  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.7  1995/12/01  19:26:07  root
 * added copyright info
 *
 */

#include "rf_types.h"
#include "rf_threadstuff.h"
#include "rf_debugMem.h"
#include "rf_memchunk.h"
#include "rf_general.h"
#include "rf_options.h"
#include "rf_shutdown.h"
#include "rf_sys.h"

typedef struct RF_ChunkHdr_s RF_ChunkHdr_t;
struct RF_ChunkHdr_s {
  int              size;
  RF_ChunkDesc_t  *list;
  RF_ChunkHdr_t   *next;
};

static RF_ChunkHdr_t *chunklist, *chunk_hdr_free_list;
static RF_ChunkDesc_t *chunk_desc_free_list;
RF_DECLARE_STATIC_MUTEX(chunkmutex)

static void rf_ShutdownMemChunk(void *);
static RF_ChunkDesc_t *NewMemChunk(int, char *);


static void rf_ShutdownMemChunk(ignored)
  void *ignored;
{
  RF_ChunkDesc_t *pt, *p;
  RF_ChunkHdr_t *hdr, *ht;

  if (rf_memChunkDebug)
    printf("Chunklist:\n");
  for (hdr = chunklist; hdr;) {
    for (p = hdr->list; p; ) {
      if (rf_memChunkDebug)
        printf("Size %d reuse count %d\n",p->size, p->reuse_count);
      pt = p; p=p->next;
      RF_Free(pt->buf, pt->size);
      RF_Free(pt, sizeof(*pt));
    }
    ht = hdr; hdr=hdr->next;
    RF_Free(ht, sizeof(*ht));
  }

  rf_mutex_destroy(&chunkmutex);
}

int rf_ConfigureMemChunk(listp)
  RF_ShutdownList_t  **listp;
{
  int rc;

  chunklist = NULL;
  chunk_hdr_free_list = NULL;
  chunk_desc_free_list = NULL;
  rc = rf_mutex_init(&chunkmutex);
  if (rc) {
    RF_ERRORMSG3("Unable to init mutex file %s line %d rc=%d\n", __FILE__,
      __LINE__, rc);
  }
  rc = rf_ShutdownCreate(listp, rf_ShutdownMemChunk, NULL);
  if (rc) {
    RF_ERRORMSG3("Unable to add to shutdown list file %s line %d rc=%d\n", __FILE__,
      __LINE__, rc);
    rf_mutex_destroy(&chunkmutex);
  }
  return(rc);
}

/* called to get a chunk descriptor for a newly-allocated chunk of memory
 * MUTEX MUST BE LOCKED
 *
 * free list is not currently used
 */
static RF_ChunkDesc_t *NewMemChunk(size, buf)
  int    size;
  char  *buf;
{
  RF_ChunkDesc_t *p;
  
  if (chunk_desc_free_list) {p = chunk_desc_free_list; chunk_desc_free_list = p->next;}
  else RF_Malloc(p, sizeof(RF_ChunkDesc_t), (RF_ChunkDesc_t *));
  p->size =  size;
  p->buf  = buf;
  p->next =  NULL;
  p->reuse_count = 0;
  return(p);
}

/* looks for a chunk of memory of acceptable size.  If none, allocates one and returns
 * a chunk descriptor for it, but does not install anything in the list.  This is done
 * when the chunk is released.
 */
RF_ChunkDesc_t *rf_GetMemChunk(size)
  int  size;
{
  RF_ChunkHdr_t *hdr = chunklist;
  RF_ChunkDesc_t *p = NULL;
  char *buf;

  RF_LOCK_MUTEX(chunkmutex);
  for (hdr = chunklist; hdr; hdr = hdr->next) if (hdr->size >= size) {
    p = hdr->list;
    if (p) {
      hdr->list = p->next;
      p->next = NULL;
      p->reuse_count++;
    }
    break;
  }
  if (!p) {
    RF_Malloc(buf, size, (char *));
    p = NewMemChunk(size, buf);
  }
  RF_UNLOCK_MUTEX(chunkmutex);
  (void) bzero(p->buf, size);
  return(p);
}

void rf_ReleaseMemChunk(chunk)
  RF_ChunkDesc_t  *chunk;
{
  RF_ChunkHdr_t *hdr, *ht = NULL, *new;
  
  RF_LOCK_MUTEX(chunkmutex);
  for (hdr = chunklist; hdr && hdr->size < chunk->size; ht=hdr,hdr=hdr->next);
  if (hdr && hdr->size == chunk->size) {
    chunk->next = hdr->list;
    hdr->list = chunk;
  }
  else {
    RF_Malloc(new, sizeof(RF_ChunkHdr_t), (RF_ChunkHdr_t *));
    new->size = chunk->size; new->list = chunk; chunk->next = NULL;
    if (ht) {
      new->next = ht->next;
      ht->next = new;
    }
    else {
      new->next = hdr;
      chunklist = new;
    }
  }
  RF_UNLOCK_MUTEX(chunkmutex);
}
