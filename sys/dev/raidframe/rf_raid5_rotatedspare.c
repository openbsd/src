/*	$OpenBSD: rf_raid5_rotatedspare.c,v 1.1 1999/01/11 14:29:44 niklas Exp $	*/
/*	$NetBSD: rf_raid5_rotatedspare.c,v 1.1 1998/11/13 04:20:33 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Khalil Amiri
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

/**************************************************************************
 *
 * rf_raid5_rotated_spare.c -- implements RAID Level 5 with rotated sparing
 *
 **************************************************************************/

/* :  
 * Log: rf_raid5_rotatedspare.c,v 
 * Revision 1.22  1996/07/31 16:56:18  jimz
 * dataBytesPerStripe, sectorsPerDisk init arch-indep.
 *
 * Revision 1.21  1996/07/29  14:05:12  jimz
 * fix numPUs/numRUs confusion (everything is now numRUs)
 * clean up some commenting, return values
 *
 * Revision 1.20  1996/07/18  22:57:14  jimz
 * port simulator to AIX
 *
 * Revision 1.19  1996/07/13  00:00:59  jimz
 * sanitized generalized reconstruction architecture
 * cleaned up head sep, rbuf problems
 *
 * Revision 1.18  1996/06/19  17:53:48  jimz
 * move GetNumSparePUs, InstallSpareTable ops into layout switch
 *
 * Revision 1.17  1996/06/11  08:54:27  jimz
 * improved error-checking at configuration time
 *
 * Revision 1.16  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.15  1996/06/07  22:26:27  jimz
 * type-ify which_ru (RF_ReconUnitNum_t)
 *
 * Revision 1.14  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.13  1996/06/03  23:28:26  jimz
 * more bugfixes
 * check in tree to sync for IPDS runs with current bugfixes
 * there still may be a problem with threads in the script test
 * getting I/Os stuck- not trivially reproducible (runs ~50 times
 * in a row without getting stuck)
 *
 * Revision 1.12  1996/06/02  17:31:48  jimz
 * Moved a lot of global stuff into array structure, where it belongs.
 * Fixed up paritylogging, pss modules in this manner. Some general
 * code cleanup. Removed lots of dead code, some dead files.
 *
 * Revision 1.11  1996/05/31  22:26:54  jimz
 * fix a lot of mapping problems, memory allocation problems
 * found some weird lock issues, fixed 'em
 * more code cleanup
 *
 * Revision 1.10  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.9  1996/05/24  01:59:45  jimz
 * another checkpoint in code cleanup for release
 * time to sync kernel tree
 *
 * Revision 1.8  1996/05/23  00:33:23  jimz
 * code cleanup: move all debug decls to rf_options.c, all extern
 * debug decls to rf_options.h, all debug vars preceded by rf_
 *
 * Revision 1.7  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.6  1996/05/03  19:48:36  wvcii
 * removed include of rf_redstripe.h
 *
 * Revision 1.5  1995/12/12  18:10:06  jimz
 * MIN -> RF_MIN, MAX -> RF_MAX, ASSERT -> RF_ASSERT
 * fix 80-column brain damage in comments
 *
 * Revision 1.4  1995/12/06  15:05:53  root
 * added copyright info
 *
 * Revision 1.3  1995/11/19  21:26:29  amiri
 * Added an assert to make sure numCol >= 3
 *
 * Revision 1.2  1995/11/17  19:03:18  wvcii
 * added prototyping to MapParity
 *
 */

#include "rf_raid.h"
#include "rf_raid5.h"
#include "rf_dag.h"
#include "rf_dagutils.h"
#include "rf_dagfuncs.h"
#include "rf_threadid.h"
#include "rf_general.h"
#include "rf_utils.h"
#include "rf_raid5_rotatedspare.h"

typedef struct RF_Raid5RSConfigInfo_s  {
  RF_RowCol_t  **stripeIdentifier;                    /* filled in at config time & used by IdentifyStripe */
} RF_Raid5RSConfigInfo_t;

