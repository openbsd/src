/*	$OpenBSD: rf_dagdegrd.h,v 1.1 1999/01/11 14:29:07 niklas Exp $	*/
/*	$NetBSD: rf_dagdegrd.h,v 1.1 1998/11/13 04:20:27 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland, Daniel Stodolsky, William V. Courtright II
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

/* 
 * :  
 * Log: rf_dagdegrd.h,v 
 * Revision 1.6  1996/07/31 16:29:06  jimz
 * asm/asmap re-fix (EO merge)
 *
 * Revision 1.5  1996/07/31  15:34:40  jimz
 * evenodd changes; bugfixes for double-degraded archs, generalize
 * some formerly PQ-only functions
 *
 * Revision 1.4  1996/07/22  19:52:16  jimz
 * switched node params to RF_DagParam_t, a union of
 * a 64-bit int and a void *, for better portability
 * attempted hpux port, but failed partway through for
 * lack of a single C compiler capable of compiling all
 * source files
 *
 * Revision 1.3  1996/05/24  22:17:04  jimz
 * continue code + namespace cleanup
 * typed a bunch of flags
 *
 * Revision 1.2  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.1  1996/05/03  19:22:06  wvcii
 * Initial revision
 *
 */

#ifndef _RF__RF_DAGDEGRD_H_
#define _RF__RF_DAGDEGRD_H_

#include "rf_types.h"

/* degraded read DAG creation routines */
void rf_CreateRaidFiveDegradedReadDAG(RF_Raid_t *raidPtr,
	RF_AccessStripeMap_t *asmap, RF_DagHeader_t *dag_h, void *bp,
	RF_RaidAccessFlags_t flags, RF_AllocListElem_t *allocList);
void rf_CreateRaidOneDegradedReadDAG(RF_Raid_t *raidPtr,
	RF_AccessStripeMap_t *asmap, RF_DagHeader_t *dag_h, void *bp,
	RF_RaidAccessFlags_t flags, RF_AllocListElem_t *allocList);
void rf_CreateDegradedReadDAG(RF_Raid_t *raidPtr,
	RF_AccessStripeMap_t *asmap, RF_DagHeader_t *dag_h, void *bp,
	RF_RaidAccessFlags_t flags, RF_AllocListElem_t *allocList,
	RF_RedFuncs_t *recFunc);
void rf_CreateRaidCDegradedReadDAG(RF_Raid_t *raidPtr,
	RF_AccessStripeMap_t *asmap, RF_DagHeader_t *dag_h, void *bp,
	RF_RaidAccessFlags_t flags, RF_AllocListElem_t *allocList);
void rf_DD_GenerateFailedAccessASMs(RF_Raid_t *raidPtr,
	RF_AccessStripeMap_t *asmap, RF_PhysDiskAddr_t **pdap,
	int *nNodep, RF_PhysDiskAddr_t **pqpdap, int *nPQNodep,
	RF_AllocListElem_t *allocList);
void rf_DoubleDegRead(RF_Raid_t *raidPtr, RF_AccessStripeMap_t *asmap,
	RF_DagHeader_t *dag_h, void *bp, RF_RaidAccessFlags_t flags,
	RF_AllocListElem_t *allocList, char *redundantReadNodeName,
	char *recoveryNodeName, int (*recovFunc)(RF_DagNode_t *));

#endif /* !_RF__RF_DAGDEGRD_H_ */
