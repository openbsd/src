/*	$OpenBSD: rf_reconbuffer.h,v 1.3 2002/12/16 07:01:05 tdeval Exp $	*/
/*	$NetBSD: rf_reconbuffer.h,v 1.3 1999/02/05 00:06:16 oster Exp $	*/

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

/*********************************************************************
 *
 * rf_reconbuffer.h -- Header file for reconstruction buffer manager.
 *
 *********************************************************************/

#ifndef	_RF__RF_RECONBUFFER_H_
#define	_RF__RF_RECONBUFFER_H_

#include "rf_types.h"
#include "rf_reconstruct.h"

int  rf_SubmitReconBuffer(RF_ReconBuffer_t *, int, int);
int  rf_SubmitReconBufferBasic(RF_ReconBuffer_t *, int, int);
int  rf_MultiWayReconXor(RF_Raid_t *, RF_ReconParityStripeStatus_t *);
RF_ReconBuffer_t *rf_GetFullReconBuffer(RF_ReconCtrl_t *);
int  rf_CheckForFullRbuf(RF_Raid_t *, RF_ReconCtrl_t *,
	RF_ReconParityStripeStatus_t *, int);
void rf_ReleaseFloatingReconBuffer(RF_Raid_t *, RF_RowCol_t,
	RF_ReconBuffer_t *);
void rf_ReleaseBufferWaiters(RF_Raid_t *, RF_ReconParityStripeStatus_t *);
void rf_ReleaseBufferWaiter(RF_ReconCtrl_t *, RF_ReconBuffer_t *);

#endif	/* !_RF__RF_RECONBUFFER_H_ */
