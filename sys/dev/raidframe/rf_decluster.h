/*	$OpenBSD: rf_decluster.h,v 1.1 1999/01/11 14:29:14 niklas Exp $	*/
/*	$NetBSD: rf_decluster.h,v 1.1 1998/11/13 04:20:28 oster Exp $	*/
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

/*----------------------------------------------------------------------
 *
 * decluster.h -- header file for declustered layout code
 *
 * Adapted from raidSim version July 1994
 * Created 10-21-92 (MCH)
 *
 *--------------------------------------------------------------------*/

/*
 * :  
 * Log: rf_decluster.h,v 
 * Revision 1.20  1996/07/29 14:05:12  jimz
 * fix numPUs/numRUs confusion (everything is now numRUs)
 * clean up some commenting, return values
 *
 * Revision 1.19  1996/07/13  00:00:59  jimz
 * sanitized generalized reconstruction architecture
 * cleaned up head sep, rbuf problems
 *
 * Revision 1.18  1996/06/19  17:53:48  jimz
 * move GetNumSparePUs, InstallSpareTable ops into layout switch
 *
 * Revision 1.17  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.16  1996/06/09  02:36:46  jimz
 * lots of little crufty cleanup- fixup whitespace
 * issues, comment #ifdefs, improve typing in some
 * places (esp size-related)
 *
 * Revision 1.15  1996/06/07  22:26:27  jimz
 * type-ify which_ru (RF_ReconUnitNum_t)
 *
 * Revision 1.14  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.13  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.12  1996/05/24  01:59:45  jimz
 * another checkpoint in code cleanup for release
 * time to sync kernel tree
 *
 * Revision 1.11  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.10  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.9  1995/12/01  15:58:23  root
 * added copyright info
 *
 * Revision 1.8  1995/11/17  18:57:02  wvcii
 * added prototyping to MapParity
 *
 * Revision 1.7  1995/07/02  15:08:31  holland
 * bug fixes related to getting distributed sparing numbers
 *
 * Revision 1.6  1995/06/23  13:41:18  robby
 * updeated to prototypes in rf_layout.h
 *
 */

#ifndef _RF__RF_DECLUSTER_H_
#define _RF__RF_DECLUSTER_H_

#include "rf_types.h"

/*
 * These structures define the tables used to locate the spare unit
 * associated with a particular data or parity unit, and to perform
 * the associated inverse mapping.
 */
struct RF_SpareTableEntry_s {
    u_int spareDisk;             /* disk to which this block is spared */
    u_int spareBlockOffsetInSUs; /* offset into spare table for that disk */
};

#define RF_SPAREMAP_NAME_LEN 128

/* this is the layout-specific info structure for the declustered layout.
 */
struct RF_DeclusteredConfigInfo_s {
  RF_StripeCount_t    groupSize;                      /* no. of stripe units per parity stripe */
  RF_RowCol_t       **LayoutTable;	                  /* the block design table */
  RF_RowCol_t       **OffsetTable;                    /* the sector offset table */
  RF_RowCol_t       **BlockTable;                     /* the block membership table */
  RF_StripeCount_t    SUsPerFullTable;                /* stripe units per full table */
  RF_StripeCount_t    SUsPerTable;                    /* stripe units per table */
  RF_StripeCount_t    PUsPerBlock;                    /* parity units per block */
  RF_StripeCount_t    SUsPerBlock;                    /* stripe units per block */
  RF_StripeCount_t    BlocksPerTable;                 /* block design tuples per table */
  RF_StripeCount_t    NumParityReps;                  /* tables per full table */
  RF_StripeCount_t    TableDepthInPUs;                /* PUs on one disk in 1 table */
  RF_StripeCount_t    FullTableDepthInPUs;            /* PUs on one disk in 1 fulltable */
  RF_StripeCount_t    FullTableLimitSUID;             /* SU where partial fulltables start */
  RF_StripeCount_t    ExtraTablesPerDisk;             /* # of tables in last fulltable */
  RF_SectorNum_t      DiskOffsetOfLastFullTableInSUs; /* disk offs of partial ft, if any */
  RF_StripeCount_t    numCompleteFullTablesPerDisk;   /* ft identifier of partial ft, if any */
  u_int               Lambda;                         /* the pair count in the block design */

