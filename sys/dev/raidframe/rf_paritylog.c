/*	$OpenBSD: rf_paritylog.c,v 1.1 1999/01/11 14:29:34 niklas Exp $	*/
/*	$NetBSD: rf_paritylog.c,v 1.1 1998/11/13 04:20:31 oster Exp $	*/
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

/* Code for manipulating in-core parity logs
 *
 * :  
 * Log: rf_paritylog.c,v 
 * Revision 1.27  1996/07/28 20:31:39  jimz
 * i386netbsd port
 * true/false fixup
 *
 * Revision 1.26  1996/07/27  23:36:08  jimz
 * Solaris port of simulator
 *
 * Revision 1.25  1996/07/17  21:00:58  jimz
 * clean up timer interface, tracing
 *
 * Revision 1.24  1996/06/11  10:18:59  jimz
 * AllocParityLogCommonData() was freeing the common pointer immediately
 * after allocating this. It appeared that this free really belonged
 * inside one of the failure cases (for backing out), so I moved it
 * in there.
 *
 * Revision 1.23  1996/06/05  18:06:02  jimz
 * Major code cleanup. The Great Renaming is now done.
 * Better modularity. Better typing. Fixed a bunch of
 * synchronization bugs. Made a lot of global stuff
 * per-desc or per-array. Removed dead code.
 *
 * Revision 1.22  1996/06/02  17:31:48  jimz
 * Moved a lot of global stuff into array structure, where it belongs.
 * Fixed up paritylogging, pss modules in this manner. Some general
 * code cleanup. Removed lots of dead code, some dead files.
 *
 * Revision 1.21  1996/05/31  22:26:54  jimz
 * fix a lot of mapping problems, memory allocation problems
 * found some weird lock issues, fixed 'em
 * more code cleanup
 *
 * Revision 1.20  1996/05/30  23:22:16  jimz
 * bugfixes of serialization, timing problems
 * more cleanup
 *
 * Revision 1.19  1996/05/30  12:59:18  jimz
 * make etimer happier, more portable
 *
 * Revision 1.18  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.17  1996/05/24  04:28:55  jimz
 * release cleanup ckpt
 *
 * Revision 1.16  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.15  1996/05/23  00:33:23  jimz
 * code cleanup: move all debug decls to rf_options.c, all extern
 * debug decls to rf_options.h, all debug vars preceded by rf_
 *
 * Revision 1.14  1996/05/20  16:16:59  jimz
 * switch to rf_{mutex,cond}_{init,destroy}
 *
 * Revision 1.13  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.12  1995/12/12  18:10:06  jimz
 * MIN -> RF_MIN, MAX -> RF_MAX, ASSERT -> RF_ASSERT
 * fix 80-column brain damage in comments
 *
 * Revision 1.11  1995/12/06  20:54:44  wvcii
 * added prototyping
 *
 * Revision 1.10  1995/11/30  16:05:37  wvcii
 * added copyright info
 *
 * Revision 1.9  1995/10/08  20:41:28  wvcii
 * fixed bug in allocation of CommonLogData (was allocating incorrect size)
 *
 * Revision 1.8  1995/09/07  15:52:12  jimz
 * noop compile when INCLUDE_PARITYLOGGING not defined
 *
 * Revision 1.7  1995/09/06  19:17:36  wvcii
 * moved code for reintegration to rf_paritylogDiskMgr.c
 *
 * Revision 1.6  95/07/07  00:16:06  wvcii
 * this version free from deadlock, fails parity verification
 * 
 * Revision 1.5  1995/06/09  13:14:24  wvcii
 * code is now nonblocking
 *
 * Revision 1.4  95/06/01  17:01:59  wvcii
 * code debug
 * 
 * Revision 1.3  95/05/31  13:08:23  wvcii
 * code debug
 * 
 * Revision 1.2  95/05/21  15:42:15  wvcii
 * code debug
 * 
 * Revision 1.1  95/05/18  10:43:54  wvcii
 * Initial revision
 * 
 */

#include "rf_archs.h"

#if RF_INCLUDE_PARITYLOGGING > 0

/*
 * Append-only log for recording parity "update" and "overwrite" records
 */

#include "rf_types.h"
#include "rf_threadstuff.h"
#include "rf_mcpair.h"
#include "rf_raid.h"
#include "rf_dag.h"
#include "rf_dagfuncs.h"
#include "rf_desc.h"
#include "rf_layout.h"
#include "rf_diskqueue.h"
#include "rf_etimer.h"
#include "rf_paritylog.h"
#include "rf_general.h"
#include "rf_threadid.h"
#include "rf_map.h"
#include "rf_paritylogging.h"
#include "rf_paritylogDiskMgr.h"
#include "rf_sys.h"

