/*	$OpenBSD: rf_raid5.c,v 1.1 1999/01/11 14:29:43 niklas Exp $	*/
/*	$NetBSD: rf_raid5.c,v 1.1 1998/11/13 04:20:33 oster Exp $	*/
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

/******************************************************************************
 *
 * rf_raid5.c -- implements RAID Level 5
 *
 *****************************************************************************/

/*
 * :  
 * Log: rf_raid5.c,v 
 * Revision 1.26  1996/11/05 21:10:40  jimz
 * failed pda generalization
 *
 * Revision 1.25  1996/07/31  16:56:18  jimz
 * dataBytesPerStripe, sectorsPerDisk init arch-indep.
 *
 * Revision 1.24  1996/07/18  22:57:14  jimz
 * port simulator to AIX
 *
 * Revision 1.23  1996/07/13  00:00:59  jimz
 * sanitized generalized reconstruction architecture
 * cleaned up head sep, rbuf problems
 *
 * Revision 1.22  1996/06/11  08:54:27  jimz
 * improved error-checking at configuration time
 *
 * Revision 1.21  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.20  1996/06/07  22:26:27  jimz
 * type-ify which_ru (RF_ReconUnitNum_t)
 *
 * Revision 1.19  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.18  1996/06/05  18:06:02  jimz
 * Major code cleanup. The Great Renaming is now done.
 * Better modularity. Better typing. Fixed a bunch of
 * synchronization bugs. Made a lot of global stuff
 * per-desc or per-array. Removed dead code.
 *
 * Revision 1.17  1996/06/03  23:28:26  jimz
 * more bugfixes
 * check in tree to sync for IPDS runs with current bugfixes
 * there still may be a problem with threads in the script test
 * getting I/Os stuck- not trivially reproducible (runs ~50 times
 * in a row without getting stuck)
 *
 * Revision 1.16  1996/06/02  17:31:48  jimz
 * Moved a lot of global stuff into array structure, where it belongs.
 * Fixed up paritylogging, pss modules in this manner. Some general
 * code cleanup. Removed lots of dead code, some dead files.
 *
 * Revision 1.15  1996/05/31  22:26:54  jimz
 * fix a lot of mapping problems, memory allocation problems
 * found some weird lock issues, fixed 'em
 * more code cleanup
 *
 * Revision 1.14  1996/05/30  23:22:16  jimz
 * bugfixes of serialization, timing problems
 * more cleanup
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
 * Revision 1.11  1996/05/24  01:59:45  jimz
 * another checkpoint in code cleanup for release
 * time to sync kernel tree
 *
 * Revision 1.10  1996/05/23  00:33:23  jimz
 * code cleanup: move all debug decls to rf_options.c, all extern
 * debug decls to rf_options.h, all debug vars preceded by rf_
 *
 * Revision 1.9  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.8  1996/05/03  19:38:58  wvcii
 * moved dag creation routines to dag library
 *
 * Revision 1.7  1995/12/12  18:10:06  jimz
 * MIN -> RF_MIN, MAX -> RF_MAX, ASSERT -> RF_ASSERT
 * fix 80-column brain damage in comments
 *
 * Revision 1.6  1995/12/06  15:04:28  root
 * added copyright info
 *
 * Revision 1.5  1995/11/17  18:59:41  wvcii
 * added prototyping to MapParity
 *
 * Revision 1.4  1995/06/23  13:38:21  robby
 * updeated to prototypes in rf_layout.h
 *
 */

#include "rf_types.h"
#include "rf_raid.h"
#include "rf_raid5.h"
#include "rf_dag.h"
#include "rf_dagffrd.h"
#include "rf_dagffwr.h"
#include "rf_dagdegrd.h"
#include "rf_dagdegwr.h"
#include "rf_dagutils.h"
#include "rf_threadid.h"
#include "rf_general.h"
#include "rf_map.h"
#include "rf_utils.h"

