/*	$OpenBSD: rf_declusterPQ.c,v 1.1 1999/01/11 14:29:14 niklas Exp $	*/
/*	$NetBSD: rf_declusterPQ.c,v 1.1 1998/11/13 04:20:28 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Authors: Daniel Stodolsky, Mark Holland, Jim Zelenka
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

/*--------------------------------------------------
 * rf_declusterPQ.c
 *
 * mapping code for declustered P & Q or declustered EvenOdd
 * much code borrowed from rf_decluster.c
 *
 *--------------------------------------------------*/

/*
 * $Header: /home/cvs/src/sys/dev/raidframe/Attic/rf_declusterPQ.c,v 1.1 1999/01/11 14:29:14 niklas Exp $
 *
 * Log: rf_declusterPQ.c,v 
 * Revision 1.34  1996/08/21 19:47:14  jimz
 * fix bogus return values from config
 *
 * Revision 1.33  1996/08/21  15:09:16  jimz
 * cleanup debugging spoo
 *
 * Revision 1.32  1996/08/21  04:13:36  jimz
 * debug with EvenOdd
 *
 * Revision 1.31  1996/08/20  22:41:54  jimz
 * 2 parity disks, not 1
 *
 * Revision 1.30  1996/07/31  16:56:18  jimz
 * dataBytesPerStripe, sectorsPerDisk init arch-indep.
 *
 * Revision 1.29  1996/07/18  22:57:14  jimz
 * port simulator to AIX
 *
 * Revision 1.28  1996/07/13  00:00:59  jimz
 * sanitized generalized reconstruction architecture
 * cleaned up head sep, rbuf problems
 *
 * Revision 1.27  1996/06/11  08:45:12  jimz
 * improved error-checking on array configuration
 *
 * Revision 1.26  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.25  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.24  1996/06/02  17:31:48  jimz
 * Moved a lot of global stuff into array structure, where it belongs.
 * Fixed up paritylogging, pss modules in this manner. Some general
 * code cleanup. Removed lots of dead code, some dead files.
 *
 * Revision 1.23  1996/05/30  23:22:16  jimz
 * bugfixes of serialization, timing problems
 * more cleanup
 *
 * Revision 1.22  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.21  1996/05/24  22:17:04  jimz
 * continue code + namespace cleanup
 * typed a bunch of flags
 *
 * Revision 1.20  1996/05/24  01:59:45  jimz
 * another checkpoint in code cleanup for release
 * time to sync kernel tree
 *
 * Revision 1.19  1996/05/23  00:33:23  jimz
 * code cleanup: move all debug decls to rf_options.c, all extern
 * debug decls to rf_options.h, all debug vars preceded by rf_
 *
 * Revision 1.18  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.17  1996/05/17  00:52:56  jimz
 * RepIndex was not being initialized before the computation of
 * RepIndexQ in MapQDeclusteredPQ(). I copied the initialization
 * from MapParityDeclusteredPQ(). Hope that was right.
 *
 * Revision 1.16  1995/12/12  18:10:06  jimz
 * MIN -> RF_MIN, MAX -> RF_MAX, ASSERT -> RF_ASSERT
 * fix 80-column brain damage in comments
 *
 * Revision 1.15  1995/12/01  15:57:46  root
 * added copyright info
 *
 * Revision 1.14  1995/11/17  19:00:13  wvcii
 * added prototyping to MapParity
 * created MapQ
 *
 * Revision 1.13  1995/10/05  22:20:48  jimz
 * free_1d_array() takes two args; provide them both
 *
 * Revision 1.12  1995/09/06  19:26:33  wvcii
 * offset cfgBuf by sparemap length (ConfigureDeclusteredPQ)
 *
 * Revision 1.11  95/06/23  13:41:11  robby
 * updeated to prototypes in rf_layout.h
 * 
 * Revision 1.10  1995/05/02  22:46:53  holland
 * minor code cleanups.
 *
 * Revision 1.9  1995/03/15  20:45:23  holland
 * distr sparing changes.
 *
 * Revision 1.8  1995/03/01  20:25:48  holland
 * kernelization changes
 *
 * Revision 1.7  1995/02/17  19:39:56  holland
 * added size param to all calls to Free().
 * this is ignored at user level, but necessary in the kernel.
 *
 * Revision 1.6  1995/02/10  17:34:10  holland
 * kernelization changes
 *
 * Revision 1.5  1995/02/03  22:31:36  holland
 * many changes related to kernelization
 *
 * Revision 1.4  1995/02/01  15:13:05  holland
 * moved #include of general.h out of raid.h and into each file
 *
 * Revision 1.3  1995/02/01  14:25:19  holland
 * began changes for kernelization:
 *      changed all instances of mutex_t and cond_t to DECLARE macros
 *      converted configuration code to use config structure
 *
 * Revision 1.2  1994/11/28  22:13:56  danner
 * corrected some mapping bugs.
 *
 */

