/*	$OpenBSD: rf_pqdegdags.h,v 1.1 1999/01/11 14:29:40 niklas Exp $	*/
/*	$NetBSD: rf_pqdegdags.h,v 1.1 1998/11/13 04:20:32 oster Exp $	*/
/*
 * rf_pqdegdags.h
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
 * rf_pqdegdags.c
 * Degraded mode dags for double fault cases. 
 */
/*
 * :  
 * Log: rf_pqdegdags.h,v 
 * Revision 1.6  1996/07/31 15:35:20  jimz
 * evenodd changes; bugfixes for double-degraded archs, generalize
 * some formerly PQ-only functions
 *
 * Revision 1.5  1996/06/02  17:31:48  jimz
 * Moved a lot of global stuff into array structure, where it belongs.
 * Fixed up paritylogging, pss modules in this manner. Some general
 * code cleanup. Removed lots of dead code, some dead files.
 *
 * Revision 1.4  1996/05/31  22:26:54  jimz
 * fix a lot of mapping problems, memory allocation problems
 * found some weird lock issues, fixed 'em
 * more code cleanup
 *
 * Revision 1.3  1996/05/24  04:28:55  jimz
 * release cleanup ckpt
 *
 * Revision 1.2  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.1  1996/05/18  19:56:30  jimz
 * Initial revision
 *
 */

#ifndef _RF__RF_PQDEGDAGS_H_
#define _RF__RF_PQDEGDAGS_H_

#include "rf_dag.h"

RF_CREATE_DAG_FUNC_DECL(rf_PQ_DoubleDegRead);
int rf_PQDoubleRecoveryFunc(RF_DagNode_t *node);
int rf_PQWriteDoubleRecoveryFunc(RF_DagNode_t *node);
RF_CREATE_DAG_FUNC_DECL(rf_PQ_DDLargeWrite);
RF_CREATE_DAG_FUNC_DECL(rf_PQ_DDSimpleSmallWrite);
RF_CREATE_DAG_FUNC_DECL(rf_PQ_200_CreateWriteDAG);

#endif /* !_RF__RF_PQDEGDAGS_H_ */