typedef struct RF_Raid5ConfigInfo_s {
  RF_RowCol_t  **stripeIdentifier;    /* filled in at config time and used by IdentifyStripe */
} RF_Raid5ConfigInfo_t;

int rf_ConfigureRAID5(
  RF_ShutdownList_t  **listp,
  RF_Raid_t           *raidPtr,
  RF_Config_t         *cfgPtr)
{
  RF_RaidLayout_t *layoutPtr = &raidPtr->Layout;
  RF_Raid5ConfigInfo_t *info;
  RF_RowCol_t i, j, startdisk;
  
  /* create a RAID level 5 configuration structure */
  RF_MallocAndAdd(info, sizeof(RF_Raid5ConfigInfo_t), (RF_Raid5ConfigInfo_t *), raidPtr->cleanupList);
  if (info == NULL)
    return(ENOMEM);
  layoutPtr->layoutSpecificInfo = (void *) info;

  RF_ASSERT(raidPtr->numRow == 1);

  /* the stripe identifier must identify the disks in each stripe,
   * IN THE ORDER THAT THEY APPEAR IN THE STRIPE.
   */
  info->stripeIdentifier = rf_make_2d_array(raidPtr->numCol, raidPtr->numCol, raidPtr->cleanupList);
  if (info->stripeIdentifier == NULL)
    return(ENOMEM);
  startdisk = 0;
  for (i=0; i<raidPtr->numCol; i++) {
    for (j=0; j<raidPtr->numCol; j++) {
      info->stripeIdentifier[i][j] = (startdisk + j) % raidPtr->numCol;
    }
    if ((--startdisk) < 0) startdisk = raidPtr->numCol-1;
  }

  /* fill in the remaining layout parameters */
  layoutPtr->numStripe = layoutPtr->stripeUnitsPerDisk;
  layoutPtr->bytesPerStripeUnit = layoutPtr->sectorsPerStripeUnit << raidPtr->logBytesPerSector;
  layoutPtr->numDataCol = raidPtr->numCol-1;
  layoutPtr->dataSectorsPerStripe = layoutPtr->numDataCol * layoutPtr->sectorsPerStripeUnit;
  layoutPtr->numParityCol = 1;
  layoutPtr->dataStripeUnitsPerDisk = layoutPtr->stripeUnitsPerDisk;

  raidPtr->totalSectors = layoutPtr->stripeUnitsPerDisk * layoutPtr->numDataCol * layoutPtr->sectorsPerStripeUnit;

  return(0);
}

int rf_GetDefaultNumFloatingReconBuffersRAID5(RF_Raid_t *raidPtr)
{
  return(20);
}

RF_HeadSepLimit_t rf_GetDefaultHeadSepLimitRAID5(RF_Raid_t *raidPtr)
{
  return(10);
}

#if !defined(__NetBSD__) && !defined(__OpenBSD__) && !defined(_KERNEL)
/* not currently used */
int rf_ShutdownRAID5(RF_Raid_t *raidPtr)
{
	return(0);
}
#endif

void rf_MapSectorRAID5(
  RF_Raid_t         *raidPtr,
  RF_RaidAddr_t      raidSector,
  RF_RowCol_t       *row,
  RF_RowCol_t       *col,
  RF_SectorNum_t    *diskSector,
  int                remap)
{
  RF_StripeNum_t SUID = raidSector / raidPtr->Layout.sectorsPerStripeUnit;
  *row = 0;
  *col = (SUID % raidPtr->numCol);
  *diskSector = (SUID / (raidPtr->Layout.numDataCol)) * raidPtr->Layout.sectorsPerStripeUnit +
    (raidSector % raidPtr->Layout.sectorsPerStripeUnit);
}