#include "rf_types.h"
#include "rf_raid.h"
#include "rf_configure.h"
#include "rf_decluster.h"
#include "rf_declusterPQ.h"
#include "rf_debugMem.h"
#include "rf_utils.h"
#include "rf_alloclist.h"
#include "rf_general.h"

/* configuration code */

int rf_ConfigureDeclusteredPQ(
  RF_ShutdownList_t  **listp,
  RF_Raid_t           *raidPtr,
  RF_Config_t         *cfgPtr)
{
    RF_RaidLayout_t *layoutPtr = &(raidPtr->Layout);
    int b, v, k, r, lambda;				/* block design params */
    int i, j, l;
    int *first_avail_slot;
    int  complete_FT_count, SUID;
    RF_DeclusteredConfigInfo_t *info;
    int numCompleteFullTablesPerDisk;
    int PUsPerDisk, spareRegionDepthInPUs, numCompleteSpareRegionsPerDisk = 0, extraPUsPerDisk;
    int totSparePUsPerDisk;
    int diskOffsetOfLastFullTableInSUs, SpareSpaceInSUs;
    char *cfgBuf = (char *) (cfgPtr->layoutSpecific);

    cfgBuf += RF_SPAREMAP_NAME_LEN;

    b        = *( (int *) cfgBuf);   cfgBuf += sizeof(int);
    v        = *( (int *) cfgBuf);   cfgBuf += sizeof(int);
    k        = *( (int *) cfgBuf);   cfgBuf += sizeof(int);
    r        = *( (int *) cfgBuf);   cfgBuf += sizeof(int);
    lambda   = *( (int *) cfgBuf);   cfgBuf += sizeof(int);
    raidPtr->noRotate = *( (int *) cfgBuf);   cfgBuf += sizeof(int);

    if (k <= 2) {
      printf("RAIDFRAME: k=%d, minimum value 2\n", k);
      return(EINVAL);
    }

    /* 1. create layout specific structure */
    RF_MallocAndAdd(info, sizeof(RF_DeclusteredConfigInfo_t), (RF_DeclusteredConfigInfo_t *), raidPtr->cleanupList);
    if (info == NULL)
      return(ENOMEM);
    layoutPtr->layoutSpecificInfo = (void *) info;

    /* the sparemaps are generated assuming that parity is rotated, so we issue
     * a warning if both distributed sparing and no-rotate are on at the same time
     */
    if ((raidPtr->Layout.map->flags & RF_DISTRIBUTE_SPARE) && raidPtr->noRotate) {
	RF_ERRORMSG("Warning:  distributed sparing specified without parity rotation.\n");
    }

    if (raidPtr->numCol != v) {
        RF_ERRORMSG2("RAID: config error: table element count (%d) not equal to no. of cols (%d)\n", v, raidPtr->numCol);
        return(EINVAL);
    }

    /* 3.  set up the values used in devRaidMap */
    info->BlocksPerTable = b;
    info->NumParityReps = info->groupSize = k;
    info->PUsPerBlock = k-2;  /* PQ */
    info->SUsPerTable = b * info->PUsPerBlock * layoutPtr->SUsPerPU;/* b blks, k-1 SUs each */
    info->SUsPerFullTable = k * info->SUsPerTable;	/* rot k times */
    info->SUsPerBlock = info->PUsPerBlock * layoutPtr->SUsPerPU;
    info->TableDepthInPUs = (b*k) / v;
    info->FullTableDepthInPUs = info->TableDepthInPUs * k;		/* k repetitions */

    /* used only in distributed sparing case */
    info->FullTablesPerSpareRegion = (v-1) / rf_gcd(r, v-1);		/* (v-1)/gcd fulltables */
    info->TablesPerSpareRegion = k * info->FullTablesPerSpareRegion;
    info->SpareSpaceDepthPerRegionInSUs = (r * info->TablesPerSpareRegion / (v-1)) * layoutPtr->SUsPerPU;

    /* check to make sure the block design is sufficiently small */
    if ((raidPtr->Layout.map->flags & RF_DISTRIBUTE_SPARE)) {
        if (info->FullTableDepthInPUs * layoutPtr->SUsPerPU + info->SpareSpaceDepthPerRegionInSUs > layoutPtr->stripeUnitsPerDisk) {
	    RF_ERRORMSG3("RAID: config error: Full Table depth (%d) + Spare Space (%d) larger than disk size (%d) (BD too big)\n",
			 (int)info->FullTableDepthInPUs, 
			 (int)info->SpareSpaceDepthPerRegionInSUs, 
			 (int)layoutPtr->stripeUnitsPerDisk);
	    return(EINVAL);
	}
    } else {
	if (info->TableDepthInPUs * layoutPtr->SUsPerPU > layoutPtr->stripeUnitsPerDisk) {
	    RF_ERRORMSG2("RAID: config error: Table depth (%d) larger than disk size (%d) (BD too big)\n",
			 (int)(info->TableDepthInPUs * layoutPtr->SUsPerPU), 
			 (int)layoutPtr->stripeUnitsPerDisk);
	    return(EINVAL);
	}
    }

	
    /* compute the size of each disk, and the number of tables in the last fulltable (which
     * need not be complete)
     */
    if ((raidPtr->Layout.map->flags & RF_DISTRIBUTE_SPARE)) {
	
	PUsPerDisk = layoutPtr->stripeUnitsPerDisk / layoutPtr->SUsPerPU;
	spareRegionDepthInPUs = (info->TablesPerSpareRegion * info->TableDepthInPUs +
				 (info->TablesPerSpareRegion * info->TableDepthInPUs) / (v-1));
	info->SpareRegionDepthInSUs = spareRegionDepthInPUs * layoutPtr->SUsPerPU;
	
	numCompleteSpareRegionsPerDisk = PUsPerDisk / spareRegionDepthInPUs;
	info->NumCompleteSRs = numCompleteSpareRegionsPerDisk;
	extraPUsPerDisk = PUsPerDisk % spareRegionDepthInPUs;

	/* assume conservatively that we need the full amount of spare space in one region in order
	 * to provide spares for the partial spare region at the end of the array.  We set "i" to
	 * the number of tables in the partial spare region.  This may actually include some fulltables.
	 */
	extraPUsPerDisk -= (info->SpareSpaceDepthPerRegionInSUs / layoutPtr->SUsPerPU);
	if (extraPUsPerDisk <= 0) i = 0;
	else i = extraPUsPerDisk/info->TableDepthInPUs;

	complete_FT_count = raidPtr->numRow * (numCompleteSpareRegionsPerDisk * (info->TablesPerSpareRegion/k) + i/k);
        info->FullTableLimitSUID = complete_FT_count * info->SUsPerFullTable;
	info->ExtraTablesPerDisk = i % k;

	/* note that in the last spare region, the spare space is complete even though data/parity space is not */
	totSparePUsPerDisk = (numCompleteSpareRegionsPerDisk+1) * (info->SpareSpaceDepthPerRegionInSUs / layoutPtr->SUsPerPU);
	info->TotSparePUsPerDisk = totSparePUsPerDisk;
	
	layoutPtr->stripeUnitsPerDisk =
	    ((complete_FT_count/raidPtr->numRow) * info->FullTableDepthInPUs +	 	/* data & parity space */
	     info->ExtraTablesPerDisk * info->TableDepthInPUs +
	     totSparePUsPerDisk								/* spare space */
	    ) * layoutPtr->SUsPerPU;
	layoutPtr->dataStripeUnitsPerDisk = 
	    (complete_FT_count * info->FullTableDepthInPUs + info->ExtraTablesPerDisk * info->TableDepthInPUs)
	    * layoutPtr->SUsPerPU * (k-1) / k;

    } else {
        /* non-dist spare case:  force each disk to contain an integral number of tables */
        layoutPtr->stripeUnitsPerDisk /= (info->TableDepthInPUs * layoutPtr->SUsPerPU);
        layoutPtr->stripeUnitsPerDisk *= (info->TableDepthInPUs * layoutPtr->SUsPerPU);

	/* compute the number of tables in the last fulltable, which need not be complete */
        complete_FT_count =
            ((layoutPtr->stripeUnitsPerDisk/layoutPtr->SUsPerPU) / info->FullTableDepthInPUs) * raidPtr->numRow;
	    
        info->FullTableLimitSUID = complete_FT_count * info->SUsPerFullTable;
        info->ExtraTablesPerDisk =
		((layoutPtr->stripeUnitsPerDisk/layoutPtr->SUsPerPU) / info->TableDepthInPUs) % k;
    }

    raidPtr->sectorsPerDisk = layoutPtr->stripeUnitsPerDisk * layoutPtr->sectorsPerStripeUnit;

    /* find the disk offset of the stripe unit where the last fulltable starts */
    numCompleteFullTablesPerDisk = complete_FT_count / raidPtr->numRow;
    diskOffsetOfLastFullTableInSUs = numCompleteFullTablesPerDisk * info->FullTableDepthInPUs * layoutPtr->SUsPerPU;
    if ((raidPtr->Layout.map->flags & RF_DISTRIBUTE_SPARE)) {
        SpareSpaceInSUs  = numCompleteSpareRegionsPerDisk * info->SpareSpaceDepthPerRegionInSUs;
        diskOffsetOfLastFullTableInSUs += SpareSpaceInSUs;
        info->DiskOffsetOfLastSpareSpaceChunkInSUs =
	    diskOffsetOfLastFullTableInSUs + info->ExtraTablesPerDisk * info->TableDepthInPUs * layoutPtr->SUsPerPU;
    }
    info->DiskOffsetOfLastFullTableInSUs = diskOffsetOfLastFullTableInSUs;
    info->numCompleteFullTablesPerDisk = numCompleteFullTablesPerDisk;
	
    /* 4.  create and initialize the lookup tables */
    info->LayoutTable = rf_make_2d_array(b, k, raidPtr->cleanupList);
    if (info->LayoutTable == NULL)
      return(ENOMEM);
    info->OffsetTable = rf_make_2d_array(b, k, raidPtr->cleanupList);
    if (info->OffsetTable == NULL)
      return(ENOMEM);
    info->BlockTable  =	rf_make_2d_array(info->TableDepthInPUs*layoutPtr->SUsPerPU, raidPtr->numCol, raidPtr->cleanupList);
    if (info->BlockTable == NULL)
      return(ENOMEM);

    first_avail_slot  = (int *) rf_make_1d_array(v, NULL);
    if (first_avail_slot == NULL)
      return(ENOMEM);

    for (i=0; i<b; i++)
      for (j=0; j<k; j++)
        info->LayoutTable[i][j] = *cfgBuf++;

    /* initialize offset table */
    for (i=0; i<b; i++) for (j=0; j<k; j++) {
        info->OffsetTable[i][j] = first_avail_slot[ info->LayoutTable[i][j] ];
        first_avail_slot[ info->LayoutTable[i][j] ]++;
    }

    /* initialize block table */
    for (SUID=l=0; l<layoutPtr->SUsPerPU; l++) {
        for (i=0; i<b; i++) {
            for (j=0; j<k; j++) {
                info->BlockTable[ (info->OffsetTable[i][j] * layoutPtr->SUsPerPU) + l ]
		                [ info->LayoutTable[i][j] ] = SUID;
            }
            SUID++;
        }
    }

    rf_free_1d_array(first_avail_slot, v);

    /* 5.  set up the remaining redundant-but-useful parameters */

    raidPtr->totalSectors = (k*complete_FT_count + raidPtr->numRow*info->ExtraTablesPerDisk) *
    			  info->SUsPerTable * layoutPtr->sectorsPerStripeUnit;
    layoutPtr->numStripe = (raidPtr->totalSectors / layoutPtr->sectorsPerStripeUnit) / (k-2);

    /* strange evaluation order below to try and minimize overflow problems */

    layoutPtr->dataSectorsPerStripe = (k-2) * layoutPtr->sectorsPerStripeUnit;
    layoutPtr->bytesPerStripeUnit = layoutPtr->sectorsPerStripeUnit << raidPtr->logBytesPerSector;
    layoutPtr->numDataCol = k-2;
    layoutPtr->numParityCol = 2;

    return(0);
}

