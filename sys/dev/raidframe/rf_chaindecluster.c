/*	$OpenBSD: rf_chaindecluster.c,v 1.1 1999/01/11 14:29:01 niklas Exp $	*/
/*	$NetBSD: rf_chaindecluster.c,v 1.1 1998/11/13 04:20:26 oster Exp $	*/
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

/******************************************************************************
 *
 * rf_chaindecluster.c -- implements chained declustering
 *
 *****************************************************************************/

/* :  
 * Log: rf_chaindecluster.c,v 
 * Revision 1.33  1996/08/02 13:20:34  jimz
 * get rid of bogus (long) casts
 *
 * Revision 1.32  1996/07/31  16:56:18  jimz
 * dataBytesPerStripe, sectorsPerDisk init arch-indep.
 *
 * Revision 1.31  1996/07/29  14:05:12  jimz
 * fix numPUs/numRUs confusion (everything is now numRUs)
 * clean up some commenting, return values
 *
 * Revision 1.30  1996/07/22  19:52:16  jimz
 * switched node params to RF_DagParam_t, a union of
 * a 64-bit int and a void *, for better portability
 * attempted hpux port, but failed partway through for
 * lack of a single C compiler capable of compiling all
 * source files
 *
 * Revision 1.29  1996/07/18  22:57:14  jimz
 * port simulator to AIX
 *
 * Revision 1.28  1996/06/19  17:53:48  jimz
 * move GetNumSparePUs, InstallSpareTable ops into layout switch
 *
 * Revision 1.27  1996/06/11  15:19:57  wvcii
 * added include of rf_chaindecluster.h
 * fixed parameter list of rf_ConfigureChainDecluster
 *
 * Revision 1.26  1996/06/11  08:55:15  jimz
 * improved error-checking at configuration time
 *
 * Revision 1.25  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.24  1996/06/07  22:26:27  jimz
 * type-ify which_ru (RF_ReconUnitNum_t)
 *
 * Revision 1.23  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.22  1996/06/06  17:31:30  jimz
 * use CreateMirrorPartitionReadDAG for mirrored reads
 *
 * Revision 1.21  1996/06/03  23:28:26  jimz
 * more bugfixes
 * check in tree to sync for IPDS runs with current bugfixes
 * there still may be a problem with threads in the script test
 * getting I/Os stuck- not trivially reproducible (runs ~50 times
 * in a row without getting stuck)
 *
 * Revision 1.20  1996/06/02  17:31:48  jimz
 * Moved a lot of global stuff into array structure, where it belongs.
 * Fixed up paritylogging, pss modules in this manner. Some general
 * code cleanup. Removed lots of dead code, some dead files.
 *
 * Revision 1.19  1996/05/31  22:26:54  jimz
 * fix a lot of mapping problems, memory allocation problems
 * found some weird lock issues, fixed 'em
 * more code cleanup
 *
 * Revision 1.18  1996/05/31  16:13:28  amiri
 * removed/added some commnets.
 *
 * Revision 1.17  1996/05/31  05:01:52  amiri
 * fixed a bug related to sparing layout.
 *
 * Revision 1.16  1996/05/30  23:22:16  jimz
 * bugfixes of serialization, timing problems
 * more cleanup
 *
 * Revision 1.15  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.14  1996/05/24  22:17:04  jimz
 * continue code + namespace cleanup
 * typed a bunch of flags
 *
 * Revision 1.13  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.12  1996/05/23  00:33:23  jimz
 * code cleanup: move all debug decls to rf_options.c, all extern
 * debug decls to rf_options.h, all debug vars preceded by rf_
 *
 * Revision 1.11  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.10  1996/05/03  19:53:56  wvcii
 * removed include of rf_redstripe.h
 * moved dag creation routines to new dag library
 *
 */

#include "rf_archs.h"
#include "rf_types.h"
#include "rf_raid.h"
#include "rf_chaindecluster.h"
#include "rf_dag.h"
#include "rf_dagutils.h"
#include "rf_dagffrd.h"
#include "rf_dagffwr.h"
#include "rf_dagdegrd.h"
#include "rf_dagfuncs.h"
#include "rf_threadid.h"
#include "rf_general.h"
#include "rf_utils.h"

typedef struct RF_ChaindeclusterConfigInfo_s {
  RF_RowCol_t       **stripeIdentifier;   /* filled in at config time
                                           * and used by IdentifyStripe */
  RF_StripeCount_t    numSparingRegions;
  RF_StripeCount_t    stripeUnitsPerSparingRegion;
  RF_SectorNum_t      mirrorStripeOffset;  
} RF_ChaindeclusterConfigInfo_t;

