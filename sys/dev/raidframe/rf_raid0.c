/*	$OpenBSD: rf_raid0.c,v 1.1 1999/01/11 14:29:41 niklas Exp $	*/
/*	$NetBSD: rf_raid0.c,v 1.1 1998/11/13 04:20:33 oster Exp $	*/
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

/***************************************
 *
 * rf_raid0.c -- implements RAID Level 0
 *
 ***************************************/

/*
 * :  
 * Log: rf_raid0.c,v 
 * Revision 1.24  1996/07/31 16:56:18  jimz
 * dataBytesPerStripe, sectorsPerDisk init arch-indep.
 *
 * Revision 1.23  1996/07/27  23:36:08  jimz
 * Solaris port of simulator
 *
 * Revision 1.22  1996/07/18  22:57:14  jimz
 * port simulator to AIX
 *
 * Revision 1.21  1996/06/19  22:07:34  jimz
 * added parity verify
 *
 * Revision 1.20  1996/06/17  14:38:33  jimz
 * properly #if out RF_DEMO code
 * fix bug in MakeConfig that was causing weird behavior
 * in configuration routines (config was not zeroed at start)
 * clean up genplot handling of stacks
 *
 * Revision 1.19  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.18  1996/06/07  22:26:27  jimz
 * type-ify which_ru (RF_ReconUnitNum_t)
 *
 * Revision 1.17  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.16  1996/06/03  23:28:26  jimz
 * more bugfixes
 * check in tree to sync for IPDS runs with current bugfixes
 * there still may be a problem with threads in the script test
 * getting I/Os stuck- not trivially reproducible (runs ~50 times
 * in a row without getting stuck)
 *
 * Revision 1.15  1996/05/31  22:26:54  jimz
 * fix a lot of mapping problems, memory allocation problems
 * found some weird lock issues, fixed 'em
 * more code cleanup
 *
 * Revision 1.14  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.13  1996/05/24  22:17:04  jimz
 * continue code + namespace cleanup
 * typed a bunch of flags
 *
 * Revision 1.12  1996/05/24  01:59:45  jimz
 * another checkpoint in code cleanup for release
 * time to sync kernel tree
 *
 * Revision 1.11  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.10  1996/05/03  19:37:32  wvcii
 * moved dag creation routines to dag library
 *
 * Revision 1.9  1995/12/12  18:10:06  jimz
 * MIN -> RF_MIN, MAX -> RF_MAX, ASSERT -> RF_ASSERT
 * fix 80-column brain damage in comments
 *
 * Revision 1.8  1995/12/06  15:06:36  root
 * added copyright info
 *
 * Revision 1.7  1995/11/17  18:57:15  wvcii
 * added prototypint to MapParity
 *
 * Revision 1.6  1995/11/16  13:53:51  wvcii
 * fixed bug in CreateRAID0WriteDAG prototype
 *
 * Revision 1.5  1995/11/07  15:22:01  wvcii
 * changed RAID0DagSelect prototype
 * function no longer generates numHdrSucc, numTermAnt
 *
 * Revision 1.4  1995/06/23  13:39:17  robby
 * updeated to prototypes in rf_layout.h
 *
 */

#include "rf_types.h"
#include "rf_raid.h"
#include "rf_raid0.h"
#include "rf_dag.h"
#include "rf_dagffrd.h"
#include "rf_dagffwr.h"
#include "rf_dagutils.h"
#include "rf_dagfuncs.h"
#include "rf_threadid.h"
#include "rf_general.h"
#include "rf_configure.h"
#include "rf_parityscan.h"

typedef struct RF_Raid0ConfigInfo_s {
  RF_RowCol_t  *stripeIdentifier;
} RF_Raid0ConfigInfo_t;