int rf_GetDefaultNumFloatingReconBuffersPQ(RF_Raid_t *raidPtr)
{
  int def_decl;

  def_decl = rf_GetDefaultNumFloatingReconBuffersDeclustered(raidPtr);
  return(RF_MAX(3 * raidPtr->numCol, def_decl));
}

void rf_MapSectorDeclusteredPQ(
  RF_Raid_t         *raidPtr,
  RF_RaidAddr_t      raidSector,
  RF_RowCol_t       *row,
  RF_RowCol_t       *col,
  RF_SectorNum_t    *diskSector,
  int                remap)
{
    RF_RaidLayout_t *layoutPtr = &(raidPtr->Layout);
    RF_DeclusteredConfigInfo_t *info = (RF_DeclusteredConfigInfo_t *) layoutPtr->layoutSpecificInfo;
    RF_StripeNum_t SUID = raidSector / layoutPtr->sectorsPerStripeUnit;
    RF_StripeNum_t FullTableID, FullTableOffset, TableID, TableOffset; 
    RF_StripeNum_t BlockID, BlockOffset, RepIndex;
    RF_StripeCount_t sus_per_fulltable = info->SUsPerFullTable;
    RF_StripeCount_t fulltable_depth  = info->FullTableDepthInPUs * layoutPtr->SUsPerPU;
    RF_StripeNum_t base_suid = 0, outSU, SpareRegion=0, SpareSpace=0;

    rf_decluster_adjust_params(layoutPtr, &SUID, &sus_per_fulltable, &fulltable_depth, &base_suid);
	    
    FullTableID     = SUID / sus_per_fulltable;		/* fulltable ID within array (across rows) */
    *row            = FullTableID % raidPtr->numRow;
    FullTableID    /= raidPtr->numRow;			/* convert to fulltable ID on this disk */
    if ((raidPtr->Layout.map->flags & RF_DISTRIBUTE_SPARE)) {
	SpareRegion = FullTableID / info->FullTablesPerSpareRegion;
        SpareSpace  = SpareRegion * info->SpareSpaceDepthPerRegionInSUs;
    }
    FullTableOffset = SUID % sus_per_fulltable;
    TableID         = FullTableOffset / info->SUsPerTable;
    TableOffset     = FullTableOffset - TableID * info->SUsPerTable;
    BlockID         = TableOffset / info->PUsPerBlock;
    BlockOffset     = TableOffset - BlockID * info->PUsPerBlock;
    BlockID        %= info->BlocksPerTable;
    RF_ASSERT(BlockOffset < info->groupSize-2 );
    /*
       TableIDs go from 0 .. GroupSize-1 inclusive. 
       PUsPerBlock is k-2. 
       We want the tableIDs to rotate from the
       right, so use GroupSize
       */
    RepIndex        = info->groupSize - 1 - TableID;
    RF_ASSERT(RepIndex >= 0);
    if (!raidPtr->noRotate)
      {
	if (TableID==0)
	    BlockOffset++; /* P on last drive, Q on first */
	else
	  BlockOffset    += ((BlockOffset >= RepIndex) ? 2 : 0); /* skip over PQ */
	RF_ASSERT(BlockOffset < info->groupSize);
	*col            = info->LayoutTable[BlockID][BlockOffset];
      }

    /* remap to distributed spare space if indicated */
    if (remap) {
	rf_remap_to_spare_space(layoutPtr, info, *row, FullTableID, TableID, BlockID, (base_suid) ? 1 : 0, SpareRegion, col, &outSU);
    } else {
    
        outSU	    = base_suid;
        outSU      += FullTableID * fulltable_depth;  				        /* offs to strt of FT */
        outSU	   += SpareSpace;						        /* skip rsvd spare space */
        outSU      += TableID * info->TableDepthInPUs * layoutPtr->SUsPerPU;   	        /* offs to strt of tble */
        outSU      += info->OffsetTable[BlockID][BlockOffset] * layoutPtr->SUsPerPU;	/* offs to the PU */
    }
    outSU          += TableOffset / (info->BlocksPerTable * info->PUsPerBlock);	        /* offs to the SU within a PU */
    
    /* convert SUs to sectors, and, if not aligned to SU boundary, add in offset to sector */
    *diskSector     = outSU*layoutPtr->sectorsPerStripeUnit + (raidSector % layoutPtr->sectorsPerStripeUnit);
}