static RF_CommonLogData_t *AllocParityLogCommonData(RF_Raid_t *raidPtr)
{
  RF_CommonLogData_t *common = NULL;
  int rc;

  /* Return a struct for holding common parity log information from the free
     list (rf_parityLogDiskQueue.freeCommonList).  If the free list is empty, call
     RF_Malloc to create a new structure.
     NON-BLOCKING */

  RF_LOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);
  if (raidPtr->parityLogDiskQueue.freeCommonList)
    {
      common = raidPtr->parityLogDiskQueue.freeCommonList;
      raidPtr->parityLogDiskQueue.freeCommonList = raidPtr->parityLogDiskQueue.freeCommonList->next;
      RF_UNLOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);
    }
  else
    {
      RF_UNLOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);
      RF_Malloc(common, sizeof(RF_CommonLogData_t), (RF_CommonLogData_t *));
      rc = rf_mutex_init(&common->mutex);
      if (rc) {
        RF_ERRORMSG3("Unable to init mutex file %s line %d rc=%d\n", __FILE__,
          __LINE__, rc);
        RF_Free(common, sizeof(RF_CommonLogData_t));
        common = NULL;
      }
    }
  common->next = NULL;
  return(common);
}

static void FreeParityLogCommonData(RF_CommonLogData_t *common)
{
  RF_Raid_t *raidPtr;

  /* Insert a single struct for holding parity log information
     (data) into the free list (rf_parityLogDiskQueue.freeCommonList).
     NON-BLOCKING */

  raidPtr = common->raidPtr;
  RF_LOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);
  common->next = raidPtr->parityLogDiskQueue.freeCommonList;
  raidPtr->parityLogDiskQueue.freeCommonList = common;
  RF_UNLOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);
}

static RF_ParityLogData_t *AllocParityLogData(RF_Raid_t *raidPtr)
{
  RF_ParityLogData_t *data = NULL;

  /* Return a struct for holding parity log information from the free
     list (rf_parityLogDiskQueue.freeList).  If the free list is empty, call
     RF_Malloc to create a new structure.
     NON-BLOCKING */
  
  RF_LOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);
  if (raidPtr->parityLogDiskQueue.freeDataList)
    {
      data = raidPtr->parityLogDiskQueue.freeDataList;
      raidPtr->parityLogDiskQueue.freeDataList = raidPtr->parityLogDiskQueue.freeDataList->next;
      RF_UNLOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);
    }
  else
    {
      RF_UNLOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);
      RF_Malloc(data, sizeof(RF_ParityLogData_t), (RF_ParityLogData_t *));
    }
  data->next = NULL;
  data->prev = NULL;
  return(data);
}


static void FreeParityLogData(RF_ParityLogData_t *data)
{
  RF_ParityLogData_t *nextItem;
  RF_Raid_t *raidPtr;

  /* Insert a linked list of structs for holding parity log
     information (data) into the free list (parityLogDiskQueue.freeList).
     NON-BLOCKING */

  raidPtr = data->common->raidPtr;
  RF_LOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);
  while (data)
    {
      nextItem = data->next;
      data->next = raidPtr->parityLogDiskQueue.freeDataList;
      raidPtr->parityLogDiskQueue.freeDataList = data;
      data = nextItem;
    }
  RF_UNLOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);
}


static void EnqueueParityLogData(
  RF_ParityLogData_t   *data,
  RF_ParityLogData_t  **head,
  RF_ParityLogData_t  **tail)
{
  RF_Raid_t *raidPtr;

  /* Insert an in-core parity log (*data) into the head of
     a disk queue (*head, *tail).
     NON-BLOCKING */

  raidPtr = data->common->raidPtr;
  if (rf_parityLogDebug)
    printf("[enqueueing parity log data, region %d, raidAddress %d, numSector %d]\n",data->regionID,(int)data->diskAddress.raidAddress, (int)data->diskAddress.numSector);
  RF_ASSERT(data->prev == NULL);
  RF_ASSERT(data->next == NULL);
  RF_LOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);
  if (*head)
    {
      /* insert into head of queue */
      RF_ASSERT((*head)->prev == NULL);
      RF_ASSERT((*tail)->next == NULL);
      data->next = *head;
      (*head)->prev = data;
      *head = data;
    }
  else
    {
      /* insert into empty list */
      RF_ASSERT(*head == NULL);
      RF_ASSERT(*tail == NULL);
      *head = data;
      *tail = data;
    }
  RF_ASSERT((*head)->prev == NULL);
  RF_ASSERT((*tail)->next == NULL);
  RF_UNLOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);
}

