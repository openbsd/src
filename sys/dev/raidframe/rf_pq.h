/*	$OpenBSD: rf_pq.h,v 1.3 2002/12/16 07:01:04 tdeval Exp $	*/
/*	$NetBSD: rf_pq.h,v 1.3 1999/02/05 00:06:15 oster Exp $	*/

/*
 * rf_pq.h
 */
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Daniel Stodolsky
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

#ifndef	_RF__RF_PQ_H_
#define	_RF__RF_PQ_H_

#include "rf_archs.h"

extern RF_RedFuncs_t rf_pFuncs;
extern RF_RedFuncs_t rf_pRecoveryFuncs;

int  rf_RegularONPFunc(RF_DagNode_t *);
int  rf_SimpleONPFunc(RF_DagNode_t *);
int  rf_RecoveryPFunc(RF_DagNode_t *);
int  rf_RegularPFunc(RF_DagNode_t *);

#if	(RF_INCLUDE_DECL_PQ > 0) || (RF_INCLUDE_RAID6 > 0)

extern RF_RedFuncs_t rf_qFuncs;
extern RF_RedFuncs_t rf_qRecoveryFuncs;
extern RF_RedFuncs_t rf_pqRecoveryFuncs;

void rf_PQDagSelect(RF_Raid_t *, RF_IoType_t, RF_AccessStripeMap_t *,
	RF_VoidFuncPtr *);
RF_CREATE_DAG_FUNC_DECL(rf_PQCreateLargeWriteDAG);
int  rf_RegularONQFunc(RF_DagNode_t *);
int  rf_SimpleONQFunc(RF_DagNode_t *);
RF_CREATE_DAG_FUNC_DECL(rf_PQCreateSmallWriteDAG);
int  rf_RegularPQFunc(RF_DagNode_t *);
int  rf_RegularQFunc(RF_DagNode_t *);
void rf_Degraded_100_PQFunc(RF_DagNode_t *);
int  rf_RecoveryQFunc(RF_DagNode_t *);
int  rf_RecoveryPQFunc(RF_DagNode_t *);
void rf_PQ_DegradedWriteQFunc(RF_DagNode_t *);
void rf_IncQ(unsigned long *, unsigned long *, unsigned, unsigned);
void rf_PQ_recover(unsigned long *, unsigned long *, unsigned long *,
	unsigned long *, unsigned, unsigned, unsigned);

#endif	/* (RF_INCLUDE_DECL_PQ > 0) || (RF_INCLUDE_RAID6 > 0) */

#endif	/* !_RF__RF_PQ_H_ */
