/*	$OpenBSD: rf_reconutil.h,v 1.3 2002/12/16 07:01:05 tdeval Exp $	*/
/*	$NetBSD: rf_reconutil.h,v 1.3 1999/02/05 00:06:17 oster Exp $	*/

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

/**************************************************************
 * rf_reconutil.h -- Header file for reconstruction utilities.
 **************************************************************/

#ifndef	_RF__RF_RECONUTIL_H_
#define	_RF__RF_RECONUTIL_H_

#include "rf_types.h"
#include "rf_reconstruct.h"

RF_ReconCtrl_t *rf_MakeReconControl(RF_RaidReconDesc_t *,
	RF_RowCol_t, RF_RowCol_t, RF_RowCol_t, RF_RowCol_t);
void rf_FreeReconControl(RF_Raid_t *, RF_RowCol_t);
RF_HeadSepLimit_t rf_GetDefaultHeadSepLimit(RF_Raid_t *);
int  rf_GetDefaultNumFloatingReconBuffers(RF_Raid_t *);
RF_ReconBuffer_t *rf_MakeReconBuffer(RF_Raid_t *,
	RF_RowCol_t, RF_RowCol_t, RF_RbufType_t);
void rf_FreeReconBuffer(RF_ReconBuffer_t *);
void rf_CheckFloatingRbufCount(RF_Raid_t *, int);

#endif	/* !_RF__RF_RECONUTIL_H_ */