static RF_ParityLogData_t *DequeueParityLogData(
  RF_Raid_t            *raidPtr,
  RF_ParityLogData_t  **head,
  RF_ParityLogData_t  **tail,
  int                   ignoreLocks)
{
  RF_ParityLogData_t *data;

  /* Remove and return an in-core parity log from the tail of
     a disk queue (*head, *tail).
     NON-BLOCKING */

  /* remove from tail, preserving FIFO order */
  if (!ignoreLocks)
    RF_LOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);
  data = *tail;
  if (data)
    {
      if (*head == *tail)
	{
	  /* removing last item from queue */
	  *head = NULL;
	  *tail = NULL;
	}
      else
	{
	  *tail = (*tail)->prev;
	  (*tail)->next = NULL;
	  RF_ASSERT((*head)->prev == NULL);
	  RF_ASSERT((*tail)->next == NULL);
	}
      data->next = NULL;
      data->prev = NULL;
      if (rf_parityLogDebug)
	printf("[dequeueing parity log data, region %d, raidAddress %d, numSector %d]\n",data->regionID,(int)data->diskAddress.raidAddress, (int)data->diskAddress.numSector);
    }
  if (*head)
    {
      RF_ASSERT((*head)->prev == NULL);
      RF_ASSERT((*tail)->next == NULL);
    }
  if (!ignoreLocks)
    RF_UNLOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);
  return(data);
}


static void RequeueParityLogData(
  RF_ParityLogData_t   *data,
  RF_ParityLogData_t  **head,
  RF_ParityLogData_t  **tail)
{
  RF_Raid_t *raidPtr;

  /* Insert an in-core parity log (*data) into the tail of
     a disk queue (*head, *tail).
     NON-BLOCKING */

  raidPtr = data->common->raidPtr;
  RF_ASSERT(data);
  if (rf_parityLogDebug)
    printf("[requeueing parity log data, region %d, raidAddress %d, numSector %d]\n",data->regionID,(int)data->diskAddress.raidAddress, (int) data->diskAddress.numSector);
  RF_LOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);
  if (*tail)
    {
      /* append to tail of list */
      data->prev = *tail;
      data->next = NULL;
      (*tail)->next = data;
      *tail = data;
    }
  else
    {
      /* inserting into an empty list */
      *head = data;
      *tail = data;
      (*head)->prev = NULL;
      (*tail)->next = NULL;
    }
  RF_ASSERT((*head)->prev == NULL);
  RF_ASSERT((*tail)->next == NULL);
  RF_UNLOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);
}

RF_ParityLogData_t *rf_CreateParityLogData(
  RF_ParityRecordType_t    operation,
  RF_PhysDiskAddr_t       *pda,
  caddr_t                  bufPtr,
  RF_Raid_t               *raidPtr,
  int                    (*wakeFunc)(RF_DagNode_t *node, int status),
  void                    *wakeArg,
  RF_AccTraceEntry_t      *tracerec,
  RF_Etimer_t              startTime)
{
  RF_ParityLogData_t *data, *resultHead = NULL, *resultTail = NULL;
  RF_CommonLogData_t *common;
  RF_PhysDiskAddr_t *diskAddress;
  int boundary, offset = 0;

  /* Return an initialized struct of info to be logged.
     Build one item per physical disk address, one item per region.

     NON-BLOCKING */

  diskAddress = pda;
  common = AllocParityLogCommonData(raidPtr);
  RF_ASSERT(common);

  common->operation = operation;
  common->bufPtr = bufPtr;
  common->raidPtr = raidPtr;
  common->wakeFunc = wakeFunc;
  common->wakeArg = wakeArg;
  common->tracerec = tracerec;
  common->startTime = startTime;
  common->cnt = 0;

  if (rf_parityLogDebug)
    printf("[entering CreateParityLogData]\n");
  while (diskAddress)
    {
      common->cnt++;
      data = AllocParityLogData(raidPtr);
      RF_ASSERT(data);
      data->common = common;
      data->next = NULL;
      data->prev = NULL;
      data->regionID = rf_MapRegionIDParityLogging(raidPtr, diskAddress->startSector);
      if (data->regionID == rf_MapRegionIDParityLogging(raidPtr, diskAddress->startSector + diskAddress->numSector - 1))
	{
	  /* disk address does not cross a region boundary */
	  data->diskAddress = *diskAddress;
	  data->bufOffset = offset;
	  offset = offset + diskAddress->numSector;
	  EnqueueParityLogData(data, &resultHead, &resultTail);
	  /* adjust disk address */
	  diskAddress = diskAddress->next;
	}
      else
	{
	  /* disk address crosses a region boundary */
	  /* find address where region is crossed */
	  boundary = 0;
	  while (data->regionID == rf_MapRegionIDParityLogging(raidPtr, diskAddress->startSector + boundary))
	    boundary++;

	  /* enter data before the boundary */
	  data->diskAddress = *diskAddress;
	  data->diskAddress.numSector = boundary;
	  data->bufOffset = offset;
	  offset += boundary;
	  EnqueueParityLogData(data, &resultHead, &resultTail);
	  /* adjust disk address */
	  diskAddress->startSector += boundary;
	  diskAddress->numSector -= boundary;
	}
    }
  if (rf_parityLogDebug)
    printf("[leaving CreateParityLogData]\n");
  return(resultHead);
}


