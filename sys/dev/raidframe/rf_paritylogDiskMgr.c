/*	$OpenBSD: rf_paritylogDiskMgr.c,v 1.1 1999/01/11 14:29:34 niklas Exp $	*/
/*	$NetBSD: rf_paritylogDiskMgr.c,v 1.1 1998/11/13 04:20:31 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: William V. Courtright II
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
/* Code for flushing and reintegration operations related to parity logging.
 *
 * :  
 * Log: rf_paritylogDiskMgr.c,v 
 * Revision 1.25  1996/07/28 20:31:39  jimz
 * i386netbsd port
 * true/false fixup
 *
 * Revision 1.24  1996/07/27  23:36:08  jimz
 * Solaris port of simulator
 *
 * Revision 1.23  1996/07/22  19:52:16  jimz
 * switched node params to RF_DagParam_t, a union of
 * a 64-bit int and a void *, for better portability
 * attempted hpux port, but failed partway through for
 * lack of a single C compiler capable of compiling all
 * source files
 *
 * Revision 1.22  1996/06/11  10:17:33  jimz
 * Put in thread startup/shutdown mechanism for proper synchronization
 * with start and end of day routines.
 *
 * Revision 1.21  1996/06/09  02:36:46  jimz
 * lots of little crufty cleanup- fixup whitespace
 * issues, comment #ifdefs, improve typing in some
 * places (esp size-related)
 *
 * Revision 1.20  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.19  1996/06/05  18:06:02  jimz
 * Major code cleanup. The Great Renaming is now done.
 * Better modularity. Better typing. Fixed a bunch of
 * synchronization bugs. Made a lot of global stuff
 * per-desc or per-array. Removed dead code.
 *
 * Revision 1.18  1996/06/02  17:31:48  jimz
 * Moved a lot of global stuff into array structure, where it belongs.
 * Fixed up paritylogging, pss modules in this manner. Some general
 * code cleanup. Removed lots of dead code, some dead files.
 *
 * Revision 1.17  1996/05/31  22:26:54  jimz
 * fix a lot of mapping problems, memory allocation problems
 * found some weird lock issues, fixed 'em
 * more code cleanup
 *
 * Revision 1.16  1996/05/30  23:22:16  jimz
 * bugfixes of serialization, timing problems
 * more cleanup
 *
 * Revision 1.15  1996/05/30  12:59:18  jimz
 * make etimer happier, more portable
 *
 * Revision 1.14  1996/05/30  11:29:41  jimz
 * Numerous bug fixes. Stripe lock release code disagreed with the taking code
 * about when stripes should be locked (I made it consistent: no parity, no lock)
 * There was a lot of extra serialization of I/Os which I've removed- a lot of
 * it was to calculate values for the cache code, which is no longer with us.
 * More types, function, macro cleanup. Added code to properly quiesce the array
 * on shutdown. Made a lot of stuff array-specific which was (bogusly) general
 * before. Fixed memory allocation, freeing bugs.
 *
 * Revision 1.13  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.12  1996/05/24  22:17:04  jimz
 * continue code + namespace cleanup
 * typed a bunch of flags
 *
 * Revision 1.11  1996/05/24  04:28:55  jimz
 * release cleanup ckpt
 *
 * Revision 1.10  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.9  1996/05/23  00:33:23  jimz
 * code cleanup: move all debug decls to rf_options.c, all extern
 * debug decls to rf_options.h, all debug vars preceded by rf_
 *
 * Revision 1.8  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.7  1995/12/12  18:10:06  jimz
 * MIN -> RF_MIN, MAX -> RF_MAX, ASSERT -> RF_ASSERT
 * fix 80-column brain damage in comments
 *
 * Revision 1.6  1995/12/06  20:58:27  wvcii
 * added prototypes
 *
 * Revision 1.5  1995/11/30  16:06:05  wvcii
 * added copyright info
 *
 * Revision 1.4  1995/10/09  22:41:10  wvcii
 * minor bug fix
 *
 * Revision 1.3  1995/10/08  20:43:47  wvcii
 * lots of random debugging - debugging still incomplete
 *
 * Revision 1.2  1995/09/07  15:52:19  jimz
 * noop compile when INCLUDE_PARITYLOGGING not defined
 *
 * Revision 1.1  1995/09/06  19:24:44  wvcii
 * Initial revision
 *
 */

