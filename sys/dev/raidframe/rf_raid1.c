/*	$OpenBSD: rf_raid1.c,v 1.1 1999/01/11 14:29:42 niklas Exp $	*/
/*	$NetBSD: rf_raid1.c,v 1.1 1998/11/13 04:20:33 oster Exp $	*/
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

/*****************************************************************************
 *
 * rf_raid1.c -- implements RAID Level 1
 *
 *****************************************************************************/

/*
 * :  
 * Log: rf_raid1.c,v 
 * Revision 1.46  1996/11/05 21:10:40  jimz
 * failed pda generalization
 *
 * Revision 1.45  1996/07/31  16:56:18  jimz
 * dataBytesPerStripe, sectorsPerDisk init arch-indep.
 *
 * Revision 1.44  1996/07/30  03:06:43  jimz
 * get rid of extra rf_threadid.h include
 *
 * Revision 1.43  1996/07/27  23:36:08  jimz
 * Solaris port of simulator
 *
 * Revision 1.42  1996/07/22  19:52:16  jimz
 * switched node params to RF_DagParam_t, a union of
 * a 64-bit int and a void *, for better portability
 * attempted hpux port, but failed partway through for
 * lack of a single C compiler capable of compiling all
 * source files
 *
 * Revision 1.41  1996/07/18  22:57:14  jimz
 * port simulator to AIX
 *
 * Revision 1.40  1996/07/17  14:31:19  jimz
 * minor cleanup for readability
 *
 * Revision 1.39  1996/07/15  17:22:18  jimz
 * nit-pick code cleanup
 * resolve stdlib problems on DEC OSF
 *
 * Revision 1.38  1996/07/15  02:56:31  jimz
 * fixed dag selection to deal with failed + recon to spare disks
 * enhanced recon, parity check debugging
 *
 * Revision 1.37  1996/07/13  00:00:59  jimz
 * sanitized generalized reconstruction architecture
 * cleaned up head sep, rbuf problems
 *
 * Revision 1.36  1996/07/11  19:08:00  jimz
 * generalize reconstruction mechanism
 * allow raid1 reconstructs via copyback (done with array
 * quiesced, not online, therefore not disk-directed)
 *
 * Revision 1.35  1996/07/10  23:01:24  jimz
 * Better commenting of VerifyParity (for posterity)
 *
 * Revision 1.34  1996/07/10  22:29:45  jimz
 * VerifyParityRAID1: corrected return values for stripes in degraded mode
 *
 * Revision 1.33  1996/07/10  16:05:39  jimz
 * fixed a couple minor bugs in VerifyParityRAID1
 * added code to correct bad RAID1 parity
 *
 * Revision 1.32  1996/06/20  18:47:04  jimz
 * fix up verification bugs
 *
 * Revision 1.31  1996/06/20  15:38:59  jimz
 * added parity verification
 * can't correct bad parity yet, but can return pass/fail
 *
 * Revision 1.30  1996/06/19  22:23:01  jimz
 * parity verification is now a layout-configurable thing
 * not all layouts currently support it (correctly, anyway)
 *
 * Revision 1.29  1996/06/11  08:54:27  jimz
 * improved error-checking at configuration time
 *
 * Revision 1.28  1996/06/10  18:25:24  wvcii
 * fixed bug in rf_IdentifyStripeRAID1 - added array initialization
 *
 * Revision 1.27  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.26  1996/06/07  22:26:27  jimz
 * type-ify which_ru (RF_ReconUnitNum_t)
 *
 * Revision 1.25  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.24  1996/06/06  17:29:43  jimz
 * use CreateMirrorIdleReadDAG for mirrored read
 *
 * Revision 1.23  1996/06/03  23:28:26  jimz
 * more bugfixes
 * check in tree to sync for IPDS runs with current bugfixes
 * there still may be a problem with threads in the script test
 * getting I/Os stuck- not trivially reproducible (runs ~50 times
 * in a row without getting stuck)
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
 * Revision 1.19  1996/05/30  11:29:41  jimz
 * Numerous bug fixes. Stripe lock release code disagreed with the taking code
 * about when stripes should be locked (I made it consistent: no parity, no lock)
 * There was a lot of extra serialization of I/Os which I've removed- a lot of
 * it was to calculate values for the cache code, which is no longer with us.
 * More types, function, macro cleanup. Added code to properly quiesce the array
 * on shutdown. Made a lot of stuff array-specific which was (bogusly) general
 * before. Fixed memory allocation, freeing bugs.
 *
 * Revision 1.18  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.17  1996/05/24  22:17:04  jimz
 * continue code + namespace cleanup
 * typed a bunch of flags
 *
 * Revision 1.16  1996/05/24  04:28:55  jimz
 * release cleanup ckpt
 *
 * Revision 1.15  1996/05/24  01:59:45  jimz
 * another checkpoint in code cleanup for release
 * time to sync kernel tree
 *
 * Revision 1.14  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.13  1996/05/03  19:36:22  wvcii
 * moved dag creation routines to dag library
 *
 * Revision 1.12  1996/02/23  01:38:16  amiri
 * removed chained declustering special case in SelectIdleDisk
 *
 * Revision 1.11  1996/02/22  16:47:18  amiri
 * disabled shortest queue optimization for chained declustering
 *
 * Revision 1.10  1995/12/12  18:10:06  jimz
 * MIN -> RF_MIN, MAX -> RF_MAX, ASSERT -> RF_ASSERT
 * fix 80-column brain damage in comments
 *
 * Revision 1.9  1995/12/04  19:21:28  wvcii
 * modified SelectIdleDisk to take a mirror node as a parameter and
 * conditionally swap params 0 (data pda) and 4 (mirror pda).
 * modified CreateRaidOneReadDAG so that it creates the DAG itself
 * as opposed to reusing code in CreateNonredundantDAG.
 *
 * Revision 1.8  1995/11/30  16:07:45  wvcii
 * added copyright info
 *
 * Revision 1.7  1995/11/16  14:46:18  wvcii
 * fixed bugs in mapping and degraded dag creation, added comments
 *
 * Revision 1.6  1995/11/14  22:29:16  wvcii
 * fixed bugs in dag creation
 *
 * Revision 1.5  1995/11/07  15:23:33  wvcii
 * changed RAID1DagSelect prototype
 * function no longer generates numHdrSucc, numTermAnt
 * changed dag creation routines:
 *   term node generated during dag creation
 *   encoded commit nodes, barrier, antecedent types
 *
 * Revision 1.4  1995/10/10  19:09:21  wvcii
 * write dag now handles non-aligned accesses
 *
 * Revision 1.3  1995/10/05  02:32:56  jimz
 * ifdef'd out queue locking for load balancing
 *
 * Revision 1.2  1995/10/04  07:04:40  wvcii
 * reads are now scheduled according to disk queue length.
 * queue length is the sum of number of ios queued in raidframe as well as those at the disk.
 * reads are sent to the disk with the shortest queue.
 * testing against user disks successful, sim & kernel untested.
 *
 * Revision 1.1  1995/10/04  03:53:23  wvcii
 * Initial revision
 *
 *
 */