int rf_ConfigureChainDecluster(
  RF_ShutdownList_t  **listp,
  RF_Raid_t           *raidPtr,
  RF_Config_t         *cfgPtr)
{
  RF_RaidLayout_t *layoutPtr = &raidPtr->Layout;
  RF_StripeCount_t num_used_stripeUnitsPerDisk;
  RF_ChaindeclusterConfigInfo_t *info;
  RF_RowCol_t i;
  
  /* create a Chained Declustering configuration structure */
  RF_MallocAndAdd(info, sizeof(RF_ChaindeclusterConfigInfo_t), (RF_ChaindeclusterConfigInfo_t *), raidPtr->cleanupList);
  if (info == NULL)
    return(ENOMEM);
  layoutPtr->layoutSpecificInfo = (void *) info;

  /*  fill in the config structure.  */
  info->stripeIdentifier = rf_make_2d_array(raidPtr->numCol, 2 , raidPtr->cleanupList);
  if (info->stripeIdentifier == NULL)
    return(ENOMEM);
  for (i=0; i< raidPtr->numCol; i++) {
      info->stripeIdentifier[i][0] = i % raidPtr->numCol;
      info->stripeIdentifier[i][1] = (i+1) % raidPtr->numCol;
    }

  RF_ASSERT(raidPtr->numRow == 1);

  /* fill in the remaining layout parameters */
  num_used_stripeUnitsPerDisk = layoutPtr->stripeUnitsPerDisk - (layoutPtr->stripeUnitsPerDisk %
        (2*raidPtr->numCol-2) );
  info->numSparingRegions = num_used_stripeUnitsPerDisk / (2*raidPtr->numCol-2);
  info->stripeUnitsPerSparingRegion = raidPtr->numCol * (raidPtr->numCol - 1);
  info->mirrorStripeOffset = info->numSparingRegions * (raidPtr->numCol-1);
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

RF_ReconUnitCount_t rf_GetNumSpareRUsChainDecluster(raidPtr)
  RF_Raid_t  *raidPtr;
{
  RF_ChaindeclusterConfigInfo_t *info = (RF_ChaindeclusterConfigInfo_t *) raidPtr->Layout.layoutSpecificInfo;

  /*
   * The layout uses two stripe units per disk as spare within each
   * sparing region.
   */
  return (2*info->numSparingRegions);
}


/* Maps to the primary copy of the data, i.e. the first mirror pair */
void rf_MapSectorChainDecluster(
  RF_Raid_t         *raidPtr,
  RF_RaidAddr_t      raidSector,
  RF_RowCol_t       *row,
  RF_RowCol_t       *col,
  RF_SectorNum_t    *diskSector,
  int                remap)
{
 RF_ChaindeclusterConfigInfo_t *info = (RF_ChaindeclusterConfigInfo_t *) raidPtr->Layout.layoutSpecificInfo;
 RF_StripeNum_t SUID = raidSector / raidPtr->Layout.sectorsPerStripeUnit;
 RF_SectorNum_t index_within_region, index_within_disk;
 RF_StripeNum_t sparing_region_id;
 int col_before_remap;

 *row = 0;
 sparing_region_id = SUID / info->stripeUnitsPerSparingRegion;
 index_within_region = SUID % info->stripeUnitsPerSparingRegion;
 index_within_disk = index_within_region / raidPtr->numCol;
 col_before_remap = SUID % raidPtr->numCol;

 if (!remap) {
        *col = col_before_remap;
        *diskSector = ( index_within_disk + ( (raidPtr->numCol-1) * sparing_region_id) ) *
                        raidPtr->Layout.sectorsPerStripeUnit;
        *diskSector += (raidSector % raidPtr->Layout.sectorsPerStripeUnit);
      }
 else {
       /* remap sector to spare space...*/
      *diskSector = sparing_region_id * (raidPtr->numCol+1) * raidPtr->Layout.sectorsPerStripeUnit;
      *diskSector += (raidPtr->numCol-1) * raidPtr->Layout.sectorsPerStripeUnit;
      *diskSector += (raidSector % raidPtr->Layout.sectorsPerStripeUnit);
      index_within_disk = index_within_region / raidPtr->numCol;
      if (index_within_disk < col_before_remap )
        *col = index_within_disk;
      else if (index_within_disk  == raidPtr->numCol-2 ) {
        *col = (col_before_remap+raidPtr->numCol-1) % raidPtr->numCol;
        *diskSector += raidPtr->Layout.sectorsPerStripeUnit;
        }
      else
        *col = (index_within_disk + 2) % raidPtr->numCol; 
   }

}



/* Maps to the second copy of the mirror pair, which is chain declustered. The second copy is contained
   in the next disk (mod numCol) after the disk containing the primary copy. 
   The offset into the disk is one-half disk down */
void rf_MapParityChainDecluster(
  RF_Raid_t       *raidPtr,
  RF_RaidAddr_t    raidSector,
  RF_RowCol_t     *row,
  RF_RowCol_t     *col,
  RF_SectorNum_t  *diskSector,
  int              remap)
{
  RF_ChaindeclusterConfigInfo_t *info = (RF_ChaindeclusterConfigInfo_t *) raidPtr->Layout.layoutSpecificInfo;
  RF_StripeNum_t SUID = raidSector / raidPtr->Layout.sectorsPerStripeUnit;
  RF_SectorNum_t index_within_region, index_within_disk;
  RF_StripeNum_t sparing_region_id;
  int col_before_remap;

  *row = 0;
  if (!remap) {
        *col = SUID % raidPtr->numCol;
        *col = (*col + 1) % raidPtr->numCol;
        *diskSector =  info->mirrorStripeOffset * raidPtr->Layout.sectorsPerStripeUnit;
        *diskSector += ( SUID / raidPtr->numCol ) * raidPtr->Layout.sectorsPerStripeUnit;
        *diskSector += (raidSector % raidPtr->Layout.sectorsPerStripeUnit);
       }
  else {
        /* remap parity to spare space ... */
        sparing_region_id = SUID / info->stripeUnitsPerSparingRegion;
        index_within_region = SUID % info->stripeUnitsPerSparingRegion;
        index_within_disk = index_within_region / raidPtr->numCol;
        *diskSector =  sparing_region_id * (raidPtr->numCol+1) * raidPtr->Layout.sectorsPerStripeUnit;
        *diskSector += (raidPtr->numCol) * raidPtr->Layout.sectorsPerStripeUnit;
        *diskSector += (raidSector % raidPtr->Layout.sectorsPerStripeUnit);
        col_before_remap = SUID % raidPtr->numCol;
        if (index_within_disk < col_before_remap)
                *col = index_within_disk;
        else if (index_within_disk  == raidPtr->numCol-2 ) {
                *col = (col_before_remap+2) % raidPtr->numCol;
                *diskSector -= raidPtr->Layout.sectorsPerStripeUnit;
                }
        else
                *col = (index_within_disk + 2) % raidPtr->numCol;
  }

}

void rf_IdentifyStripeChainDecluster(
  RF_Raid_t        *raidPtr,
  RF_RaidAddr_t     addr,
  RF_RowCol_t     **diskids,
  RF_RowCol_t      *outRow)
{
  RF_ChaindeclusterConfigInfo_t *info = (RF_ChaindeclusterConfigInfo_t *) raidPtr->Layout.layoutSpecificInfo;
  RF_StripeNum_t SUID;
  RF_RowCol_t col;

  SUID = addr / raidPtr->Layout.sectorsPerStripeUnit;
  col = SUID  % raidPtr->numCol;
  *outRow = 0;
  *diskids = info->stripeIdentifier[ col ];
}

void rf_MapSIDToPSIDChainDecluster(
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
 *              createFunc - function to use to create the graph (return value)
 *****************************************************************************/

void rf_RAIDCDagSelect(
  RF_Raid_t             *raidPtr,
  RF_IoType_t            type,
  RF_AccessStripeMap_t  *asmap,
  RF_VoidFuncPtr *createFunc)
#if 0
  void (**createFunc)(RF_Raid_t *, RF_AccessStripeMap_t *,
		     RF_DagHeader_t *, void *, RF_RaidAccessFlags_t,
		     RF_AllocListElem_t *))
#endif
{
  RF_ASSERT(RF_IO_IS_R_OR_W(type));
  RF_ASSERT(raidPtr->numRow == 1);

  if (asmap->numDataFailed + asmap->numParityFailed > 1) {
    RF_ERRORMSG("Multiple disks failed in a single group!  Aborting I/O operation.\n");
    *createFunc = NULL;
    return;
  }

  *createFunc = (type == RF_IO_TYPE_READ) ? (RF_VoidFuncPtr)rf_CreateFaultFreeReadDAG :(RF_VoidFuncPtr) rf_CreateRaidOneWriteDAG;

  if (type == RF_IO_TYPE_READ) {
    if ( ( raidPtr->status[0] == rf_rs_degraded ) || (  raidPtr->status[0] == rf_rs_reconstructing) )
      *createFunc = (RF_VoidFuncPtr)rf_CreateRaidCDegradedReadDAG;  /* array status is degraded, implement workload shifting */
    else
      *createFunc = (RF_VoidFuncPtr)rf_CreateMirrorPartitionReadDAG; /* array status not degraded, so use mirror partition dag */
  }
  else
    *createFunc = (RF_VoidFuncPtr)rf_CreateRaidOneWriteDAG;
}
