/*	$OpenBSD: rf_decluster.h,v 1.3 2002/12/16 07:01:03 tdeval Exp $	*/
/*	$NetBSD: rf_decluster.h,v 1.3 1999/02/05 00:06:09 oster Exp $	*/

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

/*****************************************************************************
 *
 * decluster.h -- Header file for declustered layout code.
 *
 * Adapted from raidSim version July 1994
 * Created 10-21-92 (MCH)
 *
 *****************************************************************************/

#ifndef	_RF__RF_DECLUSTER_H_
#define	_RF__RF_DECLUSTER_H_

#include "rf_types.h"

/*
 * These structures define the tables used to locate the spare unit
 * associated with a particular data or parity unit, and to perform
 * the associated inverse mapping.
 */
struct RF_SpareTableEntry_s {
	u_int	spareDisk;		/* Disk where this block is spared. */
	u_int	spareBlockOffsetInSUs;	/*
					 * Offset into spare table for that
					 * disk.
					 */
};

#define	RF_SPAREMAP_NAME_LEN	128

/*
 * This is the layout-specific info structure for the declustered layout.
 */
struct RF_DeclusteredConfigInfo_s {
		/* Number of stripe units per parity stripe. */
	RF_StripeCount_t	  groupSize;
		/* The block design table. */
	RF_RowCol_t		**LayoutTable;
	RF_RowCol_t		**OffsetTable;
		/* The sector offset table. */
	RF_RowCol_t		**BlockTable;
		/* The block membership table. */
	RF_StripeCount_t	  SUsPerFullTable;
		/* Stripe units per full table. */
	RF_StripeCount_t	  SUsPerTable;
		/* Stripe units per table. */
	RF_StripeCount_t	  PUsPerBlock;
		/* Parity units per block. */
	RF_StripeCount_t	  SUsPerBlock;
		/* Stripe units per block. */
	RF_StripeCount_t	  BlocksPerTable;
		/* Block design tuples per table. */
	RF_StripeCount_t	  NumParityReps;
		/* Tables per full table. */
	RF_StripeCount_t	  TableDepthInPUs;
		/* PUs on one disk in 1 table. */
	RF_StripeCount_t	  FullTableDepthInPUs;
		/* PUs on one disk in 1 fulltable. */
	RF_StripeCount_t	  FullTableLimitSUID;
		/* SU where partial fulltables start. */
	RF_StripeCount_t	  ExtraTablesPerDisk;
		/* Number of tables in last fulltable. */
	RF_SectorNum_t		  DiskOffsetOfLastFullTableInSUs;
		/* Disk offsets of partial fulltable, if any. */
	RF_StripeCount_t	  numCompleteFullTablesPerDisk;
		/* Fulltable identifier of partial fulltable, if any. */
	u_int			  Lambda;
		/* The pair count in the block design. */

	/* These are used only in the distributed-sparing case. */
	RF_StripeCount_t	  FullTablesPerSpareRegion;
		/* Number of fulltables comprising 1 spare region. */
	RF_StripeCount_t	  TablesPerSpareRegion;
		/* Number of tables. */
	RF_SectorCount_t	  SpareSpaceDepthPerRegionInSUs;
		/* Spare space/disk/region. */
	RF_SectorCount_t	  SpareRegionDepthInSUs;
		/* Number of units/disk/region. */
	RF_SectorNum_t		  DiskOffsetOfLastSpareSpaceChunkInSUs;
		/* Locates spare space after partial fulltable. */
	RF_StripeCount_t	  TotSparePUsPerDisk;
		/* Total number of spare PUs per disk. */
	RF_StripeCount_t	  NumCompleteSRs;
	RF_SpareTableEntry_t	**SpareTable;
		/* Remap table for spare space. */
	char			  sparemap_fname[RF_SPAREMAP_NAME_LEN];
		/* Where to find sparemap. Not used in kernel. */
};

int  rf_ConfigureDeclustered(RF_ShutdownList_t **, RF_Raid_t *, RF_Config_t *);
int  rf_ConfigureDeclusteredDS(RF_ShutdownList_t **, RF_Raid_t *,
	RF_Config_t *);

void rf_MapSectorDeclustered(RF_Raid_t *, RF_RaidAddr_t, RF_RowCol_t *,
	RF_RowCol_t *, RF_SectorNum_t *, int);
void rf_MapParityDeclustered(RF_Raid_t *, RF_RaidAddr_t, RF_RowCol_t *,
	RF_RowCol_t *, RF_SectorNum_t *, int);
void rf_IdentifyStripeDeclustered(RF_Raid_t *, RF_RaidAddr_t, RF_RowCol_t **,
	RF_RowCol_t *);
void rf_MapSIDToPSIDDeclustered(RF_RaidLayout_t *, RF_StripeNum_t,
	RF_StripeNum_t *, RF_ReconUnitNum_t *);
int  rf_InstallSpareTable(RF_Raid_t *, RF_RowCol_t, RF_RowCol_t);
void rf_FreeSpareTable(RF_Raid_t *);

RF_HeadSepLimit_t rf_GetDefaultHeadSepLimitDeclustered(RF_Raid_t *);
int  rf_GetDefaultNumFloatingReconBuffersDeclustered(RF_Raid_t *);

void rf_decluster_adjust_params(RF_RaidLayout_t *, RF_StripeNum_t *,
	RF_StripeCount_t *, RF_StripeCount_t *, RF_StripeNum_t *);
void rf_remap_to_spare_space(RF_RaidLayout_t *, RF_DeclusteredConfigInfo_t *,
	RF_RowCol_t, RF_StripeNum_t, RF_StripeNum_t, RF_SectorNum_t,
	RF_StripeNum_t, RF_StripeNum_t, RF_RowCol_t *, RF_StripeNum_t *);
int  rf_SetSpareTable(RF_Raid_t *, void *);
RF_ReconUnitCount_t rf_GetNumSpareRUsDeclustered(RF_Raid_t *);

#endif	/* ! _RF__RF_DECLUSTER_H_ */