#include "rf_raid.h"
#include "rf_raid1.h"
#include "rf_dag.h"
#include "rf_dagffrd.h"
#include "rf_dagffwr.h"
#include "rf_dagdegrd.h"
#include "rf_dagutils.h"
#include "rf_dagfuncs.h"
#include "rf_threadid.h"
#include "rf_diskqueue.h"
#include "rf_general.h"
#include "rf_utils.h"
#include "rf_parityscan.h"
#include "rf_mcpair.h"
#include "rf_layout.h"
#include "rf_map.h"
#include "rf_engine.h"
#include "rf_reconbuffer.h"
#include "rf_sys.h"

typedef struct RF_Raid1ConfigInfo_s {
  RF_RowCol_t  **stripeIdentifier;
} RF_Raid1ConfigInfo_t;

/* start of day code specific to RAID level 1 */
int rf_ConfigureRAID1(
  RF_ShutdownList_t  **listp,
  RF_Raid_t           *raidPtr,
  RF_Config_t         *cfgPtr)
{
  RF_RaidLayout_t *layoutPtr = &raidPtr->Layout;
  RF_Raid1ConfigInfo_t *info;
  RF_RowCol_t i;

  /* create a RAID level 1 configuration structure */
  RF_MallocAndAdd(info, sizeof(RF_Raid1ConfigInfo_t), (RF_Raid1ConfigInfo_t *), raidPtr->cleanupList);
  if (info == NULL)
    return(ENOMEM);
  layoutPtr->layoutSpecificInfo = (void *) info;

  /* ... and fill it in. */
  info->stripeIdentifier = rf_make_2d_array(raidPtr->numCol / 2, 2, raidPtr->cleanupList);
  if (info->stripeIdentifier == NULL)
    return(ENOMEM);
  for (i = 0; i < (raidPtr->numCol / 2); i ++) {
    info->stripeIdentifier[i][0] = (2 * i);
    info->stripeIdentifier[i][1] = (2 * i) + 1;
  }

  RF_ASSERT(raidPtr->numRow == 1);

  /* this implementation of RAID level 1 uses one row of numCol disks and allows multiple (numCol / 2)
   * stripes per row.  A stripe consists of a single data unit and a single parity (mirror) unit.
   * stripe id = raidAddr / stripeUnitSize
   */
  raidPtr->totalSectors = layoutPtr->stripeUnitsPerDisk * (raidPtr->numCol / 2) * layoutPtr->sectorsPerStripeUnit;
  layoutPtr->numStripe = layoutPtr->stripeUnitsPerDisk * (raidPtr->numCol / 2);
  layoutPtr->dataSectorsPerStripe = layoutPtr->sectorsPerStripeUnit;
  layoutPtr->bytesPerStripeUnit = layoutPtr->sectorsPerStripeUnit << raidPtr->logBytesPerSector;
  layoutPtr->numDataCol = 1;
  layoutPtr->numParityCol = 1;
  return(0);
}


