/*	$OpenBSD: rf_dagutils.h,v 1.3 2002/12/16 07:01:03 tdeval Exp $	*/
/*	$NetBSD: rf_dagutils.h,v 1.3 1999/02/05 00:06:08 oster Exp $	*/

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland, William V. Courtright II
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
 * rf_dagutils.h -- Header file for utility routines for manipulating DAGs.
 *
 *****************************************************************************/


#include "rf_types.h"
#include "rf_dagfuncs.h"
#include "rf_general.h"

#ifndef	_RF__RF_DAGUTILS_H_
#define	_RF__RF_DAGUTILS_H_

struct RF_RedFuncs_s {
	int	(*regular) (RF_DagNode_t *);
	char	 *RegularName;
	int	(*simple) (RF_DagNode_t *);
	char	 *SimpleName;
};

extern RF_RedFuncs_t rf_xorFuncs;
extern RF_RedFuncs_t rf_xorRecoveryFuncs;

void rf_InitNode(RF_DagNode_t *, RF_NodeStatus_t, int, int (*) (RF_DagNode_t *),
	int (*) (RF_DagNode_t *), int (*) (RF_DagNode_t *, int), int, int, int,
	int, RF_DagHeader_t *, char *, RF_AllocListElem_t *);

void rf_FreeDAG(RF_DagHeader_t *);

RF_PropHeader_t *rf_MakePropListEntry(RF_DagHeader_t *, int, int,
RF_PropHeader_t *, RF_AllocListElem_t *);

int  rf_ConfigureDAGs(RF_ShutdownList_t **);

RF_DagHeader_t *rf_AllocDAGHeader(void);

void rf_FreeDAGHeader(RF_DagHeader_t *);

void *rf_AllocBuffer(RF_Raid_t *, RF_DagHeader_t *, RF_PhysDiskAddr_t *,
	RF_AllocListElem_t *);

char *rf_NodeStatusString(RF_DagNode_t *);

void rf_PrintNodeInfoString(RF_DagNode_t *);

int  rf_AssignNodeNums(RF_DagHeader_t *);

int  rf_RecurAssignNodeNums(RF_DagNode_t *, int, int);

void rf_ResetDAGHeaderPointers(RF_DagHeader_t *, RF_DagHeader_t *);

void rf_RecurResetDAGHeaderPointers(RF_DagNode_t *, RF_DagHeader_t *);

void rf_PrintDAGList(RF_DagHeader_t *);

int  rf_ValidateDAG(RF_DagHeader_t *);

void rf_redirect_asm(RF_Raid_t *, RF_AccessStripeMap_t *);

void rf_MapUnaccessedPortionOfStripe(RF_Raid_t *, RF_RaidLayout_t *,
	RF_AccessStripeMap_t *, RF_DagHeader_t *, RF_AccessStripeMapHeader_t **,
	int *, char **, char **, RF_AllocListElem_t *);

int  rf_PDAOverlap(RF_RaidLayout_t *, RF_PhysDiskAddr_t *, RF_PhysDiskAddr_t *);

void rf_GenerateFailedAccessASMs(RF_Raid_t *, RF_AccessStripeMap_t *,
	RF_PhysDiskAddr_t *, RF_DagHeader_t *, RF_AccessStripeMapHeader_t **,
	int *, char **, char *, RF_AllocListElem_t *);

/* Flags used by RangeRestrictPDA. */
#define	RF_RESTRICT_NOBUFFER	0
#define	RF_RESTRICT_DOBUFFER	1

void rf_RangeRestrictPDA(RF_Raid_t *, RF_PhysDiskAddr_t *, RF_PhysDiskAddr_t *,
	int, int);

int  rf_compute_workload_shift(RF_Raid_t *, RF_PhysDiskAddr_t *);
void rf_SelectMirrorDiskIdle(RF_DagNode_t *);
void rf_SelectMirrorDiskPartition(RF_DagNode_t *);

#endif	/* ! _RF__RF_DAGUTILS_H_ */
