/*	$OpenBSD: rf_interdecluster.c,v 1.1 1999/01/11 14:29:26 niklas Exp $	*/
/*	$NetBSD: rf_interdecluster.c,v 1.1 1998/11/13 04:20:30 oster Exp $	*/
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

/************************************************************
 *
 * rf_interdecluster.c -- implements interleaved declustering 
 *
 ************************************************************/

/* :  
 * Log: rf_interdecluster.c,v 
 * Revision 1.24  1996/08/02 13:20:38  jimz
 * get rid of bogus (long) casts
 *
 * Revision 1.23  1996/07/31  16:56:18  jimz
 * dataBytesPerStripe, sectorsPerDisk init arch-indep.
 *
 * Revision 1.22  1996/07/29  14:05:12  jimz
 * fix numPUs/numRUs confusion (everything is now numRUs)
 * clean up some commenting, return values
 *
 * Revision 1.21  1996/07/22  19:52:16  jimz
 * switched node params to RF_DagParam_t, a union of
 * a 64-bit int and a void *, for better portability
 * attempted hpux port, but failed partway through for
 * lack of a single C compiler capable of compiling all
 * source files
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
 * Revision 1.17  1996/06/11  15:17:55  wvcii
 * added include of rf_interdecluster.h
 * fixed parameter list of rf_ConfigureInterDecluster
 * fixed return type of rf_GetNumSparePUsInterDecluster
 * removed include of rf_raid1.h
 *
 * Revision 1.16  1996/06/11  08:55:15  jimz
 * improved error-checking at configuration time
 *
 * Revision 1.15  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.14  1996/06/07  22:26:27  jimz
 * type-ify which_ru (RF_ReconUnitNum_t)
 *
 * Revision 1.13  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.12  1996/06/06  18:41:48  jimz
 * add interleaved declustering dag selection
 *
 * Revision 1.11  1996/06/02  17:31:48  jimz
 * Moved a lot of global stuff into array structure, where it belongs.
 * Fixed up paritylogging, pss modules in this manner. Some general
 * code cleanup. Removed lots of dead code, some dead files.
 *
 * Revision 1.10  1996/05/31  22:26:54  jimz
 * fix a lot of mapping problems, memory allocation problems
 * found some weird lock issues, fixed 'em
 * more code cleanup
 *
 * Revision 1.9  1996/05/31  05:03:01  amiri
 * fixed a bug related to sparing layout.
 *
 * Revision 1.8  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.7  1996/05/24  01:59:45  jimz
 * another checkpoint in code cleanup for release
 * time to sync kernel tree
 *
 * Revision 1.6  1996/05/23  00:33:23  jimz
 * code cleanup: move all debug decls to rf_options.c, all extern
 * debug decls to rf_options.h, all debug vars preceded by rf_
 *
 * Revision 1.5  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.4  1996/05/03  19:50:38  wvcii
 * removed include of rf_redstripe.h
 * fixed change log parameters in header
 *
 */

#include "rf_types.h"
#include "rf_raid.h"
#include "rf_interdecluster.h"
#include "rf_dag.h"
#include "rf_dagutils.h"
#include "rf_dagfuncs.h"
#include "rf_threadid.h"
#include "rf_general.h"
#include "rf_utils.h"
#include "rf_dagffrd.h"
#include "rf_dagdegrd.h"
#include "rf_dagffwr.h"
#include "rf_dagdegwr.h"

typedef struct RF_InterdeclusterConfigInfo_s {
  RF_RowCol_t       **stripeIdentifier;   /* filled in at config time
                                           * and used by IdentifyStripe */
  RF_StripeCount_t    numSparingRegions;
  RF_StripeCount_t    stripeUnitsPerSparingRegion;
  RF_SectorNum_t      mirrorStripeOffset;  
} RF_InterdeclusterConfigInfo_t;

