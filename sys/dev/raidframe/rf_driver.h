/*	$OpenBSD: rf_driver.h,v 1.4 2002/12/16 07:01:03 tdeval Exp $	*/
/*	$NetBSD: rf_driver.h,v 1.4 2000/02/13 04:53:57 oster Exp $	*/

/*
 * rf_driver.h
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

#ifndef	_RF__RF_DRIVER_H_
#define	_RF__RF_DRIVER_H_

#include "rf_threadstuff.h"
#include "rf_types.h"

#if	defined(__NetBSD__)
#include "rf_netbsd.h"
#elif	defined(__OpenBSD__)
#include "rf_openbsd.h"
#endif

#if	_KERNEL
RF_DECLARE_EXTERN_MUTEX(rf_printf_mutex);
int  rf_BootRaidframe(void);
int  rf_UnbootRaidframe(void);
int  rf_Shutdown(RF_Raid_t *);
int  rf_Configure(RF_Raid_t *, RF_Config_t *, RF_AutoConfig_t *);
RF_RaidAccessDesc_t *rf_AllocRaidAccDesc(RF_Raid_t *, RF_IoType_t,
	RF_RaidAddr_t, RF_SectorCount_t, caddr_t, void *, RF_DagHeader_t **,
	RF_AccessStripeMapHeader_t **, RF_RaidAccessFlags_t,
	void (*) (struct buf *), void *, RF_AccessState_t *);
void rf_FreeRaidAccDesc(RF_RaidAccessDesc_t *);
int  rf_DoAccess(RF_Raid_t *, RF_IoType_t, int, RF_RaidAddr_t,
	RF_SectorCount_t, caddr_t, void *, RF_DagHeader_t **,
	RF_AccessStripeMapHeader_t **, RF_RaidAccessFlags_t,
	RF_RaidAccessDesc_t **, void (*) (struct buf *), void *);
int  rf_SetReconfiguredMode(RF_Raid_t *, RF_RowCol_t, RF_RowCol_t);
int  rf_FailDisk(RF_Raid_t *, RF_RowCol_t, RF_RowCol_t, int);
void rf_SignalQuiescenceLock(RF_Raid_t *, RF_RaidReconDesc_t *);
int  rf_SuspendNewRequestsAndWait(RF_Raid_t *);
void rf_ResumeNewRequests(RF_Raid_t *);
void rf_StartThroughputStats(RF_Raid_t *);
void rf_StartUserStats(RF_Raid_t *);
void rf_StopUserStats(RF_Raid_t *);
void rf_UpdateUserStats(RF_Raid_t *, int, int);
void rf_PrintUserStats(RF_Raid_t *);
#endif	/* _KERNEL */

#endif	/* !_RF__RF_DRIVER_H_ */
