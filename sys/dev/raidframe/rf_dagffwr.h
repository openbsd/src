/*	$OpenBSD: rf_dagffwr.h,v 1.3 2002/12/16 07:01:03 tdeval Exp $	*/
/*	$NetBSD: rf_dagffwr.h,v 1.3 1999/02/05 00:06:08 oster Exp $	*/

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

#ifndef	_RF__RF_DAGFFWR_H_
#define	_RF__RF_DAGFFWR_H_

#include "rf_types.h"

/* Fault-free write DAG creation routines. */

void rf_CreateNonRedundantWriteDAG(RF_Raid_t *, RF_AccessStripeMap_t *,
	RF_DagHeader_t *, void *, RF_RaidAccessFlags_t, RF_AllocListElem_t *,
	RF_IoType_t);
void rf_CreateRAID0WriteDAG(RF_Raid_t *, RF_AccessStripeMap_t *,
	RF_DagHeader_t *, void *, RF_RaidAccessFlags_t, RF_AllocListElem_t *,
	RF_IoType_t);
void rf_CreateSmallWriteDAG(RF_Raid_t *, RF_AccessStripeMap_t *,
	RF_DagHeader_t *, void *, RF_RaidAccessFlags_t, RF_AllocListElem_t *);
void rf_CreateLargeWriteDAG(RF_Raid_t *, RF_AccessStripeMap_t *,
	RF_DagHeader_t *, void *, RF_RaidAccessFlags_t, RF_AllocListElem_t *);
void rf_CommonCreateLargeWriteDAG(RF_Raid_t *, RF_AccessStripeMap_t *,
	RF_DagHeader_t *, void *, RF_RaidAccessFlags_t, RF_AllocListElem_t *,
	int, int (*) (RF_DagNode_t *), int);
void rf_CommonCreateLargeWriteDAGFwd(RF_Raid_t *, RF_AccessStripeMap_t *,
	RF_DagHeader_t *, void *, RF_RaidAccessFlags_t, RF_AllocListElem_t *,
	int, int (*) (RF_DagNode_t *), int);
void rf_CommonCreateSmallWriteDAG(RF_Raid_t *, RF_AccessStripeMap_t *,
	RF_DagHeader_t *, void *, RF_RaidAccessFlags_t, RF_AllocListElem_t *,
	RF_RedFuncs_t *, RF_RedFuncs_t *);
void rf_CommonCreateSmallWriteDAGFwd(RF_Raid_t *, RF_AccessStripeMap_t *,
	RF_DagHeader_t *, void *, RF_RaidAccessFlags_t, RF_AllocListElem_t *,
	RF_RedFuncs_t *, RF_RedFuncs_t *);
void rf_CreateRaidOneWriteDAG(RF_Raid_t *, RF_AccessStripeMap_t *,
	RF_DagHeader_t *, void *, RF_RaidAccessFlags_t, RF_AllocListElem_t *);
void rf_CreateRaidOneWriteDAGFwd(RF_Raid_t *, RF_AccessStripeMap_t *,
	RF_DagHeader_t *, void *, RF_RaidAccessFlags_t, RF_AllocListElem_t *);

#endif	/* !_RF__RF_DAGFFWR_H_ */