void rf_MapParityRAID5(
  RF_Raid_t       *raidPtr,
  RF_RaidAddr_t    raidSector,
  RF_RowCol_t     *row,
  RF_RowCol_t     *col,
  RF_SectorNum_t  *diskSector,
  int              remap)
{
  RF_StripeNum_t SUID = raidSector / raidPtr->Layout.sectorsPerStripeUnit;
  
  *row = 0;
  *col = raidPtr->Layout.numDataCol-(SUID/raidPtr->Layout.numDataCol)%raidPtr->numCol;
  *diskSector =(SUID / (raidPtr->Layout.numDataCol)) * raidPtr->Layout.sectorsPerStripeUnit +
    (raidSector % raidPtr->Layout.sectorsPerStripeUnit);
}

void rf_IdentifyStripeRAID5(
  RF_Raid_t        *raidPtr,
  RF_RaidAddr_t     addr,
  RF_RowCol_t     **diskids,
  RF_RowCol_t      *outRow)
{
  RF_StripeNum_t stripeID = rf_RaidAddressToStripeID(&raidPtr->Layout, addr);
  RF_Raid5ConfigInfo_t *info = (RF_Raid5ConfigInfo_t *) raidPtr->Layout.layoutSpecificInfo;

  *outRow = 0;
  *diskids = info->stripeIdentifier[ stripeID % raidPtr->numCol ];
}

void rf_MapSIDToPSIDRAID5(
  RF_RaidLayout_t    *layoutPtr,
  RF_StripeNum_t      stripeID,
  RF_StripeNum_t     *psID,
  RF_ReconUnitNum_t  *which_ru)
{
  *which_ru = 0;
  *psID = stripeID;
}

/* select an algorithm for performing an access.  Returns two pointers,
 * one to a function that will return information about the DAG, and
 * another to a function that will create the dag.
 */