RF_ParityLogData_t *rf_SearchAndDequeueParityLogData(
  RF_Raid_t            *raidPtr,
  int                   regionID,
  RF_ParityLogData_t  **head,
  RF_ParityLogData_t  **tail,
  int                   ignoreLocks)
{
  RF_ParityLogData_t *w;

  /* Remove and return an in-core parity log from a specified region (regionID).
     If a matching log is not found, return NULL.

     NON-BLOCKING.
     */

  /* walk backward through a list, looking for an entry with a matching region ID */
  if (!ignoreLocks)
    RF_LOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);
  w = (*tail);
  while (w)
    {
      if (w->regionID == regionID)
	{
	  /* remove an element from the list */
	  if (w == *tail)
	    {
	      if (*head == *tail)
		{
		  /* removing only element in the list */
		  *head = NULL;
		  *tail = NULL;
		}
	      else
		{
		  /* removing last item in the list */
		  *tail = (*tail)->prev;
		  (*tail)->next = NULL;
		  RF_ASSERT((*head)->prev == NULL);
		  RF_ASSERT((*tail)->next == NULL);
		}
	    }
	  else
	    {
	      if (w == *head)
		{
		  /* removing first item in the list */
		  *head = (*head)->next;
		  (*head)->prev = NULL;
		  RF_ASSERT((*head)->prev == NULL);
		  RF_ASSERT((*tail)->next == NULL);
		}
	      else
		{
		  /* removing an item from the middle of the list */
		  w->prev->next = w->next;
		  w->next->prev = w->prev;
		  RF_ASSERT((*head)->prev == NULL);
		  RF_ASSERT((*tail)->next == NULL);
		}
	    }
	  w->prev = NULL;
	  w->next = NULL;
	  if (rf_parityLogDebug)
	    printf("[dequeueing parity log data, region %d, raidAddress %d, numSector %d]\n",w->regionID,(int)w->diskAddress.raidAddress,(int) w->diskAddress.numSector);
	  return(w);
	}
      else
	w = w->prev;
    }
  if (!ignoreLocks)
    RF_UNLOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);
  return(NULL);
}

static RF_ParityLogData_t *DequeueMatchingLogData(
  RF_Raid_t            *raidPtr,
  RF_ParityLogData_t  **head,
  RF_ParityLogData_t  **tail)
{
  RF_ParityLogData_t *logDataList, *logData;
  int regionID;

  /* Remove and return an in-core parity log from the tail of
     a disk queue (*head, *tail).  Then remove all matching
     (identical regionIDs) logData and return as a linked list.

     NON-BLOCKING
     */

  logDataList = DequeueParityLogData(raidPtr, head, tail, RF_TRUE);
  if (logDataList)
    {
      regionID = logDataList->regionID;
      logData = logDataList;
      logData->next = rf_SearchAndDequeueParityLogData(raidPtr, regionID, head, tail, RF_TRUE);
      while (logData->next)
	{
	  logData = logData->next;
	  logData->next = rf_SearchAndDequeueParityLogData(raidPtr, regionID, head, tail, RF_TRUE);
	}
    }
  return(logDataList);
}


