/*	$OpenBSD: rf_paritylogDiskMgr.h,v 1.1 1999/01/11 14:29:35 niklas Exp $	*/
/*	$NetBSD: rf_paritylogDiskMgr.h,v 1.1 1998/11/13 04:20:32 oster Exp $	*/
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

/* header file for parity log disk mgr code
 *
 * :  
 * Log: rf_paritylogDiskMgr.h,v 
 * Revision 1.5  1996/06/02 17:31:48  jimz
 * Moved a lot of global stuff into array structure, where it belongs.
 * Fixed up paritylogging, pss modules in this manner. Some general
 * code cleanup. Removed lots of dead code, some dead files.
 *
 * Revision 1.4  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.3  1995/12/06  20:56:39  wvcii
 * added prototypes
 *
 * Revision 1.2  1995/11/30  16:06:21  wvcii
 * added copyright info
 *
 * Revision 1.1  1995/09/06  19:25:29  wvcii
 * Initial revision
 *
 *
 */

#ifndef _RF__RF_PARITYLOGDISKMGR_H_
#define _RF__RF_PARITYLOGDISKMGR_H_

#include "rf_types.h"

int rf_ShutdownLogging(RF_Raid_t *raidPtr);
int rf_ParityLoggingDiskManager(RF_Raid_t *raidPtr);

#endif /* !_RF__RF_PARITYLOGDISKMGR_H_ */