#include "rf_archs.h"

#if RF_INCLUDE_PARITYLOGGING > 0

#include "rf_types.h"
#include "rf_threadstuff.h"
#include "rf_mcpair.h"
#include "rf_raid.h"
#include "rf_dag.h"
#include "rf_dagfuncs.h"
#include "rf_desc.h"
#include "rf_layout.h"
#include "rf_diskqueue.h"
#include "rf_paritylog.h"
#include "rf_general.h"
#include "rf_threadid.h"
#include "rf_etimer.h"
#include "rf_paritylogging.h"
#include "rf_engine.h"
#include "rf_dagutils.h"
#include "rf_map.h"
#include "rf_parityscan.h"
#include "rf_sys.h"

#include "rf_paritylogDiskMgr.h"

static caddr_t AcquireReintBuffer(RF_RegionBufferQueue_t *);

static caddr_t AcquireReintBuffer(pool)
  RF_RegionBufferQueue_t  *pool;
{
  caddr_t bufPtr = NULL;

  /* Return a region buffer from the free list (pool).
     If the free list is empty, WAIT.
     BLOCKING */

  RF_LOCK_MUTEX(pool->mutex);
  if (pool->availableBuffers > 0) {
    bufPtr = pool->buffers[pool->availBuffersIndex];
    pool->availableBuffers--;
    pool->availBuffersIndex++;
    if (pool->availBuffersIndex == pool->totalBuffers)
      pool->availBuffersIndex = 0;
    RF_UNLOCK_MUTEX(pool->mutex);
  }
  else {
    RF_PANIC(); /* should never happen in currect config, single reint */
    RF_WAIT_COND(pool->cond, pool->mutex);
  }
  return(bufPtr);
}

static void ReleaseReintBuffer(
  RF_RegionBufferQueue_t  *pool,
  caddr_t                  bufPtr)
{
  /* Insert a region buffer (bufPtr) into the free list (pool).
     NON-BLOCKING */

  RF_LOCK_MUTEX(pool->mutex);
  pool->availableBuffers++;
  pool->buffers[pool->emptyBuffersIndex] = bufPtr;
  pool->emptyBuffersIndex++;
  if (pool->emptyBuffersIndex == pool->totalBuffers)
    pool->emptyBuffersIndex = 0;
  RF_ASSERT(pool->availableBuffers <= pool->totalBuffers);
  RF_UNLOCK_MUTEX(pool->mutex);
  RF_SIGNAL_COND(pool->cond);
}



static void ReadRegionLog(
  RF_RegionId_t         regionID,
  RF_MCPair_t          *rrd_mcpair,
  caddr_t               regionBuffer,
  RF_Raid_t            *raidPtr,
  RF_DagHeader_t      **rrd_dag_h,
  RF_AllocListElem_t  **rrd_alloclist,
  RF_PhysDiskAddr_t   **rrd_pda)
{
  /* Initiate the read a region log from disk.  Once initiated, return
     to the calling routine.

     NON-BLOCKING
   */

  RF_AccTraceEntry_t tracerec;
  RF_DagNode_t *rrd_rdNode;

  /* create DAG to read region log from disk */
  rf_MakeAllocList(*rrd_alloclist);
  *rrd_dag_h = rf_MakeSimpleDAG(raidPtr, 1, 0, regionBuffer, rf_DiskReadFunc, rf_DiskReadUndoFunc,
			     "Rrl", *rrd_alloclist, RF_DAG_FLAGS_NONE, RF_IO_NORMAL_PRIORITY);

  /* create and initialize PDA for the core log */
  /* RF_Malloc(*rrd_pda, sizeof(RF_PhysDiskAddr_t), (RF_PhysDiskAddr_t *)); */
  *rrd_pda = rf_AllocPDAList(1);
  rf_MapLogParityLogging(raidPtr, regionID, 0, &((*rrd_pda)->row), &((*rrd_pda)->col), &((*rrd_pda)->startSector));
  (*rrd_pda)->numSector = raidPtr->regionInfo[regionID].capacity;

  if ((*rrd_pda)->next) {
    (*rrd_pda)->next = NULL;
    printf("set rrd_pda->next to NULL\n");
  }

  /* initialize DAG parameters */
  bzero((char *)&tracerec,sizeof(tracerec));
  (*rrd_dag_h)->tracerec = &tracerec;
  rrd_rdNode = (*rrd_dag_h)->succedents[0]->succedents[0];
  rrd_rdNode->params[0].p = *rrd_pda;
/*  rrd_rdNode->params[1] = regionBuffer; */
  rrd_rdNode->params[2].v = 0;
  rrd_rdNode->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, 0);

  /* launch region log read dag */
  rf_DispatchDAG(*rrd_dag_h, (void (*)(void *))rf_MCPairWakeupFunc, 
		 (void *) rrd_mcpair);
}