static RF_ParityLog_t *AcquireParityLog(
  RF_ParityLogData_t  *logData,
  int                  finish)
{
  RF_ParityLog_t *log = NULL;
  RF_Raid_t *raidPtr;

  /* Grab a log buffer from the pool and return it.
     If no buffers are available, return NULL.
     NON-BLOCKING
     */
  raidPtr = logData->common->raidPtr;
  RF_LOCK_MUTEX(raidPtr->parityLogPool.mutex);
  if (raidPtr->parityLogPool.parityLogs)
    {
      log = raidPtr->parityLogPool.parityLogs;
      raidPtr->parityLogPool.parityLogs = raidPtr->parityLogPool.parityLogs->next;
      log->regionID = logData->regionID;
      log->numRecords = 0;
      log->next = NULL;
      raidPtr->logsInUse++;
      RF_ASSERT(raidPtr->logsInUse >= 0 && raidPtr->logsInUse <= raidPtr->numParityLogs);
    }
  else
    {
      /* no logs available, so place ourselves on the queue of work waiting on log buffers
	 this is done while parityLogPool.mutex is held, to ensure synchronization
	 with ReleaseParityLogs.
	 */
      if (rf_parityLogDebug)
	printf("[blocked on log, region %d, finish %d]\n", logData->regionID, finish);
      if (finish)
	RequeueParityLogData(logData, &raidPtr->parityLogDiskQueue.logBlockHead, &raidPtr->parityLogDiskQueue.logBlockTail);
      else
	EnqueueParityLogData(logData, &raidPtr->parityLogDiskQueue.logBlockHead, &raidPtr->parityLogDiskQueue.logBlockTail);
    }
  RF_UNLOCK_MUTEX(raidPtr->parityLogPool.mutex);
  return(log);
}

void rf_ReleaseParityLogs(
  RF_Raid_t       *raidPtr,
  RF_ParityLog_t  *firstLog)
{
  RF_ParityLogData_t *logDataList;
  RF_ParityLog_t *log, *lastLog;
  int cnt;

  /* Insert a linked list of parity logs (firstLog) to 
     the free list (parityLogPool.parityLogPool)

     NON-BLOCKING.
     */

  RF_ASSERT(firstLog);

  /* Before returning logs to global free list, service all
     requests which are blocked on logs.  Holding mutexes for parityLogPool and parityLogDiskQueue
     forces synchronization with AcquireParityLog().
     */
  RF_LOCK_MUTEX(raidPtr->parityLogPool.mutex);
  RF_LOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);
  logDataList = DequeueMatchingLogData(raidPtr, &raidPtr->parityLogDiskQueue.logBlockHead, &raidPtr->parityLogDiskQueue.logBlockTail);
  log = firstLog;
  if (firstLog)
    firstLog = firstLog->next;
  log->numRecords = 0;
  log->next = NULL;
  while (logDataList && log)
    {
      RF_UNLOCK_MUTEX(raidPtr->parityLogPool.mutex);
      RF_UNLOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);
      rf_ParityLogAppend(logDataList, RF_TRUE, &log, RF_FALSE);
      if (rf_parityLogDebug)
	printf("[finishing up buf-blocked log data, region %d]\n", logDataList->regionID);
      if (log == NULL)
	{
	  log = firstLog;
	  if (firstLog)
	    {
	      firstLog = firstLog->next;
	      log->numRecords = 0;
	      log->next = NULL;
	    }
	}
      RF_LOCK_MUTEX(raidPtr->parityLogPool.mutex);
      RF_LOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);
      if (log)
	logDataList = DequeueMatchingLogData(raidPtr, &raidPtr->parityLogDiskQueue.logBlockHead, &raidPtr->parityLogDiskQueue.logBlockTail);
    }
  /* return remaining logs to pool */
  if (log)
    {
      log->next = firstLog;
      firstLog = log;
    }
  if (firstLog)
    {
      lastLog = firstLog;
      raidPtr->logsInUse--;
      RF_ASSERT(raidPtr->logsInUse >= 0 && raidPtr->logsInUse <= raidPtr->numParityLogs);
      while (lastLog->next)
	{
	  lastLog = lastLog->next;
	  raidPtr->logsInUse--;
	  RF_ASSERT(raidPtr->logsInUse >= 0 && raidPtr->logsInUse <= raidPtr->numParityLogs);
	}
      lastLog->next = raidPtr->parityLogPool.parityLogs;
      raidPtr->parityLogPool.parityLogs = firstLog;
      cnt = 0;
      log = raidPtr->parityLogPool.parityLogs;
      while (log)
	{
	  cnt++;
	  log = log->next;
	}
      RF_ASSERT(cnt + raidPtr->logsInUse == raidPtr->numParityLogs);
    }
  RF_UNLOCK_MUTEX(raidPtr->parityLogPool.mutex);
  RF_UNLOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);
}

static void ReintLog(
  RF_Raid_t       *raidPtr,
  int              regionID,
  RF_ParityLog_t  *log)
{
  RF_ASSERT(log);

  /* Insert an in-core parity log (log) into the disk queue of reintegration
     work.  Set the flag (reintInProgress) for the specified region (regionID)
     to indicate that reintegration is in progress for this region.
     NON-BLOCKING
     */

  RF_LOCK_MUTEX(raidPtr->regionInfo[regionID].reintMutex);
  raidPtr->regionInfo[regionID].reintInProgress = RF_TRUE;  /* cleared when reint complete */

  if (rf_parityLogDebug)
    printf("[requesting reintegration of region %d]\n", log->regionID);
  /* move record to reintegration queue */
  RF_LOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);
  log->next = raidPtr->parityLogDiskQueue.reintQueue;
  raidPtr->parityLogDiskQueue.reintQueue = log;
  RF_UNLOCK_MUTEX(raidPtr->regionInfo[regionID].reintMutex);
  RF_UNLOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);
  RF_SIGNAL_COND(raidPtr->parityLogDiskQueue.cond);
}