int rf_ConfigureInterDecluster(
  RF_ShutdownList_t  **listp,
  RF_Raid_t           *raidPtr,
  RF_Config_t         *cfgPtr)
{
  RF_RaidLayout_t *layoutPtr = &raidPtr->Layout;
  RF_StripeCount_t num_used_stripeUnitsPerDisk;
  RF_InterdeclusterConfigInfo_t *info;
  RF_RowCol_t i, tmp, SUs_per_region;
  
  /* create an Interleaved Declustering configuration structure */
  RF_MallocAndAdd(info, sizeof(RF_InterdeclusterConfigInfo_t), (RF_InterdeclusterConfigInfo_t *), 
			raidPtr->cleanupList);
  if (info == NULL)
    return(ENOMEM);
  layoutPtr->layoutSpecificInfo = (void *) info;

  /*  fill in the config structure.  */
  SUs_per_region = raidPtr->numCol * (raidPtr->numCol - 1);
  info->stripeIdentifier = rf_make_2d_array(SUs_per_region, 2 , raidPtr->cleanupList);
  if (info->stripeIdentifier == NULL)
    return(ENOMEM);
  for (i=0; i< SUs_per_region; i++) {
      info->stripeIdentifier[i][0] = i / (raidPtr->numCol-1);
      tmp = i / raidPtr->numCol;
      info->stripeIdentifier[i][1] = (i+1+tmp) % raidPtr->numCol;
    }

  /* no spare tables */
  RF_ASSERT(raidPtr->numRow == 1);

  /* fill in the remaining layout parameters */

  /* total number of stripes should a multiple of 2*numCol: Each sparing region consists of
   2*numCol stripes: n-1 primary copy, n-1 secondary copy and 2 for spare .. */ 
    num_used_stripeUnitsPerDisk = layoutPtr->stripeUnitsPerDisk - (layoutPtr->stripeUnitsPerDisk %
	(2*raidPtr->numCol) );
  info->numSparingRegions = num_used_stripeUnitsPerDisk / (2*raidPtr->numCol);
  /* this is in fact the number of stripe units (that are primary data copies) in the sparing region */
  info->stripeUnitsPerSparingRegion = raidPtr->numCol * (raidPtr->numCol - 1);
  info->mirrorStripeOffset = info->numSparingRegions * (raidPtr->numCol+1);
  layoutPtr->numStripe = info->numSparingRegions * info->stripeUnitsPerSparingRegion;
  layoutPtr->bytesPerStripeUnit = layoutPtr->sectorsPerStripeUnit << raidPtr->logBytesPerSector;
  layoutPtr->numDataCol = 1;
  layoutPtr->dataSectorsPerStripe = layoutPtr->numDataCol * layoutPtr->sectorsPerStripeUnit;
  layoutPtr->numParityCol = 1;
 
  layoutPtr->dataStripeUnitsPerDisk = num_used_stripeUnitsPerDisk;

  raidPtr->sectorsPerDisk = 
    num_used_stripeUnitsPerDisk * layoutPtr->sectorsPerStripeUnit;

  raidPtr->totalSectors = 
    (layoutPtr->numStripe) * layoutPtr->sectorsPerStripeUnit;

  layoutPtr->stripeUnitsPerDisk = raidPtr->sectorsPerDisk / layoutPtr->sectorsPerStripeUnit;

  return(0);
}

int rf_GetDefaultNumFloatingReconBuffersInterDecluster(RF_Raid_t *raidPtr)
{
  return(30);
}

RF_HeadSepLimit_t rf_GetDefaultHeadSepLimitInterDecluster(RF_Raid_t *raidPtr)
{
  return(raidPtr->sectorsPerDisk);
}

RF_ReconUnitCount_t rf_GetNumSpareRUsInterDecluster(
  RF_Raid_t  *raidPtr)
{
  RF_InterdeclusterConfigInfo_t *info = (RF_InterdeclusterConfigInfo_t *) raidPtr->Layout.layoutSpecificInfo;

  return ( 2 * ((RF_ReconUnitCount_t) info->numSparingRegions) ); 
	/* the layout uses two stripe units per disk as spare within each sparing region */
}

/* Maps to the primary copy of the data, i.e. the first mirror pair */
void rf_MapSectorInterDecluster(
  RF_Raid_t         *raidPtr,
  RF_RaidAddr_t      raidSector,
  RF_RowCol_t       *row,
  RF_RowCol_t       *col,
  RF_SectorNum_t    *diskSector,
  int                remap)
{
 RF_InterdeclusterConfigInfo_t *info = (RF_InterdeclusterConfigInfo_t *) raidPtr->Layout.layoutSpecificInfo;
 RF_StripeNum_t SUID = raidSector / raidPtr->Layout.sectorsPerStripeUnit;
 RF_StripeNum_t su_offset_into_disk, mirror_su_offset_into_disk;
 RF_StripeNum_t sparing_region_id, index_within_region;
 int col_before_remap;

 *row = 0;
 sparing_region_id = SUID / info->stripeUnitsPerSparingRegion;
 index_within_region = SUID % info->stripeUnitsPerSparingRegion;
 su_offset_into_disk = index_within_region % (raidPtr->numCol-1);
 mirror_su_offset_into_disk = index_within_region / raidPtr->numCol;
 col_before_remap = index_within_region / (raidPtr->numCol-1);

 if (!remap) {
        *col = col_before_remap;;
        *diskSector = ( su_offset_into_disk + ( (raidPtr->numCol-1) * sparing_region_id) ) *
                        raidPtr->Layout.sectorsPerStripeUnit;
        *diskSector += (raidSector % raidPtr->Layout.sectorsPerStripeUnit);
      }
 else {
      /* remap sector to spare space...*/
      *diskSector = sparing_region_id * (raidPtr->numCol+1) * raidPtr->Layout.sectorsPerStripeUnit;
      *diskSector += (raidPtr->numCol-1) * raidPtr->Layout.sectorsPerStripeUnit;
      *diskSector += (raidSector % raidPtr->Layout.sectorsPerStripeUnit);
      *col = (index_within_region + 1 + mirror_su_offset_into_disk) % raidPtr->numCol;
      *col = (*col + 1) % raidPtr->numCol;
      if (*col == col_before_remap) *col = (*col + 1) % raidPtr->numCol;
   }
}