/* returns the physical disk location of the primary copy in the mirror pair */
void rf_MapSectorRAID1(
  RF_Raid_t         *raidPtr,
  RF_RaidAddr_t      raidSector,
  RF_RowCol_t       *row,
  RF_RowCol_t       *col,
  RF_SectorNum_t    *diskSector,
  int                remap)
{
  RF_StripeNum_t SUID = raidSector / raidPtr->Layout.sectorsPerStripeUnit;
  RF_RowCol_t mirrorPair = SUID % (raidPtr->numCol / 2);

  *row = 0;
  *col = 2 * mirrorPair;
  *diskSector = ((SUID / (raidPtr->numCol / 2)) * raidPtr->Layout.sectorsPerStripeUnit) + (raidSector % raidPtr->Layout.sectorsPerStripeUnit);
}


/* Map Parity
 *
 * returns the physical disk location of the secondary copy in the mirror
 * pair
 */
void rf_MapParityRAID1(
  RF_Raid_t       *raidPtr,
  RF_RaidAddr_t    raidSector,
  RF_RowCol_t     *row,
  RF_RowCol_t     *col,
  RF_SectorNum_t  *diskSector,
  int              remap)
{
  RF_StripeNum_t SUID = raidSector / raidPtr->Layout.sectorsPerStripeUnit;
  RF_RowCol_t mirrorPair = SUID % (raidPtr->numCol / 2);

  *row = 0;
  *col = (2 * mirrorPair) + 1;

  *diskSector = ((SUID / (raidPtr->numCol / 2)) * raidPtr->Layout.sectorsPerStripeUnit) + (raidSector % raidPtr->Layout.sectorsPerStripeUnit);
}


/* IdentifyStripeRAID1
 *
 * returns a list of disks for a given redundancy group
 */
void rf_IdentifyStripeRAID1(
  RF_Raid_t        *raidPtr,
  RF_RaidAddr_t     addr,
  RF_RowCol_t     **diskids,
  RF_RowCol_t      *outRow)
{
  RF_StripeNum_t stripeID = rf_RaidAddressToStripeID(&raidPtr->Layout, addr);
  RF_Raid1ConfigInfo_t *info = raidPtr->Layout.layoutSpecificInfo;
  RF_ASSERT(stripeID >= 0);
  RF_ASSERT(addr >= 0);
  *outRow = 0;
  *diskids = info->stripeIdentifier[ stripeID % (raidPtr->numCol/2)];
  RF_ASSERT(*diskids);
}


/* MapSIDToPSIDRAID1
 *
 * maps a logical stripe to a stripe in the redundant array
 */
void rf_MapSIDToPSIDRAID1(
  RF_RaidLayout_t    *layoutPtr,
  RF_StripeNum_t      stripeID,
  RF_StripeNum_t     *psID,
  RF_ReconUnitNum_t  *which_ru)
{
  *which_ru = 0;
  *psID = stripeID;
}