static void FlushLog(
  RF_Raid_t       *raidPtr,
  RF_ParityLog_t  *log)
{
  /* insert a core log (log) into a list of logs (parityLogDiskQueue.flushQueue)
     waiting to be written to disk.
     NON-BLOCKING
     */

  RF_ASSERT(log);
  RF_ASSERT(log->numRecords == raidPtr->numSectorsPerLog);
  RF_ASSERT(log->next == NULL);
  /* move log to flush queue */
  RF_LOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);
  log->next = raidPtr->parityLogDiskQueue.flushQueue;
  raidPtr->parityLogDiskQueue.flushQueue = log;
  RF_UNLOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);
  RF_SIGNAL_COND(raidPtr->parityLogDiskQueue.cond);
}

static int DumpParityLogToDisk(
  int                  finish,
  RF_ParityLogData_t  *logData)
{
  int i, diskCount, regionID = logData->regionID;
  RF_ParityLog_t *log;
  RF_Raid_t *raidPtr;

  raidPtr = logData->common->raidPtr;

  /* Move a core log to disk.  If the log disk is full, initiate
     reintegration.

     Return (0) if we can enqueue the dump immediately, otherwise
     return (1) to indicate we are blocked on reintegration and
     control of the thread should be relinquished.

     Caller must hold regionInfo[regionID].mutex

     NON-BLOCKING
     */

  if (rf_parityLogDebug)
    printf("[dumping parity log to disk, region %d]\n", regionID);
  log = raidPtr->regionInfo[regionID].coreLog;
  RF_ASSERT(log->numRecords == raidPtr->numSectorsPerLog);
  RF_ASSERT(log->next == NULL);

  /* if reintegration is in progress, must queue work */
  RF_LOCK_MUTEX(raidPtr->regionInfo[regionID].reintMutex);
  if (raidPtr->regionInfo[regionID].reintInProgress)
    {
      /* Can not proceed since this region is currently being reintegrated.
	 We can not block, so queue remaining work and return */
      if (rf_parityLogDebug)
	printf("[region %d waiting on reintegration]\n",regionID);
      /* XXX not sure about the use of finish - shouldn't this always be "Enqueue"? */
      if (finish)
	RequeueParityLogData(logData, &raidPtr->parityLogDiskQueue.reintBlockHead, &raidPtr->parityLogDiskQueue.reintBlockTail);
      else
	EnqueueParityLogData(logData, &raidPtr->parityLogDiskQueue.reintBlockHead, &raidPtr->parityLogDiskQueue.reintBlockTail);
      RF_UNLOCK_MUTEX(raidPtr->regionInfo[regionID].reintMutex);
      return(1);  /* relenquish control of this thread */
    }
  RF_UNLOCK_MUTEX(raidPtr->regionInfo[regionID].reintMutex);
  raidPtr->regionInfo[regionID].coreLog = NULL;
  if ((raidPtr->regionInfo[regionID].diskCount) < raidPtr->regionInfo[regionID].capacity)
    /* IMPORTANT!! this loop bound assumes region disk holds an integral number of core logs */
    {
      /* update disk map for this region */
      diskCount = raidPtr->regionInfo[regionID].diskCount;
      for (i = 0; i < raidPtr->numSectorsPerLog; i++)
	{
	  raidPtr->regionInfo[regionID].diskMap[i + diskCount].operation = log->records[i].operation;
	  raidPtr->regionInfo[regionID].diskMap[i + diskCount].parityAddr = log->records[i].parityAddr;
	}
      log->diskOffset = diskCount;
      raidPtr->regionInfo[regionID].diskCount += raidPtr->numSectorsPerLog;
      FlushLog(raidPtr, log);
    }
  else
    {
      /* no room for log on disk, send it to disk manager and request reintegration */
      RF_ASSERT(raidPtr->regionInfo[regionID].diskCount == raidPtr->regionInfo[regionID].capacity);
      ReintLog(raidPtr, regionID, log);
    }
  if (rf_parityLogDebug)
    printf("[finished dumping parity log to disk, region %d]\n", regionID);
  return(0);
}