static void WriteCoreLog(
  RF_ParityLog_t       *log,
  RF_MCPair_t          *fwr_mcpair,
  RF_Raid_t            *raidPtr,
  RF_DagHeader_t      **fwr_dag_h,
  RF_AllocListElem_t  **fwr_alloclist,
  RF_PhysDiskAddr_t   **fwr_pda)
{
  RF_RegionId_t regionID = log->regionID;
  RF_AccTraceEntry_t tracerec;
  RF_SectorNum_t regionOffset;
  RF_DagNode_t *fwr_wrNode;

  /* Initiate the write of a core log to a region log disk.
     Once initiated, return to the calling routine.

     NON-BLOCKING
   */

  /* create DAG to write a core log to a region log disk */
  rf_MakeAllocList(*fwr_alloclist);
  *fwr_dag_h = rf_MakeSimpleDAG(raidPtr, 1, 0, log->bufPtr, rf_DiskWriteFunc, rf_DiskWriteUndoFunc,
			     "Wcl", *fwr_alloclist, RF_DAG_FLAGS_NONE, RF_IO_NORMAL_PRIORITY);

  /* create and initialize PDA for the region log */
  /* RF_Malloc(*fwr_pda, sizeof(RF_PhysDiskAddr_t), (RF_PhysDiskAddr_t *)); */
  *fwr_pda = rf_AllocPDAList(1);
  regionOffset = log->diskOffset;
  rf_MapLogParityLogging(raidPtr, regionID, regionOffset, &((*fwr_pda)->row), &((*fwr_pda)->col), &((*fwr_pda)->startSector));
  (*fwr_pda)->numSector = raidPtr->numSectorsPerLog;

  /* initialize DAG parameters */
  bzero((char *)&tracerec,sizeof(tracerec));
  (*fwr_dag_h)->tracerec = &tracerec;
  fwr_wrNode = (*fwr_dag_h)->succedents[0]->succedents[0];
  fwr_wrNode->params[0].p = *fwr_pda;
/*  fwr_wrNode->params[1] = log->bufPtr; */
  fwr_wrNode->params[2].v = 0;
  fwr_wrNode->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, 0);
  
  /* launch the dag to write the core log to disk */
  rf_DispatchDAG(*fwr_dag_h, (void (*)(void *)) rf_MCPairWakeupFunc,
		 (void *) fwr_mcpair);
}


