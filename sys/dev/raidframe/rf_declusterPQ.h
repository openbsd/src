/*	$OpenBSD: rf_declusterPQ.h,v 1.1 1999/01/11 14:29:14 niklas Exp $	*/
/*	$NetBSD: rf_declusterPQ.h,v 1.1 1998/11/13 04:20:28 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Daniel Stodolsky, Mark Holland
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

/* :  
 * Log: rf_declusterPQ.h,v 
 * Revision 1.13  1996/08/20 22:42:08  jimz
 * missing prototype of IdentifyStripeDeclusteredPQ added
 *
 * Revision 1.12  1996/07/13  00:00:59  jimz
 * sanitized generalized reconstruction architecture
 * cleaned up head sep, rbuf problems
 *
 * Revision 1.11  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.10  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.9  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.8  1996/05/24  01:59:45  jimz
 * another checkpoint in code cleanup for release
 * time to sync kernel tree
 *
 * Revision 1.7  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.6  1995/12/01  15:59:20  root
 * added copyright info
 *
 * Revision 1.5  1995/11/17  19:08:23  wvcii
 * added prototyping to MapParity
 *
 * Revision 1.4  1995/11/07  15:30:33  wvcii
 * changed PQDagSelect prototype
 * function no longer generates numHdrSucc, numTermAnt
 * removed ParityLoggingDagSelect prototype
 *
 * Revision 1.3  1995/06/23  13:40:57  robby
 * updeated to prototypes in rf_layout.h
 *
 * Revision 1.2  1995/05/02  22:46:53  holland
 * minor code cleanups.
 *
 * Revision 1.1  1994/11/19  20:26:57  danner
 * Initial revision
 *
 */

#ifndef _RF__RF_DECLUSTERPQ_H_
#define _RF__RF_DECLUSTERPQ_H_

#include "rf_types.h"

int  rf_ConfigureDeclusteredPQ(RF_ShutdownList_t **listp, RF_Raid_t *raidPtr,
	RF_Config_t *cfgPtr);
int rf_GetDefaultNumFloatingReconBuffersPQ(RF_Raid_t *raidPtr);
void rf_MapSectorDeclusteredPQ(RF_Raid_t *raidPtr, RF_RaidAddr_t raidSector,
	RF_RowCol_t *row, RF_RowCol_t *col, RF_SectorNum_t *diskSector, int remap);
void rf_MapParityDeclusteredPQ(RF_Raid_t *raidPtr, RF_RaidAddr_t raidSector,
	RF_RowCol_t *row, RF_RowCol_t *col, RF_SectorNum_t *diskSector, int remap);
void rf_MapQDeclusteredPQ(RF_Raid_t *raidPtr, RF_RaidAddr_t raidSector,
	RF_RowCol_t *row, RF_RowCol_t *col, RF_SectorNum_t *diskSector, int remap);
void rf_IdentifyStripeDeclusteredPQ(RF_Raid_t *raidPtr, RF_RaidAddr_t addr,
	RF_RowCol_t **diskids, RF_RowCol_t *outRow);

#endif /* !_RF__RF_DECLUSTERPQ_H_ */