int rf_ParityLogAppend(
  RF_ParityLogData_t   *logData,
  int                   finish,
  RF_ParityLog_t      **incomingLog,
  int                   clearReintFlag)
{
  int regionID, logItem, itemDone;
  RF_ParityLogData_t *item;
  int punt, done = RF_FALSE;
  RF_ParityLog_t *log;
  RF_Raid_t *raidPtr;
  RF_Etimer_t timer;
  int (*wakeFunc)(RF_DagNode_t *node, int status);
  void *wakeArg;

  /* Add parity to the appropriate log, one sector at a time.
     This routine is called is called by dag functions ParityLogUpdateFunc
     and ParityLogOverwriteFunc and therefore MUST BE NONBLOCKING.

     Parity to be logged is contained in a linked-list (logData).  When
     this routine returns, every sector in the list will be in one of
     three places:
       1) entered into the parity log
       2) queued, waiting on reintegration
       3) queued, waiting on a core log

     Blocked work is passed to the ParityLoggingDiskManager for completion.
     Later, as conditions which required the block are removed, the work
     reenters this routine with the "finish" parameter set to "RF_TRUE."

     NON-BLOCKING
     */

  raidPtr = logData->common->raidPtr;
  /* lock the region for the first item in logData */
  RF_ASSERT(logData != NULL);
  regionID = logData->regionID;
  RF_LOCK_MUTEX(raidPtr->regionInfo[regionID].mutex);
  RF_ASSERT(raidPtr->regionInfo[regionID].loggingEnabled);

  if (clearReintFlag)
    {
      /* Enable flushing for this region.  Holding both locks provides
	 a synchronization barrier with DumpParityLogToDisk
	 */
      RF_LOCK_MUTEX(raidPtr->regionInfo[regionID].reintMutex);
      RF_LOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);
      RF_ASSERT(raidPtr->regionInfo[regionID].reintInProgress == RF_TRUE);
      raidPtr->regionInfo[regionID].diskCount = 0;
      raidPtr->regionInfo[regionID].reintInProgress = RF_FALSE;
      RF_UNLOCK_MUTEX(raidPtr->regionInfo[regionID].reintMutex); /* flushing is now enabled */
      RF_UNLOCK_MUTEX(raidPtr->parityLogDiskQueue.mutex);
    }

  /* process each item in logData */
  while (logData)
    {
      /* remove an item from logData */
      item = logData;
      logData = logData->next;
      item->next = NULL;
      item->prev = NULL;

      if (rf_parityLogDebug)
	printf("[appending parity log data, region %d, raidAddress %d, numSector %d]\n",item->regionID,(int)item->diskAddress.raidAddress, (int)item->diskAddress.numSector);

      /* see if we moved to a new region */
      if (regionID != item->regionID)
	{
	  RF_UNLOCK_MUTEX(raidPtr->regionInfo[regionID].mutex);
	  regionID = item->regionID;
	  RF_LOCK_MUTEX(raidPtr->regionInfo[regionID].mutex);
	  RF_ASSERT(raidPtr->regionInfo[regionID].loggingEnabled);
	}

      punt = RF_FALSE;  /* Set to RF_TRUE if work is blocked.  This can happen in one of two ways:
		          1) no core log (AcquireParityLog)
			  2) waiting on reintegration (DumpParityLogToDisk)
			If punt is RF_TRUE, the dataItem was queued, so skip to next item.
			*/

      /* process item, one sector at a time, until all sectors processed or we punt */
      if (item->diskAddress.numSector > 0)
	done = RF_FALSE;
      else
	RF_ASSERT(0);
      while (!punt && !done)
	{
	  /* verify that a core log exists for this region */
	  if (!raidPtr->regionInfo[regionID].coreLog)
	    {
	      /* Attempt to acquire a parity log.
		 If acquisition fails, queue remaining work in data item and move to nextItem.
		 */
	      if (incomingLog) {
		if (*incomingLog)
		  {
		    RF_ASSERT((*incomingLog)->next == NULL);
		    raidPtr->regionInfo[regionID].coreLog = *incomingLog;
		    raidPtr->regionInfo[regionID].coreLog->regionID = regionID;
		    *incomingLog = NULL;
		  }
		else
		  raidPtr->regionInfo[regionID].coreLog = AcquireParityLog(item, finish);
	      } else
		raidPtr->regionInfo[regionID].coreLog = AcquireParityLog(item, finish);
	      /* Note: AcquireParityLog either returns a log or enqueues currentItem */
	    }
	  if (!raidPtr->regionInfo[regionID].coreLog)
	    punt = RF_TRUE; /* failed to find a core log */
	  else
	    {
	      RF_ASSERT(raidPtr->regionInfo[regionID].coreLog->next == NULL);
	      /* verify that the log has room for new entries */
	      /* if log is full, dump it to disk and grab a new log */
	      if (raidPtr->regionInfo[regionID].coreLog->numRecords == raidPtr->numSectorsPerLog)
		{
		  /* log is full, dump it to disk */
		  if (DumpParityLogToDisk(finish, item))
		    punt = RF_TRUE; /* dump unsuccessful, blocked on reintegration */
		  else
		    {
		      /* dump was successful */
		      if (incomingLog) {
			if (*incomingLog)
			  {
			    RF_ASSERT((*incomingLog)->next == NULL);
			    raidPtr->regionInfo[regionID].coreLog = *incomingLog;
			    raidPtr->regionInfo[regionID].coreLog->regionID = regionID;
			    *incomingLog = NULL;
			  }
			else
			  raidPtr->regionInfo[regionID].coreLog = AcquireParityLog(item, finish);
		      } else
			raidPtr->regionInfo[regionID].coreLog = AcquireParityLog(item, finish);
		      /* if a core log is not available, must queue work and return */
		      if (!raidPtr->regionInfo[regionID].coreLog)
			punt = RF_TRUE; /* blocked on log availability */
		    }
		}
	    }
	  /* if we didn't punt on this item, attempt to add a sector to the core log */
	  if (!punt)
	    {
	      RF_ASSERT(raidPtr->regionInfo[regionID].coreLog->next == NULL);
	      /* at this point, we have a core log with enough room for a sector */
	      /* copy a sector into the log */
	      log = raidPtr->regionInfo[regionID].coreLog;
	      RF_ASSERT(log->numRecords < raidPtr->numSectorsPerLog);
	      logItem = log->numRecords++;
	      log->records[logItem].parityAddr = item->diskAddress;
	      RF_ASSERT(log->records[logItem].parityAddr.startSector >= raidPtr->regionInfo[regionID].parityStartAddr);
	      RF_ASSERT(log->records[logItem].parityAddr.startSector < raidPtr->regionInfo[regionID].parityStartAddr + raidPtr->regionInfo[regionID].numSectorsParity);
	      log->records[logItem].parityAddr.numSector = 1;
	      log->records[logItem].operation = item->common->operation;
	      bcopy((item->common->bufPtr + (item->bufOffset++ * (1<<item->common->raidPtr->logBytesPerSector))), log->bufPtr + (logItem * (1<<item->common->raidPtr->logBytesPerSector)), (1<<item->common->raidPtr->logBytesPerSector));
	      item->diskAddress.numSector--;
	      item->diskAddress.startSector++;
	      if (item->diskAddress.numSector == 0)
		done = RF_TRUE;
	    }
	}

      if (!punt)
	{
	  /* Processed this item completely, decrement count of items
	     to be processed.
	     */
	  RF_ASSERT(item->diskAddress.numSector == 0);
	  RF_LOCK_MUTEX(item->common->mutex);
	  item->common->cnt--;
	  if (item->common->cnt == 0)
	    itemDone = RF_TRUE;
	  else
	    itemDone = RF_FALSE;
	  RF_UNLOCK_MUTEX(item->common->mutex);
	  if (itemDone)
	    {
	      /* Finished processing all log data for this IO
		 Return structs to free list and invoke wakeup function.
		 */
	      timer = item->common->startTime;  /* grab initial value of timer */
	      RF_ETIMER_STOP(timer);
	      RF_ETIMER_EVAL(timer);
	      item->common->tracerec->plog_us += RF_ETIMER_VAL_US(timer);
	      if (rf_parityLogDebug)
		printf("[waking process for region %d]\n", item->regionID);
	      wakeFunc = item->common->wakeFunc;
	      wakeArg = item->common->wakeArg;
	      FreeParityLogCommonData(item->common);
	      FreeParityLogData(item);
	      (wakeFunc)(wakeArg, 0);
	    }
	  else
	    FreeParityLogData(item);
	}
    }
  RF_UNLOCK_MUTEX(raidPtr->regionInfo[regionID].mutex);
  if (rf_parityLogDebug)
    printf("[exiting ParityLogAppend]\n");
  return(0);
}


void rf_EnableParityLogging(RF_Raid_t *raidPtr)
{
  int regionID;

  for (regionID = 0; regionID < rf_numParityRegions; regionID++) {
    RF_LOCK_MUTEX(raidPtr->regionInfo[regionID].mutex);
    raidPtr->regionInfo[regionID].loggingEnabled = RF_TRUE;
    RF_UNLOCK_MUTEX(raidPtr->regionInfo[regionID].mutex);
  }
  if (rf_parityLogDebug)
    printf("[parity logging enabled]\n");
}

#endif /* RF_INCLUDE_PARITYLOGGING > 0 */
