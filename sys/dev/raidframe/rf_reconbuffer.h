/*	$OpenBSD: rf_reconbuffer.h,v 1.1 1999/01/11 14:29:45 niklas Exp $	*/
/*	$NetBSD: rf_reconbuffer.h,v 1.1 1998/11/13 04:20:33 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland
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
 * rf_reconbuffer.h -- header file for reconstruction buffer manager
 *
 *******************************************************************/

/* :  
 * Log: rf_reconbuffer.h,v 
 * Revision 1.9  1996/07/13 00:00:59  jimz
 * sanitized generalized reconstruction architecture
 * cleaned up head sep, rbuf problems
 *
 * Revision 1.8  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.7  1996/06/05  18:06:02  jimz
 * Major code cleanup. The Great Renaming is now done.
 * Better modularity. Better typing. Fixed a bunch of
 * synchronization bugs. Made a lot of global stuff
 * per-desc or per-array. Removed dead code.
 *
 * Revision 1.6  1996/06/03  23:28:26  jimz
 * more bugfixes
 * check in tree to sync for IPDS runs with current bugfixes
 * there still may be a problem with threads in the script test
 * getting I/Os stuck- not trivially reproducible (runs ~50 times
 * in a row without getting stuck)
 *
 * Revision 1.5  1996/06/02  17:31:48  jimz
 * Moved a lot of global stuff into array structure, where it belongs.
 * Fixed up paritylogging, pss modules in this manner. Some general
 * code cleanup. Removed lots of dead code, some dead files.
 *
 * Revision 1.4  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.3  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.2  1995/12/06  15:04:47  root
 * added copyright info
 *
 */

#ifndef _RF__RF_RECONBUFFER_H_
#define _RF__RF_RECONBUFFER_H_

#include "rf_types.h"
#include "rf_reconstruct.h"

int rf_SubmitReconBuffer(RF_ReconBuffer_t *rbuf, int keep_int,
	int use_committed);
int rf_SubmitReconBufferBasic(RF_ReconBuffer_t *rbuf, int keep_int,
	int use_committed);
int rf_MultiWayReconXor(RF_Raid_t *raidPtr,
	RF_ReconParityStripeStatus_t *pssPtr);
RF_ReconBuffer_t *rf_GetFullReconBuffer(RF_ReconCtrl_t *reconCtrlPtr);
int rf_CheckForFullRbuf(RF_Raid_t *raidPtr, RF_ReconCtrl_t *reconCtrl,
	RF_ReconParityStripeStatus_t *pssPtr, int numDataCol);
void rf_ReleaseFloatingReconBuffer(RF_Raid_t *raidPtr, RF_RowCol_t row,
	RF_ReconBuffer_t *rbuf);
void rf_ReleaseBufferWaiters(RF_Raid_t *raidPtr,
	RF_ReconParityStripeStatus_t *pssPtr);
void rf_ReleaseBufferWaiter(RF_ReconCtrl_t *rcPtr, RF_ReconBuffer_t *rbuf);

#endif /* !_RF__RF_RECONBUFFER_H_ */