static void ReadRegionParity(
  RF_RegionId_t         regionID,
  RF_MCPair_t          *prd_mcpair,
  caddr_t               parityBuffer,
  RF_Raid_t            *raidPtr,
  RF_DagHeader_t      **prd_dag_h,
  RF_AllocListElem_t  **prd_alloclist,
  RF_PhysDiskAddr_t   **prd_pda)
{
  /* Initiate the read region parity from disk.
     Once initiated, return to the calling routine.

     NON-BLOCKING
   */

  RF_AccTraceEntry_t tracerec;
  RF_DagNode_t *prd_rdNode;

  /* create DAG to read region parity from disk */
  rf_MakeAllocList(*prd_alloclist);
  *prd_dag_h = rf_MakeSimpleDAG(raidPtr, 1, 0, NULL, rf_DiskReadFunc, rf_DiskReadUndoFunc,
			     "Rrp", *prd_alloclist, RF_DAG_FLAGS_NONE, RF_IO_NORMAL_PRIORITY);

  /* create and initialize PDA for region parity */
  /* RF_Malloc(*prd_pda, sizeof(RF_PhysDiskAddr_t), (RF_PhysDiskAddr_t *)); */
  *prd_pda = rf_AllocPDAList(1);
  rf_MapRegionParity(raidPtr, regionID, &((*prd_pda)->row), &((*prd_pda)->col), &((*prd_pda)->startSector), &((*prd_pda)->numSector));
  if (rf_parityLogDebug)
    printf("[reading %d sectors of parity from region %d]\n", 
	   (int)(*prd_pda)->numSector, regionID);
  if ((*prd_pda)->next) {
    (*prd_pda)->next = NULL;
    printf("set prd_pda->next to NULL\n");
  }

  /* initialize DAG parameters */
  bzero((char *)&tracerec,sizeof(tracerec));
  (*prd_dag_h)->tracerec = &tracerec;
  prd_rdNode = (*prd_dag_h)->succedents[0]->succedents[0];
  prd_rdNode->params[0].p = *prd_pda;
  prd_rdNode->params[1].p = parityBuffer;
  prd_rdNode->params[2].v = 0;
  prd_rdNode->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, 0);
  if (rf_validateDAGDebug)
    rf_ValidateDAG(*prd_dag_h);
  /* launch region parity read dag */
  rf_DispatchDAG(*prd_dag_h, (void (*)(void *)) rf_MCPairWakeupFunc, 
		 (void *) prd_mcpair);
}

static void WriteRegionParity(
  RF_RegionId_t         regionID,
  RF_MCPair_t          *pwr_mcpair,
  caddr_t               parityBuffer,
  RF_Raid_t            *raidPtr,
  RF_DagHeader_t      **pwr_dag_h,
  RF_AllocListElem_t  **pwr_alloclist,
  RF_PhysDiskAddr_t   **pwr_pda)
{
  /* Initiate the write of region parity to disk.
     Once initiated, return to the calling routine.

     NON-BLOCKING
   */

  RF_AccTraceEntry_t tracerec;
  RF_DagNode_t *pwr_wrNode;

  /* create DAG to write region log from disk */
  rf_MakeAllocList(*pwr_alloclist);
  *pwr_dag_h = rf_MakeSimpleDAG(raidPtr, 1, 0, parityBuffer, rf_DiskWriteFunc, rf_DiskWriteUndoFunc,
			     "Wrp", *pwr_alloclist, RF_DAG_FLAGS_NONE, RF_IO_NORMAL_PRIORITY);

  /* create and initialize PDA for region parity */
  /* RF_Malloc(*pwr_pda, sizeof(RF_PhysDiskAddr_t), (RF_PhysDiskAddr_t *)); */
  *pwr_pda = rf_AllocPDAList(1);
  rf_MapRegionParity(raidPtr, regionID, &((*pwr_pda)->row), &((*pwr_pda)->col), &((*pwr_pda)->startSector), &((*pwr_pda)->numSector));

  /* initialize DAG parameters */
  bzero((char *)&tracerec,sizeof(tracerec));
  (*pwr_dag_h)->tracerec = &tracerec;
  pwr_wrNode = (*pwr_dag_h)->succedents[0]->succedents[0];
  pwr_wrNode->params[0].p = *pwr_pda;
/*  pwr_wrNode->params[1] = parityBuffer; */
  pwr_wrNode->params[2].v = 0;
  pwr_wrNode->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, 0);

  /* launch the dag to write region parity to disk */
  rf_DispatchDAG(*pwr_dag_h, (void (*)(void *))rf_MCPairWakeupFunc,
			      (void *) pwr_mcpair);
}

