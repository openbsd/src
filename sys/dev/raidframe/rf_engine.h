/*	$OpenBSD: rf_engine.h,v 1.1 1999/01/11 14:29:19 niklas Exp $	*/
/*	$NetBSD: rf_engine.h,v 1.1 1998/11/13 04:20:29 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: William V. Courtright II, Mark Holland, Jim Zelenka
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

/**********************************************************
 *                                                        *
 * engine.h -- header file for execution engine functions *
 *                                                        *
 **********************************************************/

/* :  
 * Log: rf_engine.h,v 
 * Revision 1.11  1996/06/14 14:16:22  jimz
 * new decl of ConfigureEngine
 *
 * Revision 1.10  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.9  1996/05/30  12:59:18  jimz
 * make etimer happier, more portable
 *
 * Revision 1.8  1996/05/24  04:28:55  jimz
 * release cleanup ckpt
 *
 * Revision 1.7  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.6  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.5  1995/12/01  18:12:17  root
 * added copyright info
 *
 */

#ifndef _RF__RF_ENGINE_H_
#define _RF__RF_ENGINE_H_

int rf_ConfigureEngine(RF_ShutdownList_t **listp,
	RF_Raid_t *raidPtr, RF_Config_t *cfgPtr);

int rf_FinishNode(RF_DagNode_t *node, int context); /* return finished node to engine */

int rf_DispatchDAG(RF_DagHeader_t *dag, void (*cbFunc)(void *), void *cbArg); /* execute dag */

#endif /* !_RF__RF_ENGINE_H_ */
