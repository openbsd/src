/*	$OpenBSD: rf_map.h,v 1.3 2002/12/16 07:01:04 tdeval Exp $	*/
/*	$NetBSD: rf_map.h,v 1.3 1999/02/05 00:06:12 oster Exp $	*/

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

/* rf_map.h */

#ifndef	_RF__RF_MAP_H_
#define	_RF__RF_MAP_H_

#include "rf_types.h"
#include "rf_alloclist.h"
#include "rf_raid.h"

/* Mapping structure allocation and free routines. */
RF_AccessStripeMapHeader_t *rf_MapAccess(RF_Raid_t *, RF_RaidAddr_t,
	RF_SectorCount_t, caddr_t, int);

void rf_MarkFailuresInASMList(RF_Raid_t *, RF_AccessStripeMapHeader_t *);

RF_AccessStripeMap_t *rf_DuplicateASM(RF_AccessStripeMap_t *);

RF_PhysDiskAddr_t *rf_DuplicatePDA(RF_PhysDiskAddr_t *);

int  rf_ConfigureMapModule(RF_ShutdownList_t **);

RF_AccessStripeMapHeader_t *rf_AllocAccessStripeMapHeader(void);

void rf_FreeAccessStripeMapHeader(RF_AccessStripeMapHeader_t *);

RF_PhysDiskAddr_t *rf_AllocPhysDiskAddr(void);

RF_PhysDiskAddr_t *rf_AllocPDAList(int);

void rf_FreePhysDiskAddr(RF_PhysDiskAddr_t *);

RF_AccessStripeMap_t *rf_AllocAccessStripeMapComponent(void);

RF_AccessStripeMap_t *rf_AllocASMList(int);

void rf_FreeAccessStripeMapComponent(RF_AccessStripeMap_t *);

void rf_FreeAccessStripeMap(RF_AccessStripeMapHeader_t *);

int  rf_CheckStripeForFailures(RF_Raid_t *, RF_AccessStripeMap_t *);

int  rf_NumFailedDataUnitsInStripe(RF_Raid_t *, RF_AccessStripeMap_t *);

void rf_PrintAccessStripeMap(RF_AccessStripeMapHeader_t *);

void rf_PrintFullAccessStripeMap(RF_AccessStripeMapHeader_t *, int);

void rf_PrintRaidAddressInfo(RF_Raid_t *, RF_RaidAddr_t, RF_SectorCount_t);

void rf_ASMParityAdjust(RF_PhysDiskAddr_t *, RF_StripeNum_t, RF_SectorNum_t,
	RF_RaidLayout_t *, RF_AccessStripeMap_t *);

void rf_ASMCheckStatus(RF_Raid_t *, RF_PhysDiskAddr_t *,
	RF_AccessStripeMap_t *, RF_RaidDisk_t **, int);

#endif	/* !_RF__RF_MAP_H_ */