void rf_MapParityDeclusteredPQ(
  RF_Raid_t       *raidPtr,
  RF_RaidAddr_t    raidSector,
  RF_RowCol_t     *row,
  RF_RowCol_t     *col,
  RF_SectorNum_t  *diskSector,
  int              remap)
{
  RF_RaidLayout_t *layoutPtr = &(raidPtr->Layout);
  RF_DeclusteredConfigInfo_t *info = (RF_DeclusteredConfigInfo_t *) layoutPtr->layoutSpecificInfo;
  RF_StripeNum_t SUID = raidSector / layoutPtr->sectorsPerStripeUnit;
  RF_StripeNum_t FullTableID, FullTableOffset, TableID, TableOffset; 
  RF_StripeNum_t BlockID, BlockOffset, RepIndex;
  RF_StripeCount_t sus_per_fulltable = info->SUsPerFullTable;
  RF_StripeCount_t fulltable_depth  = info->FullTableDepthInPUs * layoutPtr->SUsPerPU;
  RF_StripeNum_t base_suid = 0, outSU, SpareRegion, SpareSpace=0;
  
  rf_decluster_adjust_params(layoutPtr, &SUID, &sus_per_fulltable, &fulltable_depth, &base_suid);
  
  /* compute row & (possibly) spare space exactly as before */
  FullTableID     = SUID / sus_per_fulltable;
  *row            = FullTableID % raidPtr->numRow;
  FullTableID    /= raidPtr->numRow;			/* convert to fulltable ID on this disk */
  if ((raidPtr->Layout.map->flags & RF_DISTRIBUTE_SPARE)) {
    SpareRegion = FullTableID / info->FullTablesPerSpareRegion;
    SpareSpace  = SpareRegion * info->SpareSpaceDepthPerRegionInSUs;
  }
  
  /* compute BlockID and RepIndex exactly as before */
  FullTableOffset = SUID % sus_per_fulltable;
  TableID         = FullTableOffset / info->SUsPerTable;
    TableOffset     = FullTableOffset - TableID * info->SUsPerTable;
    BlockID         = TableOffset / info->PUsPerBlock;
    BlockOffset     = TableOffset - BlockID * info->PUsPerBlock;
    BlockID        %= info->BlocksPerTable;

  /* the parity block is in the position indicated by RepIndex */
  RepIndex        = (raidPtr->noRotate) ? info->PUsPerBlock : info->groupSize - 1 - TableID;
  *col	    = info->LayoutTable[BlockID][RepIndex];
  
  if (remap)
    RF_PANIC();
  
  /* compute sector as before, except use RepIndex instead of BlockOffset */
  outSU        = base_suid;
  outSU       += FullTableID * fulltable_depth;
  outSU	+= SpareSpace;						/* skip rsvd spare space */
  outSU       += TableID * info->TableDepthInPUs * layoutPtr->SUsPerPU;
  outSU       += info->OffsetTable[BlockID][RepIndex] * layoutPtr->SUsPerPU;
  outSU       += TableOffset / (info->BlocksPerTable * info->PUsPerBlock);
  
  *diskSector  = outSU*layoutPtr->sectorsPerStripeUnit + (raidSector % layoutPtr->sectorsPerStripeUnit);
}

