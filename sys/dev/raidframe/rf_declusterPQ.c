/*	$OpenBSD: rf_declusterPQ.c,v 1.4 2002/12/16 07:01:03 tdeval Exp $	*/
/*	$NetBSD: rf_declusterPQ.c,v 1.3 1999/02/05 00:06:09 oster Exp $	*/

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

/*****************************************************************************
 * rf_declusterPQ.c
 *
 * Mapping code for declustered P & Q or declustered EvenOdd.
 * Much code borrowed from rf_decluster.c
 *
 *****************************************************************************/


#include "rf_types.h"
#include "rf_raid.h"
#include "rf_configure.h"
#include "rf_decluster.h"
#include "rf_declusterPQ.h"
#include "rf_debugMem.h"
#include "rf_utils.h"
#include "rf_alloclist.h"
#include "rf_general.h"

/* Configuration code. */

int
rf_ConfigureDeclusteredPQ(RF_ShutdownList_t **listp, RF_Raid_t *raidPtr,
    RF_Config_t *cfgPtr)
{
	RF_RaidLayout_t *layoutPtr = &(raidPtr->Layout);
	int b, v, k, r, lambda;	/* block design params */
	int i, j, l;
	int *first_avail_slot;
	int complete_FT_count, SUID;
	RF_DeclusteredConfigInfo_t *info;
	int numCompleteFullTablesPerDisk;
	int PUsPerDisk, spareRegionDepthInPUs, extraPUsPerDisk;
	int numCompleteSpareRegionsPerDisk = 0;
	int totSparePUsPerDisk;
	int diskOffsetOfLastFullTableInSUs, SpareSpaceInSUs;
	char *cfgBuf = (char *)(cfgPtr->layoutSpecific);

	cfgBuf += RF_SPAREMAP_NAME_LEN;

	b = *((int *) cfgBuf);
	cfgBuf += sizeof(int);
	v = *((int *) cfgBuf);
	cfgBuf += sizeof(int);
	k = *((int *) cfgBuf);
	cfgBuf += sizeof(int);
	r = *((int *) cfgBuf);
	cfgBuf += sizeof(int);
	lambda = *((int *) cfgBuf);
	cfgBuf += sizeof(int);
	raidPtr->noRotate = *((int *) cfgBuf);
	cfgBuf += sizeof(int);

	if (k <= 2) {
#ifdef	RAIDDEBUG
		printf("RAIDFRAME: k=%d, minimum value 2\n", k);
#endif	/* RAIDDEBUG */
		return (EINVAL);
	}
	/* 1. Create layout specific structure. */
	RF_MallocAndAdd(info, sizeof(RF_DeclusteredConfigInfo_t),
	    (RF_DeclusteredConfigInfo_t *), raidPtr->cleanupList);
	if (info == NULL)
		return (ENOMEM);
	layoutPtr->layoutSpecificInfo = (void *) info;

	/*
	 * 2. The sparemaps are generated assuming that parity is rotated, so
	 * we issue a warning if both distributed sparing and no-rotate are on
	 * at the same time.
	 */
	if ((raidPtr->Layout.map->flags & RF_DISTRIBUTE_SPARE) &&
	    raidPtr->noRotate) {
		RF_ERRORMSG("Warning:  distributed sparing specified without"
		    " parity rotation.\n");
	}
	if (raidPtr->numCol != v) {
		RF_ERRORMSG2("RAID: config error: table element count (%d)"
		    " not equal to no. of cols (%d).\n", v, raidPtr->numCol);
		return (EINVAL);
	}
	/* 3. Set up the values used in devRaidMap. */
	info->BlocksPerTable = b;
	info->NumParityReps = info->groupSize = k;
	info->PUsPerBlock = k - 2;	/* PQ */
	info->SUsPerTable = b * info->PUsPerBlock * layoutPtr->SUsPerPU;
		/* b blks, k-1 SUs each. */
	info->SUsPerFullTable = k * info->SUsPerTable;	/* Rot k times. */
	info->SUsPerBlock = info->PUsPerBlock * layoutPtr->SUsPerPU;
	info->TableDepthInPUs = (b * k) / v;
	info->FullTableDepthInPUs = info->TableDepthInPUs * k;
		/* k repetitions. */

	/* Used only in distributed sparing case. */
	info->FullTablesPerSpareRegion = (v - 1) / rf_gcd(r, v - 1);
		/* (v-1)/gcd fulltables. */
	info->TablesPerSpareRegion = k * info->FullTablesPerSpareRegion;
	info->SpareSpaceDepthPerRegionInSUs = (r * info->TablesPerSpareRegion
	    / (v - 1)) * layoutPtr->SUsPerPU;

	/* Check to make sure the block design is sufficiently small. */
	if ((raidPtr->Layout.map->flags & RF_DISTRIBUTE_SPARE)) {
		if (info->FullTableDepthInPUs * layoutPtr->SUsPerPU +
		    info->SpareSpaceDepthPerRegionInSUs >
		    layoutPtr->stripeUnitsPerDisk) {
			RF_ERRORMSG3("RAID: config error: Full Table depth"
			    " (%d) + Spare Space (%d) larger than disk size"
			    " (%d) (BD too big).\n",
			    (int)info->FullTableDepthInPUs,
			    (int)info->SpareSpaceDepthPerRegionInSUs,
			    (int)layoutPtr->stripeUnitsPerDisk);
			return (EINVAL);
		}
	} else {
		if (info->TableDepthInPUs * layoutPtr->SUsPerPU >
		    layoutPtr->stripeUnitsPerDisk) {
			RF_ERRORMSG2("RAID: config error: Table depth (%d)"
			    " larger than disk size (%d) (BD too big).\n",
			    (int) (info->TableDepthInPUs * layoutPtr->SUsPerPU),
			    (int) layoutPtr->stripeUnitsPerDisk);
			return (EINVAL);
		}
	}

	/*
	 * Compute the size of each disk, and the number of tables in the last
	 * fulltable (which need not be complete).
	 */
	if ((raidPtr->Layout.map->flags & RF_DISTRIBUTE_SPARE)) {

		PUsPerDisk = layoutPtr->stripeUnitsPerDisk /
		    layoutPtr->SUsPerPU;
		spareRegionDepthInPUs = (info->TablesPerSpareRegion *
		    info->TableDepthInPUs + (info->TablesPerSpareRegion *
		    info->TableDepthInPUs) / (v - 1));
		info->SpareRegionDepthInSUs = spareRegionDepthInPUs *
		    layoutPtr->SUsPerPU;

		numCompleteSpareRegionsPerDisk = PUsPerDisk /
		    spareRegionDepthInPUs;
		info->NumCompleteSRs = numCompleteSpareRegionsPerDisk;
		extraPUsPerDisk = PUsPerDisk % spareRegionDepthInPUs;

		/*
		 * Assume conservatively that we need the full amount of spare
		 * space in one region in order to provide spares for the
		 * partial spare region at the end of the array.  We set "i"
		 * to the number of tables in the partial spare region.  This
		 * may actually include some fulltables.
		 */
		extraPUsPerDisk -= (info->SpareSpaceDepthPerRegionInSUs /
		    layoutPtr->SUsPerPU);
		if (extraPUsPerDisk <= 0)
			i = 0;
		else
			i = extraPUsPerDisk / info->TableDepthInPUs;

		complete_FT_count = raidPtr->numRow *
		    (numCompleteSpareRegionsPerDisk *
		    (info->TablesPerSpareRegion / k) + i / k);
		info->FullTableLimitSUID = complete_FT_count *
		    info->SUsPerFullTable;
		info->ExtraTablesPerDisk = i % k;

		/*
		 * Note that in the last spare region, the spare space is
		 * complete even though data/parity space is not.
		 */
		totSparePUsPerDisk = (numCompleteSpareRegionsPerDisk + 1) *
		    (info->SpareSpaceDepthPerRegionInSUs / layoutPtr->SUsPerPU);
		info->TotSparePUsPerDisk = totSparePUsPerDisk;

		layoutPtr->stripeUnitsPerDisk =
		    ((complete_FT_count / raidPtr->numRow) *
		    info->FullTableDepthInPUs +	/* data & parity space */
		    info->ExtraTablesPerDisk * info->TableDepthInPUs +
		    totSparePUsPerDisk	/* spare space */
		    ) * layoutPtr->SUsPerPU;
		layoutPtr->dataStripeUnitsPerDisk =
		    (complete_FT_count * info->FullTableDepthInPUs +
		    info->ExtraTablesPerDisk * info->TableDepthInPUs)
		    * layoutPtr->SUsPerPU * (k - 1) / k;

	} else {
		/*
		 * Non-dist spare case:  force each disk to contain an
		 * integral number of tables.
		 */
		layoutPtr->stripeUnitsPerDisk /=
		    (info->TableDepthInPUs * layoutPtr->SUsPerPU);
		layoutPtr->stripeUnitsPerDisk *=
		    (info->TableDepthInPUs * layoutPtr->SUsPerPU);

		/*
		 * Compute the number of tables in the last fulltable, which
		 * need not be complete.
		 */
		complete_FT_count = ((layoutPtr->stripeUnitsPerDisk /
		    layoutPtr->SUsPerPU) / info->FullTableDepthInPUs) *
		    raidPtr->numRow;

		info->FullTableLimitSUID = complete_FT_count *
		    info->SUsPerFullTable;
		info->ExtraTablesPerDisk = ((layoutPtr->stripeUnitsPerDisk /
		    layoutPtr->SUsPerPU) / info->TableDepthInPUs) % k;
	}

	raidPtr->sectorsPerDisk = layoutPtr->stripeUnitsPerDisk *
	    layoutPtr->sectorsPerStripeUnit;

	/*
	 * Find the disk offset of the stripe unit where the last
	 * fulltable starts.
	 */
	numCompleteFullTablesPerDisk = complete_FT_count / raidPtr->numRow;
	diskOffsetOfLastFullTableInSUs = numCompleteFullTablesPerDisk *
	    info->FullTableDepthInPUs * layoutPtr->SUsPerPU;
	if ((raidPtr->Layout.map->flags & RF_DISTRIBUTE_SPARE)) {
		SpareSpaceInSUs = numCompleteSpareRegionsPerDisk *
		    info->SpareSpaceDepthPerRegionInSUs;
		diskOffsetOfLastFullTableInSUs += SpareSpaceInSUs;
		info->DiskOffsetOfLastSpareSpaceChunkInSUs =
		    diskOffsetOfLastFullTableInSUs + info->ExtraTablesPerDisk *
		    info->TableDepthInPUs * layoutPtr->SUsPerPU;
	}
	info->DiskOffsetOfLastFullTableInSUs = diskOffsetOfLastFullTableInSUs;
	info->numCompleteFullTablesPerDisk = numCompleteFullTablesPerDisk;

	/* 4. Create and initialize the lookup tables. */
	info->LayoutTable = rf_make_2d_array(b, k, raidPtr->cleanupList);
	if (info->LayoutTable == NULL)
		return (ENOMEM);

	info->OffsetTable = rf_make_2d_array(b, k, raidPtr->cleanupList);
	if (info->OffsetTable == NULL)
		return (ENOMEM);

	info->BlockTable = rf_make_2d_array(info->TableDepthInPUs *
	    layoutPtr->SUsPerPU, raidPtr->numCol, raidPtr->cleanupList);
	if (info->BlockTable == NULL)
		return (ENOMEM);

	first_avail_slot = (int *) rf_make_1d_array(v, NULL);
	if (first_avail_slot == NULL)
		return (ENOMEM);

	for (i = 0; i < b; i++)
		for (j = 0; j < k; j++)
			info->LayoutTable[i][j] = *cfgBuf++;

	/* Initialize offset table. */
	for (i = 0; i < b; i++)
		for (j = 0; j < k; j++) {
			info->OffsetTable[i][j] =
			    first_avail_slot[info->LayoutTable[i][j]];
			first_avail_slot[info->LayoutTable[i][j]]++;
		}

	/* Initialize block table. */
	for (SUID = l = 0; l < layoutPtr->SUsPerPU; l++) {
		for (i = 0; i < b; i++) {
			for (j = 0; j < k; j++) {
				info->BlockTable[(info->OffsetTable[i][j] *
				     layoutPtr->SUsPerPU) + l]
				    [info->LayoutTable[i][j]] = SUID;
			}
			SUID++;
		}
	}

	rf_free_1d_array(first_avail_slot, v);

	/* 5. Set up the remaining redundant-but-useful parameters. */

	raidPtr->totalSectors = (k * complete_FT_count + raidPtr->numRow *
	    info->ExtraTablesPerDisk) * info->SUsPerTable *
	    layoutPtr->sectorsPerStripeUnit;
	layoutPtr->numStripe = (raidPtr->totalSectors /
	    layoutPtr->sectorsPerStripeUnit) / (k - 2);

	/*
	 * Strange evaluation order below to try and minimize overflow
	 * problems.
	 */

	layoutPtr->dataSectorsPerStripe = (k - 2) *
	    layoutPtr->sectorsPerStripeUnit;
	layoutPtr->bytesPerStripeUnit = layoutPtr->sectorsPerStripeUnit <<
	    raidPtr->logBytesPerSector;
	layoutPtr->numDataCol = k - 2;
	layoutPtr->numParityCol = 2;

	return (0);
}