/* Maps to the second copy of the mirror pair. */
void rf_MapParityInterDecluster(
  RF_Raid_t       *raidPtr,
  RF_RaidAddr_t    raidSector,
  RF_RowCol_t     *row,
  RF_RowCol_t     *col,
  RF_SectorNum_t  *diskSector,
  int              remap)
{
  RF_InterdeclusterConfigInfo_t *info = (RF_InterdeclusterConfigInfo_t *) raidPtr->Layout.layoutSpecificInfo;
  RF_StripeNum_t sparing_region_id, index_within_region, mirror_su_offset_into_disk;
  RF_StripeNum_t SUID = raidSector / raidPtr->Layout.sectorsPerStripeUnit;
  int col_before_remap;
  
  sparing_region_id = SUID / info->stripeUnitsPerSparingRegion;
  index_within_region = SUID % info->stripeUnitsPerSparingRegion;
  mirror_su_offset_into_disk = index_within_region / raidPtr->numCol;
  col_before_remap = (index_within_region + 1 + mirror_su_offset_into_disk) % raidPtr->numCol;

  *row = 0;
  if (!remap) {
        *col = col_before_remap;
        *diskSector =  info->mirrorStripeOffset * raidPtr->Layout.sectorsPerStripeUnit;
        *diskSector += sparing_region_id * (raidPtr->numCol-1)  * raidPtr->Layout.sectorsPerStripeUnit;
        *diskSector += mirror_su_offset_into_disk * raidPtr->Layout.sectorsPerStripeUnit;
        *diskSector += (raidSector % raidPtr->Layout.sectorsPerStripeUnit);
       }
  else {
        /* remap parity to spare space ... */
        *diskSector =  sparing_region_id * (raidPtr->numCol+1) * raidPtr->Layout.sectorsPerStripeUnit;
        *diskSector += (raidPtr->numCol) * raidPtr->Layout.sectorsPerStripeUnit;
        *diskSector += (raidSector % raidPtr->Layout.sectorsPerStripeUnit);
        *col = index_within_region / (raidPtr->numCol-1);
        *col = (*col + 1) % raidPtr->numCol;
        if (*col == col_before_remap) *col = (*col + 1) % raidPtr->numCol;
  }
}

void rf_IdentifyStripeInterDecluster(
  RF_Raid_t        *raidPtr,
  RF_RaidAddr_t     addr,
  RF_RowCol_t     **diskids,
  RF_RowCol_t      *outRow)
{
  RF_InterdeclusterConfigInfo_t *info = (RF_InterdeclusterConfigInfo_t *) raidPtr->Layout.layoutSpecificInfo;
  RF_StripeNum_t SUID;

  SUID = addr / raidPtr->Layout.sectorsPerStripeUnit;
  SUID = SUID % info->stripeUnitsPerSparingRegion;

  *outRow = 0;
  *diskids = info->stripeIdentifier[ SUID ];
}

void rf_MapSIDToPSIDInterDecluster(
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

void rf_RAIDIDagSelect(
  RF_Raid_t             *raidPtr,
  RF_IoType_t            type,
  RF_AccessStripeMap_t  *asmap,
  RF_VoidFuncPtr *createFunc)
{
  RF_ASSERT(RF_IO_IS_R_OR_W(type));

  if (asmap->numDataFailed + asmap->numParityFailed > 1) {
    RF_ERRORMSG("Multiple disks failed in a single group!  Aborting I/O operation.\n");
    *createFunc = NULL;
    return;
  }
  
  *createFunc = (type == RF_IO_TYPE_READ) ? (RF_VoidFuncPtr)rf_CreateFaultFreeReadDAG : (RF_VoidFuncPtr)rf_CreateRaidOneWriteDAG;
  if (type == RF_IO_TYPE_READ) {
    if (asmap->numDataFailed == 0)
      *createFunc = (RF_VoidFuncPtr)rf_CreateMirrorPartitionReadDAG;
    else
      *createFunc = (RF_VoidFuncPtr)rf_CreateRaidOneDegradedReadDAG;
  }
  else
    *createFunc = (RF_VoidFuncPtr)rf_CreateRaidOneWriteDAG;
}