static void FlushLogsToDisk(
  RF_Raid_t       *raidPtr,
  RF_ParityLog_t  *logList)
{
  /* Flush a linked list of core logs to the log disk.
     Logs contain the disk location where they should be
     written.  Logs were written in FIFO order and that
     order must be preserved.

     Recommended optimizations:
       1) allow multiple flushes to occur simultaneously
       2) coalesce contiguous flush operations

     BLOCKING
     */

  RF_ParityLog_t *log;
  RF_RegionId_t regionID;
  RF_MCPair_t *fwr_mcpair;
  RF_DagHeader_t *fwr_dag_h;
  RF_AllocListElem_t *fwr_alloclist;
  RF_PhysDiskAddr_t *fwr_pda;

  fwr_mcpair = rf_AllocMCPair();
  RF_LOCK_MUTEX(fwr_mcpair->mutex);
  
  RF_ASSERT(logList);
  log = logList;
  while (log)
    {
      regionID = log->regionID;
      
      /* create and launch a DAG to write the core log */
      if (rf_parityLogDebug)
	printf("[initiating write of core log for region %d]\n", regionID);
      fwr_mcpair->flag = RF_FALSE;
      WriteCoreLog(log, fwr_mcpair, raidPtr, &fwr_dag_h, &fwr_alloclist, &fwr_pda);

      /* wait for the DAG to complete */
#ifndef SIMULATE
      while (!fwr_mcpair->flag)
	RF_WAIT_COND(fwr_mcpair->cond, fwr_mcpair->mutex);
#endif /* !SIMULATE */
      if (fwr_dag_h->status != rf_enable)
	{
	  RF_ERRORMSG1("Unable to write core log to disk (region %d)\n", regionID);
	  RF_ASSERT(0);
	}

      /* RF_Free(fwr_pda, sizeof(RF_PhysDiskAddr_t)); */
      rf_FreePhysDiskAddr(fwr_pda);
      rf_FreeDAG(fwr_dag_h);
      rf_FreeAllocList(fwr_alloclist);

      log = log->next;
    }
  RF_UNLOCK_MUTEX(fwr_mcpair->mutex);
  rf_FreeMCPair(fwr_mcpair);
  rf_ReleaseParityLogs(raidPtr, logList);
}

