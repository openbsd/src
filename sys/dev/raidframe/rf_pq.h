/*	$OpenBSD: rf_pq.h,v 1.1 1999/01/11 14:29:39 niklas Exp $	*/
/*	$NetBSD: rf_pq.h,v 1.1 1998/11/13 04:20:32 oster Exp $	*/
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
/*
 * :  
 * Log: rf_pq.h,v 
 * Revision 1.9  1996/07/31 15:35:05  jimz
 * evenodd changes; bugfixes for double-degraded archs, generalize
 * some formerly PQ-only functions
 *
 * Revision 1.8  1996/07/27  23:36:08  jimz
 * Solaris port of simulator
 *
 * Revision 1.7  1996/06/03  23:28:26  jimz
 * more bugfixes
 * check in tree to sync for IPDS runs with current bugfixes
 * there still may be a problem with threads in the script test
 * getting I/Os stuck- not trivially reproducible (runs ~50 times
 * in a row without getting stuck)
 *
 * Revision 1.6  1996/06/02  17:31:48  jimz
 * Moved a lot of global stuff into array structure, where it belongs.
 * Fixed up paritylogging, pss modules in this manner. Some general
 * code cleanup. Removed lots of dead code, some dead files.
 *
 * Revision 1.5  1996/05/31  22:26:54  jimz
 * fix a lot of mapping problems, memory allocation problems
 * found some weird lock issues, fixed 'em
 * more code cleanup
 *
 * Revision 1.4  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.3  1996/05/24  22:17:04  jimz
 * continue code + namespace cleanup
 * typed a bunch of flags
 *
 * Revision 1.2  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.1  1996/05/18  19:56:21  jimz
 * Initial revision
 *
 */

#ifndef _RF__RF_PQ_H_
#define _RF__RF_PQ_H_

#include "rf_archs.h"

extern RF_RedFuncs_t rf_pFuncs;
extern RF_RedFuncs_t rf_pRecoveryFuncs;

int rf_RegularONPFunc(RF_DagNode_t *node);
int rf_SimpleONPFunc(RF_DagNode_t *node);
int rf_RecoveryPFunc(RF_DagNode_t *node);
int rf_RegularPFunc(RF_DagNode_t *node);

#if (RF_INCLUDE_DECL_PQ > 0) || (RF_INCLUDE_RAID6 > 0)

extern RF_RedFuncs_t rf_qFuncs;
extern RF_RedFuncs_t rf_qRecoveryFuncs;
extern RF_RedFuncs_t rf_pqRecoveryFuncs;

void rf_PQDagSelect(RF_Raid_t *raidPtr, RF_IoType_t type,
	RF_AccessStripeMap_t *asmap, RF_VoidFuncPtr *createFunc);
RF_CREATE_DAG_FUNC_DECL(rf_PQCreateLargeWriteDAG);
int rf_RegularONQFunc(RF_DagNode_t *node);
int rf_SimpleONQFunc(RF_DagNode_t *node);
RF_CREATE_DAG_FUNC_DECL(rf_PQCreateSmallWriteDAG);
int rf_RegularPQFunc(RF_DagNode_t *node);
int rf_RegularQFunc(RF_DagNode_t *node);
void rf_Degraded_100_PQFunc(RF_DagNode_t *node);
int rf_RecoveryQFunc(RF_DagNode_t *node);
int rf_RecoveryPQFunc(RF_DagNode_t *node);
void rf_PQ_DegradedWriteQFunc(RF_DagNode_t *node);
void rf_IncQ(unsigned long *dest, unsigned long *buf, unsigned length,
	unsigned coeff);
void rf_PQ_recover(unsigned long *pbuf, unsigned long *qbuf, unsigned long *abuf,
	unsigned long *bbuf, unsigned length, unsigned coeff_a, unsigned coeff_b);

#endif /* (RF_INCLUDE_DECL_PQ > 0) || (RF_INCLUDE_RAID6 > 0) */

#endif /* !_RF__RF_PQ_H_ */