int
rf_GetDefaultNumFloatingReconBuffersPQ(RF_Raid_t *raidPtr)
{
	int def_decl;

	def_decl = rf_GetDefaultNumFloatingReconBuffersDeclustered(raidPtr);
	return (RF_MAX(3 * raidPtr->numCol, def_decl));
}

void
rf_MapSectorDeclusteredPQ(RF_Raid_t *raidPtr, RF_RaidAddr_t raidSector,
    RF_RowCol_t *row, RF_RowCol_t *col, RF_SectorNum_t *diskSector, int remap)
{
	RF_RaidLayout_t *layoutPtr = &(raidPtr->Layout);
	RF_DeclusteredConfigInfo_t *info =
	    (RF_DeclusteredConfigInfo_t *) layoutPtr->layoutSpecificInfo;
	RF_StripeNum_t SUID = raidSector / layoutPtr->sectorsPerStripeUnit;
	RF_StripeNum_t FullTableID, FullTableOffset, TableID, TableOffset;
	RF_StripeNum_t BlockID, BlockOffset, RepIndex;
	RF_StripeCount_t sus_per_fulltable = info->SUsPerFullTable;
	RF_StripeCount_t fulltable_depth = info->FullTableDepthInPUs *
	    layoutPtr->SUsPerPU;
	RF_StripeNum_t base_suid = 0, outSU, SpareRegion = 0, SpareSpace = 0;

	rf_decluster_adjust_params(layoutPtr, &SUID, &sus_per_fulltable,
	    &fulltable_depth, &base_suid);

	/* Fulltable ID within array (across rows). */
	FullTableID = SUID / sus_per_fulltable;
	*row = FullTableID % raidPtr->numRow;

	/* Convert to fulltable ID on this disk. */
	FullTableID /= raidPtr->numRow;

	if ((raidPtr->Layout.map->flags & RF_DISTRIBUTE_SPARE)) {
		SpareRegion = FullTableID / info->FullTablesPerSpareRegion;
		SpareSpace = SpareRegion * info->SpareSpaceDepthPerRegionInSUs;
	}
	FullTableOffset = SUID % sus_per_fulltable;
	TableID = FullTableOffset / info->SUsPerTable;
	TableOffset = FullTableOffset - TableID * info->SUsPerTable;
	BlockID = TableOffset / info->PUsPerBlock;
	BlockOffset = TableOffset - BlockID * info->PUsPerBlock;
	BlockID %= info->BlocksPerTable;
	RF_ASSERT(BlockOffset < info->groupSize - 2);
	/*
	 * TableIDs go from 0 .. GroupSize-1 inclusive.
	 * PUsPerBlock is k-2.
	 * We want the tableIDs to rotate from the
	 * right, so use GroupSize.
	 */
	RepIndex = info->groupSize - 1 - TableID;
	RF_ASSERT(RepIndex >= 0);
	if (!raidPtr->noRotate) {
		if (TableID == 0)
			/* P on last drive, Q on first. */
			BlockOffset++;
		else
			/* Skip over PQ. */
			BlockOffset += ((BlockOffset >= RepIndex) ? 2 : 0);

		RF_ASSERT(BlockOffset < info->groupSize);
		*col = info->LayoutTable[BlockID][BlockOffset];
	}
	/* Remap to distributed spare space if indicated. */
	if (remap) {
		rf_remap_to_spare_space(layoutPtr, info, *row, FullTableID,
		    TableID, BlockID, (base_suid) ? 1 : 0, SpareRegion, col,
		    &outSU);
	} else {

		outSU = base_suid;
		outSU += FullTableID * fulltable_depth;
			/* Offset to strt of FT. */
		outSU += SpareSpace;
			/* Skip reserved spare space. */
		outSU += TableID * info->TableDepthInPUs * layoutPtr->SUsPerPU;
			/* Offset to start of table. */
		outSU += info->OffsetTable[BlockID][BlockOffset] *
		    layoutPtr->SUsPerPU;
			/* Offset to the PU. */
	}
	outSU += TableOffset / (info->BlocksPerTable * info->PUsPerBlock);
		/* Offset to the SU within a PU. */

	/*
	 * Convert SUs to sectors, and, if not aligned to SU boundary, add in
	 * offset to sector.
	 */
	*diskSector = outSU * layoutPtr->sectorsPerStripeUnit +
	    (raidSector % layoutPtr->sectorsPerStripeUnit);
}