/******************************************************************************
 * select a graph to perform a single-stripe access
 *
 * Parameters:  raidPtr    - description of the physical array
 *              type       - type of operation (read or write) requested
 *              asmap      - logical & physical addresses for this access
 *              createFunc - name of function to use to create the graph
 *****************************************************************************/

void rf_RAID1DagSelect(
  RF_Raid_t             *raidPtr,
  RF_IoType_t            type,
  RF_AccessStripeMap_t  *asmap,
  RF_VoidFuncPtr        *createFunc)
{
  RF_RowCol_t frow, fcol, or, oc;
  RF_PhysDiskAddr_t *failedPDA;
  int prior_recon, tid;
  RF_RowStatus_t rstat;
  RF_SectorNum_t oo;


  RF_ASSERT(RF_IO_IS_R_OR_W(type));

  if (asmap->numDataFailed + asmap->numParityFailed > 1) {
    RF_ERRORMSG("Multiple disks failed in a single group!  Aborting I/O operation.\n");
    *createFunc = NULL;
    return;
  }

  if (asmap->numDataFailed + asmap->numParityFailed) {
    /*
     * We've got a fault. Re-map to spare space, iff applicable.
     * Shouldn't the arch-independent code do this for us?
     * Anyway, it turns out if we don't do this here, then when
     * we're reconstructing, writes go only to the surviving
     * original disk, and aren't reflected on the reconstructed
     * spare. Oops. --jimz
     */
    failedPDA = asmap->failedPDAs[0];
    frow = failedPDA->row;
    fcol = failedPDA->col;
    rstat = raidPtr->status[frow];
    prior_recon = (rstat == rf_rs_reconfigured) || (
      (rstat == rf_rs_reconstructing) ?
      rf_CheckRUReconstructed(raidPtr->reconControl[frow]->reconMap, failedPDA->startSector) : 0
      );
    if (prior_recon) {
      or = frow;
      oc = fcol;
      oo = failedPDA->startSector;
      /*
       * If we did distributed sparing, we'd monkey with that here.
       * But we don't, so we'll 
       */
      failedPDA->row = raidPtr->Disks[frow][fcol].spareRow;
      failedPDA->col = raidPtr->Disks[frow][fcol].spareCol;
      /*
       * Redirect other components, iff necessary. This looks
       * pretty suspicious to me, but it's what the raid5
       * DAG select does.
       */
      if (asmap->parityInfo->next) {
        if (failedPDA == asmap->parityInfo) {
          failedPDA->next->row = failedPDA->row;
          failedPDA->next->col = failedPDA->col;
        }
        else {
          if (failedPDA == asmap->parityInfo->next) {
            asmap->parityInfo->row = failedPDA->row;
            asmap->parityInfo->col = failedPDA->col;
          }
        }
      }
      if (rf_dagDebug || rf_mapDebug) {
        rf_get_threadid(tid);
        printf("[%d] Redirected type '%c' r %d c %d o %ld -> r %d c %d o %ld\n",
          tid, type, or, oc, (long)oo, failedPDA->row, failedPDA->col,
          (long)failedPDA->startSector);
      }
      asmap->numDataFailed = asmap->numParityFailed = 0;
    }
  }
  if (type == RF_IO_TYPE_READ) {
    if (asmap->numDataFailed == 0)
      *createFunc = (RF_VoidFuncPtr)rf_CreateMirrorIdleReadDAG;
    else
      *createFunc = (RF_VoidFuncPtr)rf_CreateRaidOneDegradedReadDAG;
  }
  else {
    *createFunc = (RF_VoidFuncPtr)rf_CreateRaidOneWriteDAG;
  }
}