void rf_MapQDeclusteredPQ(
  RF_Raid_t       *raidPtr,
  RF_RaidAddr_t    raidSector,
  RF_RowCol_t     *row,
  RF_RowCol_t     *col,
  RF_SectorNum_t  *diskSector,
  int              remap)
{
  RF_RaidLayout_t *layoutPtr = &(raidPtr->Layout);
  RF_DeclusteredConfigInfo_t *info = (RF_DeclusteredConfigInfo_t *) layoutPtr->layoutSpecificInfo;
  RF_StripeNum_t SUID = raidSector / layoutPtr->sectorsPerStripeUnit;
  RF_StripeNum_t FullTableID, FullTableOffset, TableID, TableOffset; 
  RF_StripeNum_t BlockID, BlockOffset, RepIndex, RepIndexQ;
  RF_StripeCount_t sus_per_fulltable = info->SUsPerFullTable;
  RF_StripeCount_t fulltable_depth  = info->FullTableDepthInPUs * layoutPtr->SUsPerPU;
  RF_StripeNum_t base_suid = 0, outSU, SpareRegion, SpareSpace=0;
  
  rf_decluster_adjust_params(layoutPtr, &SUID, &sus_per_fulltable, &fulltable_depth, &base_suid);
    
  /* compute row & (possibly) spare space exactly as before */
  FullTableID     = SUID / sus_per_fulltable;
  *row            = FullTableID % raidPtr->numRow;
  FullTableID    /= raidPtr->numRow;			/* convert to fulltable ID on this disk */
  if ((raidPtr->Layout.map->flags & RF_DISTRIBUTE_SPARE)) {
    SpareRegion = FullTableID / info->FullTablesPerSpareRegion;
    SpareSpace  = SpareRegion * info->SpareSpaceDepthPerRegionInSUs;
  }

  /* compute BlockID and RepIndex exactly as before */
  FullTableOffset = SUID % sus_per_fulltable;
  TableID         = FullTableOffset / info->SUsPerTable;
    TableOffset     = FullTableOffset - TableID * info->SUsPerTable;
    BlockID         = TableOffset / info->PUsPerBlock;
    BlockOffset     = TableOffset - BlockID * info->PUsPerBlock;
    BlockID        %= info->BlocksPerTable;

  /* the q block is in the position indicated by RepIndex */
  RepIndex        = (raidPtr->noRotate) ? info->PUsPerBlock : info->groupSize - 1 - TableID;
  RepIndexQ = ((RepIndex == (info->groupSize-1)) ? 0 : RepIndex+1);
  *col = info->LayoutTable[BlockID][RepIndexQ];
  
  if (remap)
    RF_PANIC();
  
  /* compute sector as before, except use RepIndex instead of BlockOffset */
  outSU        = base_suid;
  outSU       += FullTableID * fulltable_depth;
  outSU	+= SpareSpace;						/* skip rsvd spare space */
  outSU       += TableID * info->TableDepthInPUs * layoutPtr->SUsPerPU;
  outSU       += TableOffset / (info->BlocksPerTable * info->PUsPerBlock);

  outSU       += info->OffsetTable[BlockID][RepIndexQ] * layoutPtr->SUsPerPU;
  *diskSector = outSU*layoutPtr->sectorsPerStripeUnit + (raidSector % layoutPtr->sectorsPerStripeUnit);
}

