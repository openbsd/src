/*	$OpenBSD: rf_copyback.h,v 1.1 1999/01/11 14:29:03 niklas Exp $	*/
/*	$NetBSD: rf_copyback.h,v 1.1 1998/11/13 04:20:27 oster Exp $	*/
/*
 * rf_copyback.h
 */
/*
 * Copyright (c) 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Jim Zelenka
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
 * Log: rf_copyback.h,v 
 * Revision 1.5  1996/07/11 19:08:00  jimz
 * generalize reconstruction mechanism
 * allow raid1 reconstructs via copyback (done with array
 * quiesced, not online, therefore not disk-directed)
 *
 * Revision 1.4  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.3  1996/05/24  01:59:45  jimz
 * another checkpoint in code cleanup for release
 * time to sync kernel tree
 *
 * Revision 1.2  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.1  1996/05/18  19:55:02  jimz
 * Initial revision
 *
 */

#ifndef _RF__RF_COPYBACK_H_
#define _RF__RF_COPYBACK_H_

#include "rf_types.h"

typedef struct RF_CopybackDesc_s {
  RF_Raid_t           *raidPtr;
  RF_RowCol_t          frow;
  RF_RowCol_t          fcol;
  RF_RowCol_t          spRow;
  RF_RowCol_t          spCol;
  int                  status;
  RF_StripeNum_t       stripeAddr;
  RF_SectorCount_t     sectPerSU;
  RF_SectorCount_t     sectPerStripe;
  char                *databuf;
  RF_DiskQueueData_t  *readreq;
  RF_DiskQueueData_t  *writereq;
  struct timeval       starttime;
#ifndef SIMULATE
  RF_MCPair_t         *mcpair;
#endif /* !SIMULATE */
} RF_CopybackDesc_t;

extern int rf_copyback_in_progress;

int  rf_ConfigureCopyback(RF_ShutdownList_t **listp);
void rf_CopybackReconstructedData(RF_Raid_t *raidPtr);
void rf_ContinueCopyback(RF_CopybackDesc_t *desc);

#endif /* !_RF__RF_COPYBACK_H_ */
