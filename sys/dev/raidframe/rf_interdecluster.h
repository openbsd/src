/*	$OpenBSD: rf_interdecluster.h,v 1.1 1999/01/11 14:29:26 niklas Exp $	*/
/*	$NetBSD: rf_interdecluster.h,v 1.1 1998/11/13 04:20:30 oster Exp $	*/
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

/* rf_interdecluster.h
 * header file for Interleaved Declustering
 */

/*
 * :  
 * Log: rf_interdecluster.h,v 
 * Revision 1.13  1996/07/29 14:05:12  jimz
 * fix numPUs/numRUs confusion (everything is now numRUs)
 * clean up some commenting, return values
 *
 * Revision 1.12  1996/07/22  19:52:16  jimz
 * switched node params to RF_DagParam_t, a union of
 * a 64-bit int and a void *, for better portability
 * attempted hpux port, but failed partway through for
 * lack of a single C compiler capable of compiling all
 * source files
 *
 * Revision 1.11  1996/07/13  00:00:59  jimz
 * sanitized generalized reconstruction architecture
 * cleaned up head sep, rbuf problems
 *
 * Revision 1.10  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.9  1996/06/07  22:26:27  jimz
 * type-ify which_ru (RF_ReconUnitNum_t)
 *
 * Revision 1.8  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.7  1996/06/06  18:41:58  jimz
 * add RAIDIDagSelect
 *
 * Revision 1.6  1996/05/31  22:26:54  jimz
 * fix a lot of mapping problems, memory allocation problems
 * found some weird lock issues, fixed 'em
 * more code cleanup
 *
 * Revision 1.5  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.4  1996/05/24  01:59:45  jimz
 * another checkpoint in code cleanup for release
 * time to sync kernel tree
 *
 * Revision 1.3  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.2  1995/12/01  19:07:25  root
 * added copyright info
 *
 * Revision 1.1  1995/11/28  21:38:27  amiri
 * Initial revision
 */

#ifndef _RF__RF_INTERDECLUSTER_H_
#define _RF__RF_INTERDECLUSTER_H_

int  rf_ConfigureInterDecluster(RF_ShutdownList_t **listp, RF_Raid_t *raidPtr,
	RF_Config_t *cfgPtr);
int rf_GetDefaultNumFloatingReconBuffersInterDecluster(RF_Raid_t *raidPtr);
RF_HeadSepLimit_t rf_GetDefaultHeadSepLimitInterDecluster(RF_Raid_t *raidPtr);
RF_ReconUnitCount_t rf_GetNumSpareRUsInterDecluster(RF_Raid_t *raidPtr);
void rf_MapSectorInterDecluster(RF_Raid_t *raidPtr, RF_RaidAddr_t raidSector,
	RF_RowCol_t *row, RF_RowCol_t *col, RF_SectorNum_t *diskSector, int remap);
void rf_MapParityInterDecluster(RF_Raid_t *raidPtr, RF_RaidAddr_t raidSector,
	RF_RowCol_t *row, RF_RowCol_t *col, RF_SectorNum_t *diskSector, int remap);
void rf_IdentifyStripeInterDecluster(RF_Raid_t *raidPtr, RF_RaidAddr_t addr,
	RF_RowCol_t **diskids, RF_RowCol_t *outRow);
void rf_MapSIDToPSIDInterDecluster(RF_RaidLayout_t *layoutPtr,
	RF_StripeNum_t stripeID, RF_StripeNum_t *psID,
	RF_ReconUnitNum_t *which_ru);
void rf_RAIDIDagSelect(RF_Raid_t *raidPtr, RF_IoType_t type,
	RF_AccessStripeMap_t *asmap, RF_VoidFuncPtr *createFunc);

#endif /* !_RF__RF_INTERDECLUSTER_H_ */