static void ReintegrateRegion(
  RF_Raid_t       *raidPtr,
  RF_RegionId_t    regionID,
  RF_ParityLog_t  *coreLog)
{
  RF_MCPair_t *rrd_mcpair=NULL, *prd_mcpair, *pwr_mcpair;
  RF_DagHeader_t *rrd_dag_h, *prd_dag_h, *pwr_dag_h;
  RF_AllocListElem_t *rrd_alloclist, *prd_alloclist, *pwr_alloclist;
  RF_PhysDiskAddr_t *rrd_pda, *prd_pda, *pwr_pda;
  caddr_t parityBuffer, regionBuffer=NULL;

  /* Reintegrate a region (regionID).
     1. acquire region and parity buffers
     2. read log from disk
     3. read parity from disk
     4. apply log to parity
     5. apply core log to parity
     6. write new parity to disk

     BLOCKING
    */

  if (rf_parityLogDebug)
    printf("[reintegrating region %d]\n", regionID);

  /* initiate read of region parity */
  if (rf_parityLogDebug)
    printf("[initiating read of parity for region %d]\n", regionID);
  parityBuffer = AcquireReintBuffer(&raidPtr->parityBufferPool);
  prd_mcpair = rf_AllocMCPair();
  RF_LOCK_MUTEX(prd_mcpair->mutex);
  prd_mcpair->flag = RF_FALSE;
  ReadRegionParity(regionID, prd_mcpair, parityBuffer, raidPtr, &prd_dag_h, &prd_alloclist, &prd_pda);

  /* if region log nonempty, initiate read */
  if (raidPtr->regionInfo[regionID].diskCount > 0)
    {
      if (rf_parityLogDebug)
	printf("[initiating read of disk log for region %d]\n", regionID);
      regionBuffer = AcquireReintBuffer(&raidPtr->regionBufferPool);
      rrd_mcpair = rf_AllocMCPair();
      RF_LOCK_MUTEX(rrd_mcpair->mutex);
      rrd_mcpair->flag = RF_FALSE;
      ReadRegionLog(regionID, rrd_mcpair, regionBuffer, raidPtr, &rrd_dag_h, &rrd_alloclist, &rrd_pda);
    }

  /* wait on read of region parity to complete */
#ifndef SIMULATE
  while (!prd_mcpair->flag) {
    RF_WAIT_COND(prd_mcpair->cond, prd_mcpair->mutex);
  }
#endif /* !SIMULATE */
  RF_UNLOCK_MUTEX(prd_mcpair->mutex);
  if (prd_dag_h->status != rf_enable)
    {
      RF_ERRORMSG("Unable to read parity from disk\n");
      /* add code to fail the parity disk */
      RF_ASSERT(0);
    }
  
  /* apply core log to parity */
  /*  if (coreLog)
      ApplyLogsToParity(coreLog, parityBuffer); */
  
  if (raidPtr->regionInfo[regionID].diskCount > 0)
    {
      /* wait on read of region log to complete */
#ifndef SIMULATE
      while (!rrd_mcpair->flag)
	RF_WAIT_COND(rrd_mcpair->cond, rrd_mcpair->mutex);
#endif /* !SIMULATE */
      RF_UNLOCK_MUTEX(rrd_mcpair->mutex);
      if (rrd_dag_h->status != rf_enable)
	{
	  RF_ERRORMSG("Unable to read region log from disk\n");
	  /* add code to fail the log disk */
	  RF_ASSERT(0);
	}
      /* apply region log to parity */
      /*      ApplyRegionToParity(regionID, regionBuffer, parityBuffer); */
      /* release resources associated with region log */
      /* RF_Free(rrd_pda, sizeof(RF_PhysDiskAddr_t)); */
      rf_FreePhysDiskAddr(rrd_pda);
      rf_FreeDAG(rrd_dag_h);
      rf_FreeAllocList(rrd_alloclist);
      rf_FreeMCPair(rrd_mcpair);
      ReleaseReintBuffer(&raidPtr->regionBufferPool, regionBuffer);
    }

  /* write reintegrated parity to disk */
  if (rf_parityLogDebug)
    printf("[initiating write of parity for region %d]\n", regionID);
  pwr_mcpair = rf_AllocMCPair();
  RF_LOCK_MUTEX(pwr_mcpair->mutex);
  pwr_mcpair->flag = RF_FALSE;
  WriteRegionParity(regionID, pwr_mcpair, parityBuffer, raidPtr, &pwr_dag_h, &pwr_alloclist, &pwr_pda);
#ifndef SIMULATE
  while (!pwr_mcpair->flag)
    RF_WAIT_COND(pwr_mcpair->cond, pwr_mcpair->mutex);
#endif /* !SIMULATE */
  RF_UNLOCK_MUTEX(pwr_mcpair->mutex);
  if (pwr_dag_h->status != rf_enable)
    {
      RF_ERRORMSG("Unable to write parity to disk\n");
      /* add code to fail the parity disk */
      RF_ASSERT(0);
    }

  /* release resources associated with read of old parity */
  /* RF_Free(prd_pda, sizeof(RF_PhysDiskAddr_t)); */
  rf_FreePhysDiskAddr(prd_pda);
  rf_FreeDAG(prd_dag_h);
  rf_FreeAllocList(prd_alloclist);
  rf_FreeMCPair(prd_mcpair);

  /* release resources associated with write of new parity */
  ReleaseReintBuffer(&raidPtr->parityBufferPool, parityBuffer);
  /* RF_Free(pwr_pda, sizeof(RF_PhysDiskAddr_t)); */
  rf_FreePhysDiskAddr(pwr_pda);
  rf_FreeDAG(pwr_dag_h);
  rf_FreeAllocList(pwr_alloclist);
  rf_FreeMCPair(pwr_mcpair);

  if (rf_parityLogDebug)
    printf("[finished reintegrating region %d]\n", regionID);
}