void
rf_MapParityDeclusteredPQ(RF_Raid_t *raidPtr, RF_RaidAddr_t raidSector,
    RF_RowCol_t *row, RF_RowCol_t *col, RF_SectorNum_t *diskSector, int remap)
{
	RF_RaidLayout_t *layoutPtr = &(raidPtr->Layout);
	RF_DeclusteredConfigInfo_t *info =
	    (RF_DeclusteredConfigInfo_t *) layoutPtr->layoutSpecificInfo;
	RF_StripeNum_t SUID = raidSector / layoutPtr->sectorsPerStripeUnit;
	RF_StripeNum_t FullTableID, FullTableOffset, TableID, TableOffset;
	RF_StripeNum_t BlockID, BlockOffset, RepIndex;
	RF_StripeCount_t sus_per_fulltable = info->SUsPerFullTable;
	RF_StripeCount_t fulltable_depth = info->FullTableDepthInPUs *
	    layoutPtr->SUsPerPU;
	RF_StripeNum_t base_suid = 0, outSU, SpareRegion, SpareSpace = 0;

	rf_decluster_adjust_params(layoutPtr, &SUID, &sus_per_fulltable,
	    &fulltable_depth, &base_suid);

	/* Compute row & (possibly) spare space exactly as before. */
	FullTableID = SUID / sus_per_fulltable;
	*row = FullTableID % raidPtr->numRow;
	/* Convert to fulltable ID on this disk. */
	FullTableID /= raidPtr->numRow;
	if ((raidPtr->Layout.map->flags & RF_DISTRIBUTE_SPARE)) {
		SpareRegion = FullTableID / info->FullTablesPerSpareRegion;
		SpareSpace = SpareRegion * info->SpareSpaceDepthPerRegionInSUs;
	}
	/* Compute BlockID and RepIndex exactly as before. */
	FullTableOffset = SUID % sus_per_fulltable;
	TableID = FullTableOffset / info->SUsPerTable;
	TableOffset = FullTableOffset - TableID * info->SUsPerTable;
	BlockID = TableOffset / info->PUsPerBlock;
	BlockOffset = TableOffset - BlockID * info->PUsPerBlock;
	BlockID %= info->BlocksPerTable;

	/* The parity block is in the position indicated by RepIndex. */
	RepIndex = (raidPtr->noRotate) ?
	    info->PUsPerBlock : info->groupSize - 1 - TableID;
	*col = info->LayoutTable[BlockID][RepIndex];

	if (remap)
		RF_PANIC();

	/*
	 * Compute sector as before, except use RepIndex instead of
	 * BlockOffset.
	 */
	outSU = base_suid;
	outSU += FullTableID * fulltable_depth;
	outSU += SpareSpace;	/* skip rsvd spare space */
	outSU += TableID * info->TableDepthInPUs * layoutPtr->SUsPerPU;
	outSU += info->OffsetTable[BlockID][RepIndex] * layoutPtr->SUsPerPU;
	outSU += TableOffset / (info->BlocksPerTable * info->PUsPerBlock);

	*diskSector = outSU * layoutPtr->sectorsPerStripeUnit +
	    (raidSector % layoutPtr->sectorsPerStripeUnit);
}