int rf_ConfigureRAID0(
  RF_ShutdownList_t  **listp,
  RF_Raid_t           *raidPtr,
  RF_Config_t         *cfgPtr)
{
  RF_RaidLayout_t *layoutPtr = &raidPtr->Layout;
  RF_Raid0ConfigInfo_t *info;
  RF_RowCol_t i;

  /* create a RAID level 0 configuration structure */
  RF_MallocAndAdd(info, sizeof(RF_Raid0ConfigInfo_t), (RF_Raid0ConfigInfo_t *), raidPtr->cleanupList);
  if (info == NULL)
    return(ENOMEM);
  layoutPtr->layoutSpecificInfo = (void *)info;

  RF_MallocAndAdd(info->stripeIdentifier, raidPtr->numCol * sizeof(RF_RowCol_t), (RF_RowCol_t *), raidPtr->cleanupList);
  if (info->stripeIdentifier == NULL)
    return(ENOMEM);
  for (i=0; i<raidPtr->numCol; i++)
    info->stripeIdentifier[i] = i;

  RF_ASSERT(raidPtr->numRow == 1);
  raidPtr->totalSectors = layoutPtr->stripeUnitsPerDisk * raidPtr->numCol * layoutPtr->sectorsPerStripeUnit;
  layoutPtr->numStripe = layoutPtr->stripeUnitsPerDisk;
  layoutPtr->dataSectorsPerStripe = raidPtr->numCol * layoutPtr->sectorsPerStripeUnit;
  layoutPtr->bytesPerStripeUnit = layoutPtr->sectorsPerStripeUnit << raidPtr->logBytesPerSector;
  layoutPtr->numDataCol = raidPtr->numCol;
  layoutPtr->numParityCol = 0;
  return(0);
}

void rf_MapSectorRAID0(
  RF_Raid_t         *raidPtr,
  RF_RaidAddr_t      raidSector,
  RF_RowCol_t       *row,
  RF_RowCol_t       *col,
  RF_SectorNum_t    *diskSector,
  int                remap)
{
  RF_StripeNum_t SUID = raidSector / raidPtr->Layout.sectorsPerStripeUnit;
  *row = 0;
  *col = SUID % raidPtr->numCol;
  *diskSector = (SUID / raidPtr->numCol) * raidPtr->Layout.sectorsPerStripeUnit +
    (raidSector % raidPtr->Layout.sectorsPerStripeUnit);
}

void rf_MapParityRAID0(
  RF_Raid_t       *raidPtr,
  RF_RaidAddr_t    raidSector,
  RF_RowCol_t     *row,
  RF_RowCol_t     *col,
  RF_SectorNum_t  *diskSector,
  int              remap)
{
  *row = *col = 0;
  *diskSector = 0;
}

void rf_IdentifyStripeRAID0(
  RF_Raid_t        *raidPtr,
  RF_RaidAddr_t     addr,
  RF_RowCol_t     **diskids,
  RF_RowCol_t      *outRow)
{
  RF_Raid0ConfigInfo_t *info;

  info = raidPtr->Layout.layoutSpecificInfo;
  *diskids = info->stripeIdentifier;
  *outRow = 0;
}

void rf_MapSIDToPSIDRAID0(
  RF_RaidLayout_t    *layoutPtr,
  RF_StripeNum_t      stripeID,
  RF_StripeNum_t     *psID,
  RF_ReconUnitNum_t  *which_ru)
{
  *which_ru = 0;
  *psID = stripeID;
}

void rf_RAID0DagSelect(
  RF_Raid_t             *raidPtr,
  RF_IoType_t            type,
  RF_AccessStripeMap_t  *asmap,
  RF_VoidFuncPtr        *createFunc)
{
  *createFunc = ((type == RF_IO_TYPE_READ) ?
    (RF_VoidFuncPtr)rf_CreateFaultFreeReadDAG : (RF_VoidFuncPtr)rf_CreateRAID0WriteDAG);
}

int rf_VerifyParityRAID0(
  RF_Raid_t             *raidPtr,
  RF_RaidAddr_t          raidAddr,
  RF_PhysDiskAddr_t     *parityPDA,
  int                    correct_it,
  RF_RaidAccessFlags_t   flags)
{
  /*
   * No parity is always okay.
   */
  return(RF_PARITY_OKAY);
}
