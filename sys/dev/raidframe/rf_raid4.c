/*	$OpenBSD: rf_raid4.c,v 1.1 1999/01/11 14:29:43 niklas Exp $	*/
/*	$NetBSD: rf_raid4.c,v 1.1 1998/11/13 04:20:33 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Rachad Youssef
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
 * rf_raid4.c -- implements RAID Level 4
 *
 ***************************************/

/*
 * :  
 * Log: rf_raid4.c,v 
 * Revision 1.24  1996/07/31 16:56:18  jimz
 * dataBytesPerStripe, sectorsPerDisk init arch-indep.
 *
 * Revision 1.23  1996/07/18  22:57:14  jimz
 * port simulator to AIX
 *
 * Revision 1.22  1996/07/13  00:00:59  jimz
 * sanitized generalized reconstruction architecture
 * cleaned up head sep, rbuf problems
 *
 * Revision 1.21  1996/06/11  08:54:27  jimz
 * improved error-checking at configuration time
 *
 * Revision 1.20  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.19  1996/06/07  22:26:27  jimz
 * type-ify which_ru (RF_ReconUnitNum_t)
 *
 * Revision 1.18  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.17  1996/06/03  23:28:26  jimz
 * more bugfixes
 * check in tree to sync for IPDS runs with current bugfixes
 * there still may be a problem with threads in the script test
 * getting I/Os stuck- not trivially reproducible (runs ~50 times
 * in a row without getting stuck)
 *
 * Revision 1.16  1996/05/31  22:26:54  jimz
 * fix a lot of mapping problems, memory allocation problems
 * found some weird lock issues, fixed 'em
 * more code cleanup
 *
 * Revision 1.15  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.14  1996/05/24  01:59:45  jimz
 * another checkpoint in code cleanup for release
 * time to sync kernel tree
 *
 * Revision 1.13  1996/05/23  00:33:23  jimz
 * code cleanup: move all debug decls to rf_options.c, all extern
 * debug decls to rf_options.h, all debug vars preceded by rf_
 *
 * Revision 1.12  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.11  1996/05/03  19:39:41  wvcii
 * added includes for dag library
 *
 * Revision 1.10  1995/12/12  18:10:06  jimz
 * MIN -> RF_MIN, MAX -> RF_MAX, ASSERT -> RF_ASSERT
 * fix 80-column brain damage in comments
 *
 * Revision 1.9  1995/12/06  15:02:46  root
 * added copyright info
 *
 * Revision 1.8  1995/11/17  18:57:32  wvcii
 * added prototyping to MapParity
 *
 * Revision 1.7  1995/06/23  13:38:58  robby
 * updeated to prototypes in rf_layout.h
 *
 */

#include "rf_raid.h"
#include "rf_dag.h"
#include "rf_dagutils.h"
#include "rf_dagfuncs.h"
#include "rf_dagffrd.h"
#include "rf_dagffwr.h"
#include "rf_dagdegrd.h"
#include "rf_dagdegwr.h"
#include "rf_threadid.h"
#include "rf_raid4.h"
#include "rf_general.h"

typedef struct RF_Raid4ConfigInfo_s {
  RF_RowCol_t  *stripeIdentifier;               /* filled in at config time & used by IdentifyStripe */
} RF_Raid4ConfigInfo_t;



int rf_ConfigureRAID4(
  RF_ShutdownList_t  **listp,
  RF_Raid_t           *raidPtr,
  RF_Config_t         *cfgPtr)
{
  RF_RaidLayout_t *layoutPtr = &raidPtr->Layout;
  RF_Raid4ConfigInfo_t *info;
  int i;

  /* create a RAID level 4 configuration structure ... */
  RF_MallocAndAdd(info, sizeof(RF_Raid4ConfigInfo_t), (RF_Raid4ConfigInfo_t *), raidPtr->cleanupList);
  if (info == NULL)
    return(ENOMEM);
  layoutPtr->layoutSpecificInfo = (void *) info;

  /* ... and fill it in. */
  RF_MallocAndAdd(info->stripeIdentifier, raidPtr->numCol * sizeof(RF_RowCol_t), (RF_RowCol_t *), raidPtr->cleanupList);
  if (info->stripeIdentifier == NULL)
    return(ENOMEM);
  for (i=0; i<raidPtr->numCol; i++)
    info->stripeIdentifier[i] = i;

  RF_ASSERT(raidPtr->numRow == 1);

  /* fill in the remaining layout parameters */
  layoutPtr->numStripe = layoutPtr->stripeUnitsPerDisk;
  layoutPtr->bytesPerStripeUnit = layoutPtr->sectorsPerStripeUnit << raidPtr->logBytesPerSector;
  layoutPtr->numDataCol = raidPtr->numCol-1;
  layoutPtr->dataSectorsPerStripe = layoutPtr->numDataCol * layoutPtr->sectorsPerStripeUnit;
  layoutPtr->numParityCol = 1;
  raidPtr->totalSectors = layoutPtr->stripeUnitsPerDisk * layoutPtr->numDataCol * layoutPtr->sectorsPerStripeUnit;

  return(0);
}

int rf_GetDefaultNumFloatingReconBuffersRAID4(RF_Raid_t *raidPtr)
{
  return(20);
}

RF_HeadSepLimit_t rf_GetDefaultHeadSepLimitRAID4(RF_Raid_t *raidPtr)
{
  return(20);
}

void rf_MapSectorRAID4(
  RF_Raid_t         *raidPtr,
  RF_RaidAddr_t      raidSector,
  RF_RowCol_t       *row,
  RF_RowCol_t       *col,
  RF_SectorNum_t    *diskSector,
  int                remap)
{
  RF_StripeNum_t SUID = raidSector / raidPtr->Layout.sectorsPerStripeUnit;
  *row = 0;
  *col = SUID % raidPtr->Layout.numDataCol;
  *diskSector = (SUID / (raidPtr->Layout.numDataCol)) * raidPtr->Layout.sectorsPerStripeUnit +
    (raidSector % raidPtr->Layout.sectorsPerStripeUnit);
}

void rf_MapParityRAID4(
  RF_Raid_t       *raidPtr,
  RF_RaidAddr_t    raidSector,
  RF_RowCol_t     *row,
  RF_RowCol_t     *col,
  RF_SectorNum_t  *diskSector,
  int              remap)
{
  RF_StripeNum_t SUID = raidSector / raidPtr->Layout.sectorsPerStripeUnit;
  
  *row = 0;
  *col = raidPtr->Layout.numDataCol;
  *diskSector =(SUID / (raidPtr->Layout.numDataCol)) * raidPtr->Layout.sectorsPerStripeUnit +
    (raidSector % raidPtr->Layout.sectorsPerStripeUnit);
}

void rf_IdentifyStripeRAID4(
  RF_Raid_t        *raidPtr,
  RF_RaidAddr_t     addr,
  RF_RowCol_t     **diskids,
  RF_RowCol_t      *outRow)
{
  RF_Raid4ConfigInfo_t *info = raidPtr->Layout.layoutSpecificInfo;
  
  *outRow = 0;
  *diskids = info->stripeIdentifier;
}

void rf_MapSIDToPSIDRAID4(
  RF_RaidLayout_t    *layoutPtr,
  RF_StripeNum_t      stripeID,
  RF_StripeNum_t     *psID,
  RF_ReconUnitNum_t  *which_ru)
{
  *which_ru = 0;
  *psID = stripeID;
}