void
rf_MapQDeclusteredPQ(RF_Raid_t *raidPtr, RF_RaidAddr_t raidSector,
    RF_RowCol_t *row, RF_RowCol_t *col, RF_SectorNum_t *diskSector, int remap)
{
	RF_RaidLayout_t *layoutPtr = &(raidPtr->Layout);
	RF_DeclusteredConfigInfo_t *info =
	    (RF_DeclusteredConfigInfo_t *) layoutPtr->layoutSpecificInfo;
	RF_StripeNum_t SUID = raidSector / layoutPtr->sectorsPerStripeUnit;
	RF_StripeNum_t FullTableID, FullTableOffset, TableID, TableOffset;
	RF_StripeNum_t BlockID, BlockOffset, RepIndex, RepIndexQ;
	RF_StripeCount_t sus_per_fulltable = info->SUsPerFullTable;
	RF_StripeCount_t fulltable_depth = info->FullTableDepthInPUs *
	    layoutPtr->SUsPerPU;
	RF_StripeNum_t base_suid = 0, outSU, SpareRegion, SpareSpace = 0;

	rf_decluster_adjust_params(layoutPtr, &SUID, &sus_per_fulltable,
	    &fulltable_depth, &base_suid);

	/* Compute row & (possibly) spare space exactly as before. */
	FullTableID = SUID / sus_per_fulltable;
	*row = FullTableID % raidPtr->numRow;
	/* Convert to fulltable ID on this disk. */
	FullTableID /= raidPtr->numRow;
	if ((raidPtr->Layout.map->flags & RF_DISTRIBUTE_SPARE)) {
		SpareRegion = FullTableID / info->FullTablesPerSpareRegion;
		SpareSpace = SpareRegion * info->SpareSpaceDepthPerRegionInSUs;
	}
	/* Compute BlockID and RepIndex exactly as before. */
	FullTableOffset = SUID % sus_per_fulltable;
	TableID = FullTableOffset / info->SUsPerTable;
	TableOffset = FullTableOffset - TableID * info->SUsPerTable;
	BlockID = TableOffset / info->PUsPerBlock;
	BlockOffset = TableOffset - BlockID * info->PUsPerBlock;
	BlockID %= info->BlocksPerTable;

	/* The q block is in the position indicated by RepIndex. */
	RepIndex = (raidPtr->noRotate) ?
	    info->PUsPerBlock : info->groupSize - 1 - TableID;
	RepIndexQ = ((RepIndex == (info->groupSize - 1)) ? 0 : RepIndex + 1);
	*col = info->LayoutTable[BlockID][RepIndexQ];

	if (remap)
		RF_PANIC();

	/*
	 * Compute sector as before, except use RepIndex instead of
	 * BlockOffset.
	 */
	outSU = base_suid;
	outSU += FullTableID * fulltable_depth;
	outSU += SpareSpace;	/* skip rsvd spare space */
	outSU += TableID * info->TableDepthInPUs * layoutPtr->SUsPerPU;
	outSU += TableOffset / (info->BlocksPerTable * info->PUsPerBlock);

	outSU += info->OffsetTable[BlockID][RepIndexQ] * layoutPtr->SUsPerPU;
	*diskSector = outSU * layoutPtr->sectorsPerStripeUnit +
	    (raidSector % layoutPtr->sectorsPerStripeUnit);
}