/* returns an array of ints identifying the disks that comprise the stripe containing the indicated address.
 * the caller must _never_ attempt to modify this array.
 */
void rf_IdentifyStripeDeclusteredPQ(
  RF_Raid_t        *raidPtr,
  RF_RaidAddr_t     addr,
  RF_RowCol_t     **diskids,
  RF_RowCol_t      *outRow)
{
  RF_RaidLayout_t *layoutPtr           = &(raidPtr->Layout);
  RF_DeclusteredConfigInfo_t *info     = (RF_DeclusteredConfigInfo_t *) layoutPtr->layoutSpecificInfo;
  RF_StripeCount_t sus_per_fulltable = info->SUsPerFullTable;
  RF_StripeCount_t fulltable_depth   = info->FullTableDepthInPUs * layoutPtr->SUsPerPU;
  RF_StripeNum_t base_suid         = 0;
  RF_StripeNum_t SUID              = rf_RaidAddressToStripeUnitID(layoutPtr, addr);
  RF_StripeNum_t stripeID, FullTableID;
  int tableOffset;

  rf_decluster_adjust_params(layoutPtr, &SUID, &sus_per_fulltable, &fulltable_depth, &base_suid);
  FullTableID     = SUID / sus_per_fulltable;		/* fulltable ID within array (across rows) */
  *outRow         = FullTableID % raidPtr->numRow;
  stripeID = rf_StripeUnitIDToStripeID(layoutPtr, SUID);                     /* find stripe offset into array */
  tableOffset = (stripeID % info->BlocksPerTable);                        /* find offset into block design table */
  *diskids = info->LayoutTable[tableOffset];
}