static void ReintegrateLogs(
  RF_Raid_t       *raidPtr,
  RF_ParityLog_t  *logList)
{
  RF_ParityLog_t *log, *freeLogList = NULL;
  RF_ParityLogData_t *logData, *logDataList;
  RF_RegionId_t regionID;

  RF_ASSERT(logList);
  while (logList)
    {
      log = logList;
      logList = logList->next;
      log->next = NULL;
      regionID = log->regionID;
      ReintegrateRegion(raidPtr, regionID, log);
      log->numRecords = 0;

      /* remove all items which are blocked on reintegration of this region */
      RF_LOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);
      logData = rf_SearchAndDequeueParityLogData(raidPtr, regionID, &raidPtr->parityLogDiskQueue.reintBlockHead, &raidPtr->parityLogDiskQueue.reintBlockTail, RF_TRUE);
      logDataList = logData;
      while (logData)
	{
	  logData->next = rf_SearchAndDequeueParityLogData(raidPtr, regionID, &raidPtr->parityLogDiskQueue.reintBlockHead, &raidPtr->parityLogDiskQueue.reintBlockTail, RF_TRUE);
	  logData = logData->next;
	}
      RF_UNLOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);

      /* process blocked log data and clear reintInProgress flag for this region */
      if (logDataList)
	rf_ParityLogAppend(logDataList, RF_TRUE, &log, RF_TRUE);
      else
	{
	  /* Enable flushing for this region.  Holding both locks provides
	     a synchronization barrier with DumpParityLogToDisk
	     */
	  RF_LOCK_MUTEX(raidPtr->regionInfo[regionID].mutex);
	  RF_LOCK_MUTEX(raidPtr->regionInfo[regionID].reintMutex);
	  RF_LOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);
	  raidPtr->regionInfo[regionID].diskCount = 0;
	  raidPtr->regionInfo[regionID].reintInProgress = RF_FALSE;
	  RF_UNLOCK_MUTEX(raidPtr->regionInfo[regionID].mutex);
	  RF_UNLOCK_MUTEX(raidPtr->regionInfo[regionID].reintMutex); /* flushing is now enabled */
	  RF_UNLOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);
	}
      /* if log wasn't used, attach it to the list of logs to be returned */
      if (log)
	{
	  log->next = freeLogList;
	  freeLogList = log;
	}
    }
  if (freeLogList)
    rf_ReleaseParityLogs(raidPtr, freeLogList);
}

int rf_ShutdownLogging(RF_Raid_t *raidPtr)
{
  /* shutdown parity logging
     1) disable parity logging in all regions
     2) reintegrate all regions
     */

  RF_SectorCount_t diskCount;
  RF_RegionId_t regionID;
  RF_ParityLog_t *log;

  if (rf_parityLogDebug)
    printf("[shutting down parity logging]\n");
  /* Since parity log maps are volatile, we must reintegrate all regions. */
  if (rf_forceParityLogReint) {
    for (regionID = 0; regionID < rf_numParityRegions; regionID++)
      {
	RF_LOCK_MUTEX(raidPtr->regionInfo[regionID].mutex);
	raidPtr->regionInfo[regionID].loggingEnabled = RF_FALSE;
	log = raidPtr->regionInfo[regionID].coreLog;
	raidPtr->regionInfo[regionID].coreLog = NULL;
	diskCount = raidPtr->regionInfo[regionID].diskCount;
	RF_UNLOCK_MUTEX(raidPtr->regionInfo[regionID].mutex);
	if (diskCount > 0 || log != NULL)
	  ReintegrateRegion(raidPtr, regionID, log);
	if (log != NULL)
	  rf_ReleaseParityLogs(raidPtr, log);
      }
  }
  if (rf_parityLogDebug)
    {
      printf("[parity logging disabled]\n");
      printf("[should be done!]\n");
    }
  return(0);
}