void rf_RaidFiveDagSelect(
  RF_Raid_t             *raidPtr,
  RF_IoType_t            type,
  RF_AccessStripeMap_t  *asmap,
  RF_VoidFuncPtr        *createFunc)
{
  RF_RaidLayout_t *layoutPtr = &(raidPtr->Layout);
  RF_PhysDiskAddr_t *failedPDA=NULL;
  RF_RowCol_t frow, fcol;
  RF_RowStatus_t rstat;
  int prior_recon;
  int tid;

  RF_ASSERT(RF_IO_IS_R_OR_W(type));

  if (asmap->numDataFailed + asmap->numParityFailed > 1) {
    RF_ERRORMSG("Multiple disks failed in a single group!  Aborting I/O operation.\n");
    /* *infoFunc = */ *createFunc = NULL;
    return;
  } else if (asmap->numDataFailed + asmap->numParityFailed == 1) {
    
    /* if under recon & already reconstructed, redirect the access to the spare drive 
     * and eliminate the failure indication 
     */
    failedPDA = asmap->failedPDAs[0];
    frow = failedPDA->row; fcol = failedPDA->col;
    rstat = raidPtr->status[failedPDA->row];
    prior_recon = (rstat == rf_rs_reconfigured) || (
      (rstat == rf_rs_reconstructing) ?
      rf_CheckRUReconstructed(raidPtr->reconControl[frow]->reconMap, failedPDA->startSector) : 0
      );
    if (prior_recon) {
      RF_RowCol_t or = failedPDA->row,oc=failedPDA->col;
      RF_SectorNum_t oo=failedPDA->startSector;

      if (layoutPtr->map->flags & RF_DISTRIBUTE_SPARE) {         /* redirect to dist spare space */

	if (failedPDA == asmap->parityInfo) {

	  /* parity has failed */
	  (layoutPtr->map->MapParity)(raidPtr, failedPDA->raidAddress, &failedPDA->row, 
				      &failedPDA->col, &failedPDA->startSector, RF_REMAP);

	  if (asmap->parityInfo->next) {				/* redir 2nd component, if any */
	    RF_PhysDiskAddr_t *p = asmap->parityInfo->next;
	    RF_SectorNum_t SUoffs = p->startSector % layoutPtr->sectorsPerStripeUnit;
	    p->row = failedPDA->row;
	    p->col = failedPDA->col;
	    p->startSector = rf_RaidAddressOfPrevStripeUnitBoundary(layoutPtr, failedPDA->startSector) +
			     SUoffs;  	/* cheating:  startSector is not really a RAID address */
	  }

	} else if (asmap->parityInfo->next && failedPDA == asmap->parityInfo->next) {
	  RF_ASSERT(0);  		/* should not ever happen */
	} else {

	  /* data has failed */
	  (layoutPtr->map->MapSector)(raidPtr, failedPDA->raidAddress, &failedPDA->row, 
				      &failedPDA->col, &failedPDA->startSector, RF_REMAP);

	}

      } else {                                                 /* redirect to dedicated spare space */
	
	failedPDA->row = raidPtr->Disks[frow][fcol].spareRow;
	failedPDA->col = raidPtr->Disks[frow][fcol].spareCol;
	
	/* the parity may have two distinct components, both of which may need to be redirected */
	if (asmap->parityInfo->next) {
	  if (failedPDA == asmap->parityInfo) {
	    failedPDA->next->row = failedPDA->row;
	    failedPDA->next->col = failedPDA->col;
	  } else if (failedPDA == asmap->parityInfo->next) {    /* paranoid:  should never occur */
	    asmap->parityInfo->row = failedPDA->row;
	    asmap->parityInfo->col = failedPDA->col;
	  }
	}
      }

      RF_ASSERT(failedPDA->col != -1);
       
      if (rf_dagDebug || rf_mapDebug) {
	rf_get_threadid(tid);
	printf("[%d] Redirected type '%c' r %d c %d o %ld -> r %d c %d o %ld\n",
	       tid,type,or,oc,(long)oo,failedPDA->row,failedPDA->col,
	       (long)failedPDA->startSector);
      }
      
      asmap->numDataFailed = asmap->numParityFailed = 0;
    }

  }

  /* all dags begin/end with block/unblock node
   * therefore, hdrSucc & termAnt counts should always be 1
   * also, these counts should not be visible outside dag creation routines - 
   * manipulating the counts here should be removed */
  if (type == RF_IO_TYPE_READ) {
    if (asmap->numDataFailed == 0)
      *createFunc = (RF_VoidFuncPtr)rf_CreateFaultFreeReadDAG;
    else
      *createFunc = (RF_VoidFuncPtr)rf_CreateRaidFiveDegradedReadDAG;
  } else {

    
    /* if mirroring, always use large writes.  If the access requires two
     * distinct parity updates, always do a small write.  If the stripe
     * contains a failure but the access does not, do a small write.
     * The first conditional (numStripeUnitsAccessed <= numDataCol/2) uses a
     * less-than-or-equal rather than just a less-than because when G is 3
     * or 4, numDataCol/2 is 1, and I want single-stripe-unit updates to use
     * just one disk.
     */
    if ( (asmap->numDataFailed + asmap->numParityFailed) == 0) {
      if (rf_suppressLocksAndLargeWrites ||
	  (((asmap->numStripeUnitsAccessed <= (layoutPtr->numDataCol / 2)) && (layoutPtr->numDataCol!=1)) ||
	   (asmap->parityInfo->next!=NULL) || rf_CheckStripeForFailures(raidPtr, asmap))) {
	*createFunc = (RF_VoidFuncPtr)rf_CreateSmallWriteDAG;
      } 
      else
	*createFunc = (RF_VoidFuncPtr)rf_CreateLargeWriteDAG;
    }
    else {
      if (asmap->numParityFailed == 1)
	*createFunc = (RF_VoidFuncPtr)rf_CreateNonRedundantWriteDAG;
      else
	if (asmap->numStripeUnitsAccessed != 1 && failedPDA->numSector != layoutPtr->sectorsPerStripeUnit)
	  *createFunc = NULL;
	else
	  *createFunc = (RF_VoidFuncPtr)rf_CreateDegradedWriteDAG;
    }
  }
}
