/*	$OpenBSD: rf_interdecluster.h,v 1.3 2002/12/16 07:01:04 tdeval Exp $	*/
/*	$NetBSD: rf_interdecluster.h,v 1.3 1999/02/05 00:06:12 oster Exp $	*/

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Khalil Amiri
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

/* rf_interdecluster.h
 * header file for Interleaved Declustering
 */

#ifndef	_RF__RF_INTERDECLUSTER_H_
#define	_RF__RF_INTERDECLUSTER_H_

int  rf_ConfigureInterDecluster(RF_ShutdownList_t **, RF_Raid_t *,
	RF_Config_t *);
int  rf_GetDefaultNumFloatingReconBuffersInterDecluster(RF_Raid_t *);
RF_HeadSepLimit_t rf_GetDefaultHeadSepLimitInterDecluster(RF_Raid_t *);
RF_ReconUnitCount_t rf_GetNumSpareRUsInterDecluster(RF_Raid_t *);
void rf_MapSectorInterDecluster(RF_Raid_t *, RF_RaidAddr_t, RF_RowCol_t *,
	RF_RowCol_t *, RF_SectorNum_t *, int);
void rf_MapParityInterDecluster(RF_Raid_t *, RF_RaidAddr_t, RF_RowCol_t *,
	RF_RowCol_t *, RF_SectorNum_t *, int);
void rf_IdentifyStripeInterDecluster(RF_Raid_t *, RF_RaidAddr_t,
	RF_RowCol_t **, RF_RowCol_t *);
void rf_MapSIDToPSIDInterDecluster(RF_RaidLayout_t *, RF_StripeNum_t,
	RF_StripeNum_t *, RF_ReconUnitNum_t *);
void rf_RAIDIDagSelect(RF_Raid_t *, RF_IoType_t, RF_AccessStripeMap_t *,
	RF_VoidFuncPtr *);

#endif	/* !_RF__RF_INTERDECLUSTER_H_ */
