/*	$OpenBSD: rf_dagutils.h,v 1.1 1999/01/11 14:29:12 niklas Exp $	*/
/*	$NetBSD: rf_dagutils.h,v 1.1 1998/11/13 04:20:28 oster Exp $	*/
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

/*************************************************************************
 *
 * rf_dagutils.h -- header file for utility routines for manipulating DAGs
 *
 *************************************************************************/

/*
 * :  
 * Log: rf_dagutils.h,v 
 * Revision 1.19  1996/07/22 19:52:16  jimz
 * switched node params to RF_DagParam_t, a union of
 * a 64-bit int and a void *, for better portability
 * attempted hpux port, but failed partway through for
 * lack of a single C compiler capable of compiling all
 * source files
 *
 * Revision 1.18  1996/07/15  17:22:18  jimz
 * nit-pick code cleanup
 * resolve stdlib problems on DEC OSF
 *
 * Revision 1.17  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.16  1996/06/06  17:27:46  jimz
 * added another select mirror func (partitioning), changed names so dag
 * creation routines can use the appropriate one
 *
 * fixed old idle mirror func to pick closest arm if queue lengths are equal
 *
 * Revision 1.15  1996/06/03  23:28:26  jimz
 * more bugfixes
 * check in tree to sync for IPDS runs with current bugfixes
 * there still may be a problem with threads in the script test
 * getting I/Os stuck- not trivially reproducible (runs ~50 times
 * in a row without getting stuck)
 *
 * Revision 1.14  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.13  1996/05/24  22:17:04  jimz
 * continue code + namespace cleanup
 * typed a bunch of flags
 *
 * Revision 1.12  1996/05/24  04:28:55  jimz
 * release cleanup ckpt
 *
 * Revision 1.11  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.10  1996/05/23  00:33:23  jimz
 * code cleanup: move all debug decls to rf_options.c, all extern
 * debug decls to rf_options.h, all debug vars preceded by rf_
 *
 * Revision 1.9  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.8  1996/05/08  21:01:24  jimz
 * fixed up enum type names that were conflicting with other
 * enums and function names (ie, "panic")
 * future naming trends will be towards RF_ and rf_ for
 * everything raidframe-related
 *
 * Revision 1.7  1996/05/03  19:55:27  wvcii
 * added misc routines from old dag creation files
 *
 * Revision 1.6  1995/12/01  15:57:28  root
 * added copyright info
 *
 * Revision 1.5  1995/11/07  16:21:36  wvcii
 * modified InitNode and InitNodeFromBuf prototypes
 *
 */

#include "rf_types.h"
#include "rf_dagfuncs.h"
#include "rf_general.h"

#ifndef _RF__RF_DAGUTILS_H_
#define _RF__RF_DAGUTILS_H_

struct RF_RedFuncs_s {
  int   (*regular)(RF_DagNode_t *);
  char   *RegularName;
  int   (*simple)(RF_DagNode_t *);
  char   *SimpleName;
};

extern RF_RedFuncs_t rf_xorFuncs;
extern RF_RedFuncs_t rf_xorRecoveryFuncs;

void rf_InitNode(RF_DagNode_t *node, RF_NodeStatus_t initstatus,
	      int commit,
	      int (*doFunc)(RF_DagNode_t *node), 
	      int (*undoFunc)(RF_DagNode_t *node), 
	      int (*wakeFunc)(RF_DagNode_t *node, int status),
	      int nSucc, int nAnte, int nParam, int nResult, 
	      RF_DagHeader_t *hdr, char *name, RF_AllocListElem_t *alist);

void rf_FreeDAG(RF_DagHeader_t *dag_h);

RF_PropHeader_t *rf_MakePropListEntry(RF_DagHeader_t *dag_h, int resultNum,
	int paramNum, RF_PropHeader_t *next, RF_AllocListElem_t *allocList);

int rf_ConfigureDAGs(RF_ShutdownList_t **listp);

RF_DagHeader_t *rf_AllocDAGHeader(void);

void rf_FreeDAGHeader(RF_DagHeader_t *dh);

void *rf_AllocBuffer(RF_Raid_t *raidPtr, RF_DagHeader_t *dag_h,
	RF_PhysDiskAddr_t *pda, RF_AllocListElem_t *allocList);

char *rf_NodeStatusString(RF_DagNode_t *node);

void rf_PrintNodeInfoString(RF_DagNode_t *node);

int rf_AssignNodeNums(RF_DagHeader_t *dag_h);

int rf_RecurAssignNodeNums(RF_DagNode_t *node, int num, int unvisited);

void rf_ResetDAGHeaderPointers(RF_DagHeader_t *dag_h, RF_DagHeader_t *newptr);

void rf_RecurResetDAGHeaderPointers(RF_DagNode_t *node, RF_DagHeader_t *newptr);

void rf_PrintDAGList(RF_DagHeader_t *dag_h);

int rf_ValidateDAG(RF_DagHeader_t *dag_h);

void rf_redirect_asm(RF_Raid_t *raidPtr, RF_AccessStripeMap_t *asmap);

void rf_MapUnaccessedPortionOfStripe(RF_Raid_t *raidPtr,
	RF_RaidLayout_t *layoutPtr,
	RF_AccessStripeMap_t *asmap, RF_DagHeader_t *dag_h,
	RF_AccessStripeMapHeader_t **new_asm_h, int *nRodNodes, char **sosBuffer,
	char **eosBuffer, RF_AllocListElem_t *allocList);

int rf_PDAOverlap(RF_RaidLayout_t *layoutPtr, RF_PhysDiskAddr_t *src,
	RF_PhysDiskAddr_t *dest);

void rf_GenerateFailedAccessASMs(RF_Raid_t *raidPtr,
	RF_AccessStripeMap_t *asmap, RF_PhysDiskAddr_t *failedPDA,
	RF_DagHeader_t *dag_h, RF_AccessStripeMapHeader_t **new_asm_h,
	int *nXorBufs, char **rpBufPtr, char *overlappingPDAs,
	RF_AllocListElem_t *allocList);

/* flags used by RangeRestrictPDA */
#define RF_RESTRICT_NOBUFFER 0
#define RF_RESTRICT_DOBUFFER 1

void rf_RangeRestrictPDA(RF_Raid_t *raidPtr, RF_PhysDiskAddr_t *src,
	RF_PhysDiskAddr_t *dest, int dobuffer, int doraidaddr);

int rf_compute_workload_shift(RF_Raid_t *raidPtr, RF_PhysDiskAddr_t *pda);
void rf_SelectMirrorDiskIdle(RF_DagNode_t *node);
void rf_SelectMirrorDiskPartition(RF_DagNode_t *node);

#endif /* !_RF__RF_DAGUTILS_H_ */