int rf_ConfigureRAID5_RS(
  RF_ShutdownList_t  **listp,
  RF_Raid_t           *raidPtr,
  RF_Config_t         *cfgPtr)
{
  RF_RaidLayout_t *layoutPtr = &raidPtr->Layout;
  RF_Raid5RSConfigInfo_t *info;
  RF_RowCol_t i, j, startdisk;

  /* create a RAID level 5 configuration structure */
  RF_MallocAndAdd(info, sizeof(RF_Raid5RSConfigInfo_t), (RF_Raid5RSConfigInfo_t *), raidPtr->cleanupList);
  if (info == NULL)
    return(ENOMEM);
  layoutPtr->layoutSpecificInfo = (void *) info;

  RF_ASSERT(raidPtr->numRow == 1);
  RF_ASSERT(raidPtr->numCol >= 3); 

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
  layoutPtr->numDataCol = raidPtr->numCol-2;
  layoutPtr->dataSectorsPerStripe = layoutPtr->numDataCol * layoutPtr->sectorsPerStripeUnit;
  layoutPtr->numParityCol = 1;
  layoutPtr->dataStripeUnitsPerDisk = layoutPtr->stripeUnitsPerDisk;
  raidPtr->sectorsPerDisk = layoutPtr->stripeUnitsPerDisk * layoutPtr->sectorsPerStripeUnit;
 
  raidPtr->totalSectors = layoutPtr->stripeUnitsPerDisk * layoutPtr->numDataCol * layoutPtr->sectorsPerStripeUnit;

  return(0);
}

RF_ReconUnitCount_t rf_GetNumSpareRUsRAID5_RS(raidPtr)
  RF_Raid_t  *raidPtr;
{
  return ( raidPtr->Layout.stripeUnitsPerDisk / raidPtr->numCol );
}

void rf_MapSectorRAID5_RS(
  RF_Raid_t         *raidPtr,
  RF_RaidAddr_t      raidSector,
  RF_RowCol_t       *row,
  RF_RowCol_t       *col,
  RF_SectorNum_t    *diskSector,
  int                remap)
{
  RF_StripeNum_t SUID = raidSector / raidPtr->Layout.sectorsPerStripeUnit;

  *row = 0;
  if (remap) {
    *col =  raidPtr->numCol-1-(1+SUID/raidPtr->Layout.numDataCol)%raidPtr->numCol;
    *col = (*col+1)%raidPtr->numCol; /*spare unit is rotated with parity; line above maps to parity */
  }
  else {
      *col = ( SUID + (SUID/raidPtr->Layout.numDataCol) ) % raidPtr->numCol;
  }
  *diskSector = (SUID / (raidPtr->Layout.numDataCol)) * raidPtr->Layout.sectorsPerStripeUnit +
    (raidSector % raidPtr->Layout.sectorsPerStripeUnit);
}

void rf_MapParityRAID5_RS(
  RF_Raid_t       *raidPtr,
  RF_RaidAddr_t    raidSector,
  RF_RowCol_t     *row,
  RF_RowCol_t     *col,
  RF_SectorNum_t  *diskSector,
  int              remap)
{
  RF_StripeNum_t SUID = raidSector / raidPtr->Layout.sectorsPerStripeUnit;
  
  *row = 0;
  *col = raidPtr->numCol-1-(1+SUID/raidPtr->Layout.numDataCol)%raidPtr->numCol;
  *diskSector =(SUID / (raidPtr->Layout.numDataCol)) * raidPtr->Layout.sectorsPerStripeUnit +
    (raidSector % raidPtr->Layout.sectorsPerStripeUnit);
  if (remap)
	*col = (*col+1)%raidPtr->numCol;
}

void rf_IdentifyStripeRAID5_RS(
  RF_Raid_t        *raidPtr,
  RF_RaidAddr_t     addr,
  RF_RowCol_t     **diskids,
  RF_RowCol_t      *outRow)
{
  RF_StripeNum_t stripeID = rf_RaidAddressToStripeID(&raidPtr->Layout, addr);
  RF_Raid5RSConfigInfo_t *info = (RF_Raid5RSConfigInfo_t *) raidPtr->Layout.layoutSpecificInfo;
  *outRow = 0;
  *diskids = info->stripeIdentifier[ stripeID % raidPtr->numCol ];

}

void rf_MapSIDToPSIDRAID5_RS(
  RF_RaidLayout_t    *layoutPtr,
  RF_StripeNum_t      stripeID,
  RF_StripeNum_t     *psID,
  RF_ReconUnitNum_t  *which_ru)
{
  *which_ru = 0;
  *psID = stripeID;
}