int rf_ParityLoggingDiskManager(RF_Raid_t *raidPtr)
{
  RF_ParityLog_t *reintQueue, *flushQueue;
  int workNeeded, done = RF_FALSE;

  rf_assign_threadid(); /* don't remove this line */

  /* Main program for parity logging disk thread.  This routine waits
     for work to appear in either the flush or reintegration queues
     and is responsible for flushing core logs to the log disk as
     well as reintegrating parity regions.

     BLOCKING
     */

  RF_LOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);

  /*
   * Inform our creator that we're running. Don't bother doing the
   * mutex lock/unlock dance- we locked above, and we'll unlock
   * below with nothing to do, yet.
   */
  raidPtr->parityLogDiskQueue.threadState |= RF_PLOG_RUNNING;
  RF_SIGNAL_COND(raidPtr->parityLogDiskQueue.cond);

  /* empty the work queues */
  flushQueue = raidPtr->parityLogDiskQueue.flushQueue;  raidPtr->parityLogDiskQueue.flushQueue = NULL;
  reintQueue = raidPtr->parityLogDiskQueue.reintQueue;  raidPtr->parityLogDiskQueue.reintQueue = NULL;
  workNeeded = (flushQueue || reintQueue);

  while (!done)
    {
      while (workNeeded)
	{
	  /* First, flush all logs in the flush queue, freeing buffers
	     Second, reintegrate all regions which are reported as full.
	     Third, append queued log data until blocked.

	     Note: Incoming appends (ParityLogAppend) can block on either
	       1. empty buffer pool
	       2. region under reintegration
	     To preserve a global FIFO ordering of appends, buffers are not
	     released to the world until those appends blocked on buffers are
	     removed from the append queue.  Similarly, regions which are
	     reintegrated are not opened for general use until the append
	     queue has been emptied.
	     */

	  RF_UNLOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);

	  /* empty flushQueue, using free'd log buffers to process bufTail */
	  if (flushQueue)
	    FlushLogsToDisk(raidPtr, flushQueue);

	  /* empty reintQueue, flushing from reintTail as we go */
	  if (reintQueue)
	    ReintegrateLogs(raidPtr, reintQueue);

	  RF_LOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);
	  flushQueue = raidPtr->parityLogDiskQueue.flushQueue;  raidPtr->parityLogDiskQueue.flushQueue = NULL;
	  reintQueue = raidPtr->parityLogDiskQueue.reintQueue;  raidPtr->parityLogDiskQueue.reintQueue = NULL;
	  workNeeded = (flushQueue || reintQueue);
	}
      /* no work is needed at this point */
      if (raidPtr->parityLogDiskQueue.threadState&RF_PLOG_TERMINATE)
	{
	  /* shutdown parity logging
	     1. disable parity logging in all regions
	     2. reintegrate all regions
	     */
	  done = RF_TRUE;  /* thread disabled, no work needed */
	  RF_UNLOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);
	  rf_ShutdownLogging(raidPtr);
	}
      if (!done)
	{
	  /* thread enabled, no work needed, so sleep */
	  if (rf_parityLogDebug)
	    printf("[parity logging disk manager sleeping]\n");
	  RF_WAIT_COND(raidPtr->parityLogDiskQueue.cond, raidPtr->parityLogDiskQueue.mutex);
	  if (rf_parityLogDebug)
	    printf("[parity logging disk manager just woke up]\n");
	  flushQueue = raidPtr->parityLogDiskQueue.flushQueue;  raidPtr->parityLogDiskQueue.flushQueue = NULL;
	  reintQueue = raidPtr->parityLogDiskQueue.reintQueue;  raidPtr->parityLogDiskQueue.reintQueue = NULL;
	  workNeeded = (flushQueue || reintQueue);
	}
    }
  /*
   * Announce that we're done.
   */
  RF_LOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);
  raidPtr->parityLogDiskQueue.threadState |= RF_PLOG_SHUTDOWN;
  RF_UNLOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);
  RF_SIGNAL_COND(raidPtr->parityLogDiskQueue.cond);
#if (defined(__NetBSD__) || defined(__OpenBSD__)) && defined(_KERNEL)
  /*
   * In the Net- and OpenBSD kernels, the thread must exit; returning would
   * cause the proc trampoline to attempt to return to userspace.
   */
  kthread_exit(0);	/* does not return */
#else
  return(0);
#endif
}

#endif /* RF_INCLUDE_PARITYLOGGING > 0 */