/*
 * Returns an array of ints identifying the disks that comprise the stripe
 * containing the indicated address.
 * The caller must _never_ attempt to modify this array.
 */
void
rf_IdentifyStripeDeclusteredPQ(RF_Raid_t *raidPtr, RF_RaidAddr_t addr,
    RF_RowCol_t **diskids, RF_RowCol_t *outRow)
{
	RF_RaidLayout_t *layoutPtr = &(raidPtr->Layout);
	RF_DeclusteredConfigInfo_t *info =
	    (RF_DeclusteredConfigInfo_t *) layoutPtr->layoutSpecificInfo;
	RF_StripeCount_t sus_per_fulltable = info->SUsPerFullTable;
	RF_StripeCount_t fulltable_depth = info->FullTableDepthInPUs *
	    layoutPtr->SUsPerPU;
	RF_StripeNum_t base_suid = 0;
	RF_StripeNum_t SUID = rf_RaidAddressToStripeUnitID(layoutPtr, addr);
	RF_StripeNum_t stripeID, FullTableID;
	int tableOffset;

	rf_decluster_adjust_params(layoutPtr, &SUID, &sus_per_fulltable,
	    &fulltable_depth, &base_suid);
	/* Fulltable ID within array (across rows). */
	FullTableID = SUID / sus_per_fulltable;
	*outRow = FullTableID % raidPtr->numRow;
	/* Find stripe offset into array. */
	stripeID = rf_StripeUnitIDToStripeID(layoutPtr, SUID);
	/* Find offset into block design table. */
	tableOffset = (stripeID % info->BlocksPerTable);
	*diskids = info->LayoutTable[tableOffset];
}
