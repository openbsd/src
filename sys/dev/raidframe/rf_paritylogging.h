/*	$OpenBSD: rf_paritylogging.h,v 1.1 1999/01/11 14:29:36 niklas Exp $	*/
/*	$NetBSD: rf_paritylogging.h,v 1.1 1998/11/13 04:20:32 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: William V. Courtright II
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

/* header file for Parity Logging */

/*
 * :  
 * Log: rf_paritylogging.h,v 
 * Revision 1.22  1996/07/27 23:36:08  jimz
 * Solaris port of simulator
 *
 * Revision 1.21  1996/07/13  00:00:59  jimz
 * sanitized generalized reconstruction architecture
 * cleaned up head sep, rbuf problems
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
 * Revision 1.14  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.13  1996/05/24  01:59:45  jimz
 * another checkpoint in code cleanup for release
 * time to sync kernel tree
 *
 * Revision 1.12  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.11  1995/12/06  20:56:25  wvcii
 * added prototypes
 *
 * Revision 1.10  1995/11/30  16:06:58  wvcii
 * added copyright info
 *
 * Revision 1.9  1995/11/17  19:53:08  wvcii
 * fixed bug in MapParityRegion prototype
 *
 * Revision 1.8  1995/11/17  19:09:24  wvcii
 * added prototypint to MapParity
 *
 * Revision 1.7  1995/11/07  15:28:17  wvcii
 * changed ParityLoggingDagSelect prototype
 * function no longer generates numHdrSucc, numTermAnt
 *
 * Revision 1.6  1995/07/07  00:16:50  wvcii
 * this version free from deadlock, fails parity verification
 *
 * Revision 1.5  1995/06/23  13:39:44  robby
 * updeated to prototypes in rf_layout.h
 *
 */

#ifndef _RF__RF_PARITYLOGGING_H_
#define _RF__RF_PARITYLOGGING_H_

int  rf_ConfigureParityLogging(RF_ShutdownList_t **listp, RF_Raid_t *raidPtr,
	RF_Config_t *cfgPtr);
int rf_GetDefaultNumFloatingReconBuffersParityLogging(RF_Raid_t *raidPtr);
RF_HeadSepLimit_t rf_GetDefaultHeadSepLimitParityLogging(RF_Raid_t *raidPtr);
RF_RegionId_t rf_MapRegionIDParityLogging(RF_Raid_t *raidPtr,
	RF_SectorNum_t address);
void rf_MapSectorParityLogging(RF_Raid_t *raidPtr, RF_RaidAddr_t raidSector,
	RF_RowCol_t *row, RF_RowCol_t *col, RF_SectorNum_t *diskSector,
	int remap);
void rf_MapParityParityLogging(RF_Raid_t *raidPtr, RF_RaidAddr_t raidSector,
	RF_RowCol_t *row, RF_RowCol_t *col, RF_SectorNum_t *diskSector,
	int remap);
void rf_MapLogParityLogging(RF_Raid_t *raidPtr, RF_RegionId_t regionID,
	RF_SectorNum_t regionOffset, RF_RowCol_t *row, RF_RowCol_t *col,
	RF_SectorNum_t *startSector);
void rf_MapRegionParity(RF_Raid_t *raidPtr, RF_RegionId_t regionID,
	RF_RowCol_t *row, RF_RowCol_t *col, RF_SectorNum_t *startSector,
	RF_SectorCount_t *numSector);
void rf_IdentifyStripeParityLogging(RF_Raid_t *raidPtr, RF_RaidAddr_t addr,
	RF_RowCol_t **diskids, RF_RowCol_t *outRow);
void rf_MapSIDToPSIDParityLogging(RF_RaidLayout_t *layoutPtr,
	RF_StripeNum_t stripeID, RF_StripeNum_t *psID,
	RF_ReconUnitNum_t *which_ru);
void rf_ParityLoggingDagSelect(RF_Raid_t *raidPtr, RF_IoType_t type,
	RF_AccessStripeMap_t *asmap, RF_VoidFuncPtr *createFunc);

#endif /* !_RF__RF_PARITYLOGGING_H_ */