int rf_VerifyParityRAID1(
  RF_Raid_t             *raidPtr,
  RF_RaidAddr_t          raidAddr,
  RF_PhysDiskAddr_t     *parityPDA,
  int                    correct_it,
  RF_RaidAccessFlags_t   flags)
{
  int nbytes, bcount, stripeWidth, ret, i, j, tid=0, nbad, *bbufs;
  RF_DagNode_t *blockNode, *unblockNode, *wrBlock;
  RF_DagHeader_t *rd_dag_h, *wr_dag_h;
  RF_AccessStripeMapHeader_t *asm_h;
  RF_AllocListElem_t *allocList;
  RF_AccTraceEntry_t tracerec;
  RF_ReconUnitNum_t which_ru;
  RF_RaidLayout_t *layoutPtr;
  RF_AccessStripeMap_t *aasm;
  RF_SectorCount_t nsector;
  RF_RaidAddr_t startAddr;
  char *buf, *buf1, *buf2;
  RF_PhysDiskAddr_t *pda;
  RF_StripeNum_t psID;
  RF_MCPair_t *mcpair;

  if (rf_verifyParityDebug) {
    rf_get_threadid(tid);
  }

  layoutPtr = &raidPtr->Layout;
  startAddr = rf_RaidAddressOfPrevStripeBoundary(layoutPtr, raidAddr);
  nsector = parityPDA->numSector;
  nbytes = rf_RaidAddressToByte(raidPtr, nsector);
  psID = rf_RaidAddressToParityStripeID(layoutPtr, raidAddr, &which_ru);

  asm_h = NULL;
  rd_dag_h = wr_dag_h = NULL;
  mcpair = NULL;

  ret = RF_PARITY_COULD_NOT_VERIFY;

  rf_MakeAllocList(allocList);
  if (allocList == NULL)
    return(RF_PARITY_COULD_NOT_VERIFY);
  mcpair = rf_AllocMCPair();
  if (mcpair == NULL)
    goto done;
  RF_ASSERT(layoutPtr->numDataCol == layoutPtr->numParityCol);
  stripeWidth = layoutPtr->numDataCol + layoutPtr->numParityCol;
  bcount = nbytes*(layoutPtr->numDataCol + layoutPtr->numParityCol);
  RF_MallocAndAdd(buf, bcount, (char *), allocList);
  if (buf == NULL)
    goto done;
  if (rf_verifyParityDebug) {
    printf("[%d] RAID1 parity verify: buf=%lx bcount=%d (%lx - %lx)\n",
      tid, (long)buf, bcount, (long)buf, (long)buf+bcount);
  }

  /*
   * Generate a DAG which will read the entire stripe- then we can
   * just compare data chunks versus "parity" chunks.
   */

  rd_dag_h = rf_MakeSimpleDAG(raidPtr, stripeWidth, nbytes, buf,
    rf_DiskReadFunc, rf_DiskReadUndoFunc, "Rod", allocList, flags,
    RF_IO_NORMAL_PRIORITY);
  if (rd_dag_h == NULL)
    goto done;
  blockNode = rd_dag_h->succedents[0];
  unblockNode = blockNode->succedents[0]->succedents[0];

  /*
   * Map the access to physical disk addresses (PDAs)- this will
   * get us both a list of data addresses, and "parity" addresses
   * (which are really mirror copies).
   */
  asm_h = rf_MapAccess(raidPtr, startAddr, layoutPtr->dataSectorsPerStripe,
    buf, RF_DONT_REMAP);
  aasm = asm_h->stripeMap;

  buf1 = buf;
  /*
   * Loop through the data blocks, setting up read nodes for each.
   */
  for(pda=aasm->physInfo,i=0;i<layoutPtr->numDataCol;i++,pda=pda->next)
  {
    RF_ASSERT(pda);

    rf_RangeRestrictPDA(raidPtr, parityPDA, pda, 0, 1);

    RF_ASSERT(pda->numSector != 0);
    if (rf_TryToRedirectPDA(raidPtr, pda, 0)) {
      /* cannot verify parity with dead disk */
      goto done;
    }
    pda->bufPtr = buf1;
    blockNode->succedents[i]->params[0].p = pda;
    blockNode->succedents[i]->params[1].p = buf1;
    blockNode->succedents[i]->params[2].v = psID;
    blockNode->succedents[i]->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);
    buf1 += nbytes;
  }
  RF_ASSERT(pda == NULL);
  /*
   * keep i, buf1 running
   *
   * Loop through parity blocks, setting up read nodes for each.
   */
  for(pda=aasm->parityInfo;i<layoutPtr->numDataCol+layoutPtr->numParityCol;i++,pda=pda->next)
  {
    RF_ASSERT(pda);
    rf_RangeRestrictPDA(raidPtr, parityPDA, pda, 0, 1);
    RF_ASSERT(pda->numSector != 0);
    if (rf_TryToRedirectPDA(raidPtr, pda, 0)) {
      /* cannot verify parity with dead disk */
      goto done;
    }
    pda->bufPtr = buf1;
    blockNode->succedents[i]->params[0].p = pda;
    blockNode->succedents[i]->params[1].p = buf1;
    blockNode->succedents[i]->params[2].v = psID;
    blockNode->succedents[i]->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);
    buf1 += nbytes;
  }
  RF_ASSERT(pda == NULL);

  bzero((char *)&tracerec, sizeof(tracerec));
  rd_dag_h->tracerec = &tracerec;

  if (rf_verifyParityDebug > 1) {
    printf("[%d] RAID1 parity verify read dag:\n", tid);
    rf_PrintDAGList(rd_dag_h);
  }

  RF_LOCK_MUTEX(mcpair->mutex);
  mcpair->flag = 0;
  rf_DispatchDAG(rd_dag_h, (void (*)(void *))rf_MCPairWakeupFunc, 
		 (void *)mcpair);
  while (mcpair->flag == 0) {
    RF_WAIT_MCPAIR(mcpair);
  }
  RF_UNLOCK_MUTEX(mcpair->mutex);

  if (rd_dag_h->status != rf_enable) {
    RF_ERRORMSG("Unable to verify raid1 parity: can't read stripe\n");
    ret = RF_PARITY_COULD_NOT_VERIFY;
    goto done;
  }

  /*
   * buf1 is the beginning of the data blocks chunk
   * buf2 is the beginning of the parity blocks chunk
   */
  buf1 = buf;
  buf2 = buf + (nbytes * layoutPtr->numDataCol);
  ret = RF_PARITY_OKAY;
  /*
   * bbufs is "bad bufs"- an array whose entries are the data
   * column numbers where we had miscompares. (That is, column 0
   * and column 1 of the array are mirror copies, and are considered
   * "data column 0" for this purpose).
   */
  RF_MallocAndAdd(bbufs, layoutPtr->numParityCol*sizeof(int), (int *),
    allocList);
  nbad = 0;
  /*
   * Check data vs "parity" (mirror copy).
   */
  for(i=0;i<layoutPtr->numDataCol;i++) {
    if (rf_verifyParityDebug) {
      printf("[%d] RAID1 parity verify %d bytes: i=%d buf1=%lx buf2=%lx buf=%lx\n",
        tid, nbytes, i, (long)buf1, (long)buf2, (long)buf);
    }
    ret = bcmp(buf1, buf2, nbytes);
    if (ret) {
      if (rf_verifyParityDebug > 1) {
        for(j=0;j<nbytes;j++) {
         if (buf1[j] != buf2[j])
           break;
        }
        printf("psid=%ld j=%d\n", (long)psID, j);
        printf("buf1 %02x %02x %02x %02x %02x\n", buf1[0]&0xff,
          buf1[1]&0xff, buf1[2]&0xff, buf1[3]&0xff, buf1[4]&0xff);
        printf("buf2 %02x %02x %02x %02x %02x\n", buf2[0]&0xff,
          buf2[1]&0xff, buf2[2]&0xff, buf2[3]&0xff, buf2[4]&0xff);
      }
      if (rf_verifyParityDebug) {
        printf("[%d] RAID1: found bad parity, i=%d\n", tid, i);
      }
      /*
       * Parity is bad. Keep track of which columns were bad.
       */
      if (bbufs)
        bbufs[nbad] = i;
      nbad++;
      ret = RF_PARITY_BAD;
    }
    buf1 += nbytes;
    buf2 += nbytes;
  }

  if ((ret != RF_PARITY_OKAY) && correct_it) {
    ret = RF_PARITY_COULD_NOT_CORRECT;
    if (rf_verifyParityDebug) {
      printf("[%d] RAID1 parity verify: parity not correct\n", tid);
    }
    if (bbufs == NULL)
      goto done;
    /*
     * Make a DAG with one write node for each bad unit. We'll simply
     * write the contents of the data unit onto the parity unit for
     * correction. (It's possible that the mirror copy was the correct
     * copy, and that we're spooging good data by writing bad over it,
     * but there's no way we can know that.
     */
    wr_dag_h = rf_MakeSimpleDAG(raidPtr, nbad, nbytes, buf,
      rf_DiskWriteFunc, rf_DiskWriteUndoFunc, "Wnp", allocList, flags,
      RF_IO_NORMAL_PRIORITY);
    if (wr_dag_h == NULL)
      goto done;
    wrBlock = wr_dag_h->succedents[0];
    /*
     * Fill in a write node for each bad compare.
     */
    for(i=0;i<nbad;i++) {
      j = i+layoutPtr->numDataCol;
      pda = blockNode->succedents[j]->params[0].p;
      pda->bufPtr = blockNode->succedents[i]->params[1].p;
      wrBlock->succedents[i]->params[0].p = pda;
      wrBlock->succedents[i]->params[1].p = pda->bufPtr;
      wrBlock->succedents[i]->params[2].v = psID;
      wrBlock->succedents[0]->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);
    }
    bzero((char *)&tracerec, sizeof(tracerec));
    wr_dag_h->tracerec = &tracerec;
    if (rf_verifyParityDebug > 1) {
      printf("Parity verify write dag:\n");
      rf_PrintDAGList(wr_dag_h);
    }
    RF_LOCK_MUTEX(mcpair->mutex);
    mcpair->flag = 0;
    /* fire off the write DAG */
    rf_DispatchDAG(wr_dag_h, (void (*)(void *))rf_MCPairWakeupFunc, 
		   (void *)mcpair);
    while (!mcpair->flag) {
      RF_WAIT_COND(mcpair->cond, mcpair->mutex);
    }
    RF_UNLOCK_MUTEX(mcpair->mutex);
    if (wr_dag_h->status != rf_enable) {
      RF_ERRORMSG("Unable to correct RAID1 parity in VerifyParity\n");
      goto done;
    }
    ret = RF_PARITY_CORRECTED;
  }

