/*	$OpenBSD: rf_parityloggingdags.h,v 1.1 1999/01/11 14:29:37 niklas Exp $	*/
/*	$NetBSD: rf_parityloggingdags.h,v 1.1 1998/11/13 04:20:32 oster Exp $	*/
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

/****************************************************************************
 *                                                                          *
 * rf_parityloggingdags.h -- header file for parity logging dags            *
 *                                                                          *
 ****************************************************************************/

/* :  
 * Log: rf_parityloggingdags.h,v 
 * Revision 1.10  1996/07/27 23:36:08  jimz
 * Solaris port of simulator
 *
 * Revision 1.9  1996/06/02  17:31:48  jimz
 * Moved a lot of global stuff into array structure, where it belongs.
 * Fixed up paritylogging, pss modules in this manner. Some general
 * code cleanup. Removed lots of dead code, some dead files.
 *
 * Revision 1.8  1996/05/31  22:26:54  jimz
 * fix a lot of mapping problems, memory allocation problems
 * found some weird lock issues, fixed 'em
 * more code cleanup
 *
 * Revision 1.7  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.6  1996/05/24  22:17:04  jimz
 * continue code + namespace cleanup
 * typed a bunch of flags
 *
 * Revision 1.5  1996/05/24  04:28:55  jimz
 * release cleanup ckpt
 *
 * Revision 1.4  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.3  1995/12/06  20:55:08  wvcii
 * added prototyping
 *
 */

#ifndef _RF__RF_PARITYLOGGINGDAGS_H_
#define _RF__RF_PARITYLOGGINGDAGS_H_

/* routines that create DAGs */
void rf_CommonCreateParityLoggingLargeWriteDAG(RF_Raid_t *raidPtr,
	RF_AccessStripeMap_t *asmap, RF_DagHeader_t *dag_h,
	void *bp, RF_RaidAccessFlags_t flags, RF_AllocListElem_t *allocList,
	int nfaults, int (*redFunc)(RF_DagNode_t *));
void rf_CommonCreateParityLoggingSmallWriteDAG(RF_Raid_t *raidPtr,
	RF_AccessStripeMap_t *asmap, RF_DagHeader_t *dag_h,
	void *bp, RF_RaidAccessFlags_t flags, RF_AllocListElem_t *allocList,
	RF_RedFuncs_t *pfuncs, RF_RedFuncs_t *qfuncs);

void rf_CreateParityLoggingLargeWriteDAG(RF_Raid_t *raidPtr,
	RF_AccessStripeMap_t *asmap, RF_DagHeader_t *dag_h,
	void *bp, RF_RaidAccessFlags_t flags, RF_AllocListElem_t *allocList,
	int nfaults, int (*redFunc)(RF_DagNode_t *));
void rf_CreateParityLoggingSmallWriteDAG(RF_Raid_t *raidPtr,
	RF_AccessStripeMap_t *asmap, RF_DagHeader_t *dag_h,
	void *bp, RF_RaidAccessFlags_t flags, RF_AllocListElem_t *allocList,
	RF_RedFuncs_t *pfuncs, RF_RedFuncs_t *qfuncs);

#endif /* !_RF__RF_PARITYLOGGINGDAGS_H_ */
