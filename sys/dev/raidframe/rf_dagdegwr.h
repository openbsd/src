/*	$OpenBSD: rf_dagdegwr.h,v 1.4 2002/12/16 07:01:03 tdeval Exp $	*/
/*	$NetBSD: rf_dagdegwr.h,v 1.4 1999/08/15 02:36:03 oster Exp $	*/

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


#ifndef	_RF__RF_DAGDEGWR_H_
#define	_RF__RF_DAGDEGWR_H_

/* Degraded write DAG creation routines. */

void rf_CreateDegradedWriteDAG(RF_Raid_t *, RF_AccessStripeMap_t *,
	RF_DagHeader_t *, void *, RF_RaidAccessFlags_t, RF_AllocListElem_t *);

void rf_CommonCreateSimpleDegradedWriteDAG(RF_Raid_t *, RF_AccessStripeMap_t *,
	RF_DagHeader_t *, void *, RF_RaidAccessFlags_t, RF_AllocListElem_t *,
	int, int (*) (RF_DagNode_t *), int);

void rf_WriteGenerateFailedAccessASMs(RF_Raid_t *, RF_AccessStripeMap_t *,
	RF_PhysDiskAddr_t **, int *, RF_PhysDiskAddr_t **, int *,
	RF_AllocListElem_t *);

void rf_DoubleDegSmallWrite(RF_Raid_t *, RF_AccessStripeMap_t *,
	RF_DagHeader_t *, void *, RF_RaidAccessFlags_t, RF_AllocListElem_t *,
	char *, char *, char *, int (*) (RF_DagNode_t *));

#endif	/* !_RF__RF_DAGDEGWR_H_ */