done:
  /*
   * All done. We might've gotten here without doing part of the function,
   * so cleanup what we have to and return our running status.
   */
  if (asm_h)
    rf_FreeAccessStripeMap(asm_h);
  if (rd_dag_h)
    rf_FreeDAG(rd_dag_h);
  if (wr_dag_h)
    rf_FreeDAG(wr_dag_h);
  if (mcpair)
    rf_FreeMCPair(mcpair);
  rf_FreeAllocList(allocList);
  if (rf_verifyParityDebug) {
    printf("[%d] RAID1 parity verify, returning %d\n", tid, ret);
  }
  return(ret);
}

int rf_SubmitReconBufferRAID1(rbuf, keep_it, use_committed)
  RF_ReconBuffer_t  *rbuf;          /* the recon buffer to submit */
  int                keep_it;       /* whether we can keep this buffer or we have to return it */
  int                use_committed; /* whether to use a committed or an available recon buffer */
{
  RF_ReconParityStripeStatus_t *pssPtr;
  RF_ReconCtrl_t *reconCtrlPtr;
  RF_RaidLayout_t *layoutPtr;
  int tid=0, retcode, created;
  RF_CallbackDesc_t *cb, *p;
  RF_ReconBuffer_t *t;
  RF_Raid_t *raidPtr;
  caddr_t ta;

  retcode = 0;
  created = 0;

  raidPtr = rbuf->raidPtr;
  layoutPtr = &raidPtr->Layout;
  reconCtrlPtr = raidPtr->reconControl[rbuf->row];

  RF_ASSERT(rbuf);
  RF_ASSERT(rbuf->col != reconCtrlPtr->fcol);

  if (rf_reconbufferDebug) {
    rf_get_threadid(tid);
    printf("[%d] RAID1 reconbuffer submission r%d c%d psid %ld ru%d (failed offset %ld)\n",
      tid, rbuf->row, rbuf->col, (long)rbuf->parityStripeID, rbuf->which_ru,
      (long)rbuf->failedDiskSectorOffset);
  }

  if (rf_reconDebug) {
    printf("RAID1 reconbuffer submit psid %ld buf %lx\n", 
	   (long)rbuf->parityStripeID, (long)rbuf->buffer);
    printf("RAID1 psid %ld   %02x %02x %02x %02x %02x\n", 
	   (long)rbuf->parityStripeID,
      rbuf->buffer[0], rbuf->buffer[1], rbuf->buffer[2], rbuf->buffer[3],
      rbuf->buffer[4]);
  }

  RF_LOCK_PSS_MUTEX(raidPtr,rbuf->row,rbuf->parityStripeID);

  RF_LOCK_MUTEX(reconCtrlPtr->rb_mutex);

  pssPtr = rf_LookupRUStatus(raidPtr, reconCtrlPtr->pssTable,
    rbuf->parityStripeID, rbuf->which_ru, RF_PSS_NONE, &created);
  RF_ASSERT(pssPtr); /* if it didn't exist, we wouldn't have gotten an rbuf for it */

  /*
   * Since this is simple mirroring, the first submission for a stripe is also
   * treated as the last.
   */

  t = NULL;
  if (keep_it) {
    if (rf_reconbufferDebug) {
      printf("[%d] RAID1 rbuf submission: keeping rbuf\n", tid);
    }
    t = rbuf;
  }
  else {
    if (use_committed) {
      if (rf_reconbufferDebug) {
        printf("[%d] RAID1 rbuf submission: using committed rbuf\n", tid);
      }
      t = reconCtrlPtr->committedRbufs;
      RF_ASSERT(t);
      reconCtrlPtr->committedRbufs = t->next;
      t->next = NULL;
    }
    else if (reconCtrlPtr->floatingRbufs) {
      if (rf_reconbufferDebug) {
        printf("[%d] RAID1 rbuf submission: using floating rbuf\n", tid);
      }
      t = reconCtrlPtr->floatingRbufs;
      reconCtrlPtr->floatingRbufs = t->next;
      t->next = NULL;
    }
  }
  if (t == NULL) {
    if (rf_reconbufferDebug) {
      printf("[%d] RAID1 rbuf submission: waiting for rbuf\n", tid);
    }
    RF_ASSERT((keep_it == 0) && (use_committed == 0));
    raidPtr->procsInBufWait++;
    if ((raidPtr->procsInBufWait == (raidPtr->numCol-1))
      && (raidPtr->numFullReconBuffers == 0))
    {
      /* ruh-ro */
      RF_ERRORMSG("Buffer wait deadlock\n");
      rf_PrintPSStatusTable(raidPtr, rbuf->row);
      RF_PANIC();
    }
    pssPtr->flags |= RF_PSS_BUFFERWAIT;
    cb = rf_AllocCallbackDesc();
    cb->row = rbuf->row;
    cb->col = rbuf->col;
    cb->callbackArg.v = rbuf->parityStripeID;
    cb->callbackArg2.v = rbuf->which_ru;
    cb->next = NULL;
    if (reconCtrlPtr->bufferWaitList == NULL) {
      /* we are the wait list- lucky us */
      reconCtrlPtr->bufferWaitList = cb;
    }
    else {
      /* append to wait list */
      for(p=reconCtrlPtr->bufferWaitList;p->next;p=p->next);
      p->next = cb;
    }
    retcode = 1;
    goto out;
  }
  if (t != rbuf) {
    t->row = rbuf->row;
    t->col = reconCtrlPtr->fcol;
    t->parityStripeID = rbuf->parityStripeID;
    t->which_ru = rbuf->which_ru;
    t->failedDiskSectorOffset = rbuf->failedDiskSectorOffset;
    t->spRow = rbuf->spRow;
    t->spCol = rbuf->spCol;
    t->spOffset = rbuf->spOffset;
    /* Swap buffers. DANCE! */
    ta = t->buffer;
    t->buffer = rbuf->buffer;
    rbuf->buffer = ta;
  }
  /*
   * Use the rbuf we've been given as the target.
   */
  RF_ASSERT(pssPtr->rbuf == NULL);
  pssPtr->rbuf = t;

  t->count = 1;
  /*
   * Below, we use 1 for numDataCol (which is equal to the count in the
   * previous line), so we'll always be done.
   */
  rf_CheckForFullRbuf(raidPtr, reconCtrlPtr, pssPtr, 1);

out:
  RF_UNLOCK_PSS_MUTEX( raidPtr,rbuf->row,rbuf->parityStripeID);
  RF_UNLOCK_MUTEX( reconCtrlPtr->rb_mutex );
  if (rf_reconbufferDebug) {
    printf("[%d] RAID1 rbuf submission: returning %d\n", tid, retcode);
  }
  return(retcode);
}
