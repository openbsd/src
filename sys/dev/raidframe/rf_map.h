/*	$OpenBSD: rf_map.h,v 1.1 1999/01/11 14:29:29 niklas Exp $	*/
/*	$NetBSD: rf_map.h,v 1.1 1998/11/13 04:20:31 oster Exp $	*/
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

/* :  
 * Log: rf_map.h,v 
 * Revision 1.9  1996/07/22 19:52:16  jimz
 * switched node params to RF_DagParam_t, a union of
 * a 64-bit int and a void *, for better portability
 * attempted hpux port, but failed partway through for
 * lack of a single C compiler capable of compiling all
 * source files
 *
 * Revision 1.8  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.7  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.6  1996/05/31  22:26:54  jimz
 * fix a lot of mapping problems, memory allocation problems
 * found some weird lock issues, fixed 'em
 * more code cleanup
 *
 * Revision 1.5  1996/05/30  11:29:41  jimz
 * Numerous bug fixes. Stripe lock release code disagreed with the taking code
 * about when stripes should be locked (I made it consistent: no parity, no lock)
 * There was a lot of extra serialization of I/Os which I've removed- a lot of
 * it was to calculate values for the cache code, which is no longer with us.
 * More types, function, macro cleanup. Added code to properly quiesce the array
 * on shutdown. Made a lot of stuff array-specific which was (bogusly) general
 * before. Fixed memory allocation, freeing bugs.
 *
 * Revision 1.4  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.3  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.2  1995/12/01  19:25:14  root
 * added copyright info
 *
 */

#ifndef _RF__RF_MAP_H_
#define _RF__RF_MAP_H_

#include "rf_types.h"
#include "rf_alloclist.h"
#include "rf_raid.h"

/* mapping structure allocation and free routines */
RF_AccessStripeMapHeader_t *rf_MapAccess(RF_Raid_t *raidPtr,
	RF_RaidAddr_t raidAddress, RF_SectorCount_t numBlocks,
	caddr_t buffer, int remap);

void rf_MarkFailuresInASMList(RF_Raid_t *raidPtr,
	RF_AccessStripeMapHeader_t *asm_h);

RF_AccessStripeMap_t *rf_DuplicateASM(RF_AccessStripeMap_t *asmap);

RF_PhysDiskAddr_t *rf_DuplicatePDA(RF_PhysDiskAddr_t *pda);

int rf_ConfigureMapModule(RF_ShutdownList_t **listp);

RF_AccessStripeMapHeader_t *rf_AllocAccessStripeMapHeader(void);

void rf_FreeAccessStripeMapHeader(RF_AccessStripeMapHeader_t *p);

RF_PhysDiskAddr_t *rf_AllocPhysDiskAddr(void);

RF_PhysDiskAddr_t *rf_AllocPDAList(int count);

void rf_FreePhysDiskAddr(RF_PhysDiskAddr_t *p);

RF_AccessStripeMap_t *rf_AllocAccessStripeMapComponent(void);

RF_AccessStripeMap_t *rf_AllocASMList(int count);

void rf_FreeAccessStripeMapComponent(RF_AccessStripeMap_t *p);

void rf_FreeAccessStripeMap(RF_AccessStripeMapHeader_t *hdr);

int rf_CheckStripeForFailures(RF_Raid_t *raidPtr, RF_AccessStripeMap_t *asmap);

int rf_NumFailedDataUnitsInStripe(RF_Raid_t *raidPtr, RF_AccessStripeMap_t *asmap);

void rf_PrintAccessStripeMap(RF_AccessStripeMapHeader_t *asm_h);

void rf_PrintFullAccessStripeMap(RF_AccessStripeMapHeader_t *asm_h, int prbuf);

void rf_PrintRaidAddressInfo(RF_Raid_t *raidPtr, RF_RaidAddr_t raidAddr,
	RF_SectorCount_t numBlocks);

void rf_ASMParityAdjust(RF_PhysDiskAddr_t *toAdjust,
	RF_StripeNum_t startAddrWithinStripe, RF_SectorNum_t endAddress,
	RF_RaidLayout_t *layoutPtr, RF_AccessStripeMap_t *asm_p);

void rf_ASMCheckStatus(RF_Raid_t *raidPtr, RF_PhysDiskAddr_t *pda_p,
	RF_AccessStripeMap_t *asm_p, RF_RaidDisk_t **disks, int parity);

#endif /* !_RF__RF_MAP_H_ */