  /* these are used only in the distributed-sparing case */
  RF_StripeCount_t    FullTablesPerSpareRegion;       /* # of ft's comprising 1 spare region */
  RF_StripeCount_t    TablesPerSpareRegion;           /* # of tables */
  RF_SectorCount_t    SpareSpaceDepthPerRegionInSUs;  /* spare space/disk/region */
  RF_SectorCount_t    SpareRegionDepthInSUs;          /* # of units/disk/region */
  RF_SectorNum_t      DiskOffsetOfLastSpareSpaceChunkInSUs; /* locates sp space after partial ft */
  RF_StripeCount_t    TotSparePUsPerDisk;             /* total number of spare PUs per disk */
  RF_StripeCount_t    NumCompleteSRs;
  RF_SpareTableEntry_t        **SpareTable;           /* remap table for spare space */
  char sparemap_fname[RF_SPAREMAP_NAME_LEN];          /* where to find sparemap. not used in kernel */
};

int rf_ConfigureDeclustered(RF_ShutdownList_t **listp, RF_Raid_t *raidPtr,
	RF_Config_t *cfgPtr);
int rf_ConfigureDeclusteredDS(RF_ShutdownList_t **listp, RF_Raid_t *raidPtr,
	RF_Config_t *cfgPtr);

void rf_MapSectorDeclustered(RF_Raid_t *raidPtr, RF_RaidAddr_t raidSector,
	RF_RowCol_t *row, RF_RowCol_t *col, RF_SectorNum_t *diskSector, int remap);
void rf_MapParityDeclustered(RF_Raid_t *raidPtr, RF_RaidAddr_t raidSector,
	RF_RowCol_t *row, RF_RowCol_t *col, RF_SectorNum_t *diskSector, int remap);
void rf_IdentifyStripeDeclustered(RF_Raid_t *raidPtr, RF_RaidAddr_t addr,
	RF_RowCol_t **diskids, RF_RowCol_t *outRow);
void rf_MapSIDToPSIDDeclustered(RF_RaidLayout_t *layoutPtr,
	RF_StripeNum_t stripeID, RF_StripeNum_t *psID,
	RF_ReconUnitNum_t *which_ru);
int  rf_InstallSpareTable(RF_Raid_t *raidPtr, RF_RowCol_t frow, RF_RowCol_t fcol);
void rf_FreeSpareTable(RF_Raid_t *raidPtr);

RF_HeadSepLimit_t rf_GetDefaultHeadSepLimitDeclustered(RF_Raid_t *raidPtr);
int rf_GetDefaultNumFloatingReconBuffersDeclustered(RF_Raid_t *raidPtr);

void rf_decluster_adjust_params(RF_RaidLayout_t *layoutPtr,
	RF_StripeNum_t *SUID, RF_StripeCount_t *sus_per_fulltable,
	RF_StripeCount_t *fulltable_depth, RF_StripeNum_t *base_suid);
void rf_remap_to_spare_space(
RF_RaidLayout_t *layoutPtr,
RF_DeclusteredConfigInfo_t *info, RF_RowCol_t row, RF_StripeNum_t FullTableID,
	RF_StripeNum_t TableID, RF_SectorNum_t BlockID, RF_StripeNum_t base_suid,
	RF_StripeNum_t SpareRegion, RF_RowCol_t *outCol, RF_StripeNum_t *outSU);
int rf_SetSpareTable(RF_Raid_t *raidPtr, void *data);
RF_ReconUnitCount_t rf_GetNumSpareRUsDeclustered(RF_Raid_t *raidPtr);

#endif /* !_RF__RF_DECLUSTER_H_ */
