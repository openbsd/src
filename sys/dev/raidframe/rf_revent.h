/*	$OpenBSD: rf_revent.h,v 1.1 1999/01/11 14:29:48 niklas Exp $	*/
/*	$NetBSD: rf_revent.h,v 1.1 1998/11/13 04:20:34 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author:
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

/*******************************************************************
 *
 * rf_revent.h -- header file for reconstruction event handling code
 *
 *******************************************************************/

/* :  
 * Log: rf_revent.h,v 
 * Revision 1.7  1996/07/15 05:40:41  jimz
 * some recon datastructure cleanup
 * better handling of multiple failures
 * added undocumented double-recon test
 *
 * Revision 1.6  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.5  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.4  1996/06/05  18:06:02  jimz
 * Major code cleanup. The Great Renaming is now done.
 * Better modularity. Better typing. Fixed a bunch of
 * synchronization bugs. Made a lot of global stuff
 * per-desc or per-array. Removed dead code.
 *
 * Revision 1.3  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.2  1995/12/06  15:04:20  root
 * added copyright info
 *
 */

#ifndef _RF__RF_REVENT_H_
#define _RF__RF_REVENT_H_

#include "rf_types.h"

int rf_ConfigureReconEvent(RF_ShutdownList_t **listp);

RF_ReconEvent_t *rf_GetNextReconEvent(RF_RaidReconDesc_t *reconDesc,
	RF_RowCol_t row, void (*continueFunc)(void *), void *continueArg);

void rf_CauseReconEvent(RF_Raid_t *raidPtr, RF_RowCol_t row, RF_RowCol_t col,
	void *arg, RF_Revent_t type);

void rf_FreeReconEventDesc(RF_ReconEvent_t *event);

#endif /* !_RF__RF_REVENT_H_ */
