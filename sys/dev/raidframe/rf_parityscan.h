/*	$OpenBSD: rf_parityscan.h,v 1.1 1999/01/11 14:29:38 niklas Exp $	*/
/*	$NetBSD: rf_parityscan.h,v 1.1 1998/11/13 04:20:32 oster Exp $	*/
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

/* :  
 * Log: rf_parityscan.h,v 
 * Revision 1.14  1996/07/05 18:01:12  jimz
 * don't make parity protos ndef KERNEL
 *
 * Revision 1.13  1996/06/20  17:41:43  jimz
 * change decl for VerifyParity
 *
 * Revision 1.12  1996/06/20  15:38:39  jimz
 * renumber parityscan return codes
 *
 * Revision 1.11  1996/06/19  22:23:01  jimz
 * parity verification is now a layout-configurable thing
 * not all layouts currently support it (correctly, anyway)
 *
 * Revision 1.10  1996/06/09  02:36:46  jimz
 * lots of little crufty cleanup- fixup whitespace
 * issues, comment #ifdefs, improve typing in some
 * places (esp size-related)
 *
 * Revision 1.9  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.8  1996/06/02  17:31:48  jimz
 * Moved a lot of global stuff into array structure, where it belongs.
 * Fixed up paritylogging, pss modules in this manner. Some general
 * code cleanup. Removed lots of dead code, some dead files.
 *
 * Revision 1.7  1996/05/31  22:26:54  jimz
 * fix a lot of mapping problems, memory allocation problems
 * found some weird lock issues, fixed 'em
 * more code cleanup
 *
 * Revision 1.6  1996/05/24  22:17:04  jimz
 * continue code + namespace cleanup
 * typed a bunch of flags
 *
 * Revision 1.5  1996/05/24  04:28:55  jimz
 * release cleanup ckpt
 *
 * Revision 1.4  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.3  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.2  1995/11/30  16:20:46  wvcii
 * added copyright info
 *
 */

#ifndef _RF__RF_PARITYSCAN_H_
#define _RF__RF_PARITYSCAN_H_

#include "rf_types.h"
#include "rf_alloclist.h"

int rf_RewriteParity(RF_Raid_t *raidPtr);
int rf_VerifyParityBasic(RF_Raid_t *raidPtr, RF_RaidAddr_t raidAddr,
	RF_PhysDiskAddr_t *parityPDA, int correct_it, RF_RaidAccessFlags_t flags);
int rf_VerifyParity(RF_Raid_t *raidPtr, RF_AccessStripeMap_t *stripeMap,
	int correct_it, RF_RaidAccessFlags_t flags);
int rf_TryToRedirectPDA(RF_Raid_t *raidPtr, RF_PhysDiskAddr_t *pda, int parity);
int rf_VerifyDegrModeWrite(RF_Raid_t *raidPtr, RF_AccessStripeMapHeader_t *asmh);
RF_DagHeader_t *rf_MakeSimpleDAG(RF_Raid_t *raidPtr, int nNodes, 
				 int bytesPerSU, char *databuf, 
				 int (*doFunc)(RF_DagNode_t *), 
				 int (*undoFunc)(RF_DagNode_t *), 
				 char *name, RF_AllocListElem_t *alloclist,
				 RF_RaidAccessFlags_t flags, int priority);

#define RF_DO_CORRECT_PARITY   1
#define RF_DONT_CORRECT_PARITY 0

/*
 * Return vals for VerifyParity operation
 *
 * Ordering is important here.
 */
#define RF_PARITY_OKAY               0 /* or no parity information */
#define RF_PARITY_CORRECTED          1
#define RF_PARITY_BAD                2
#define RF_PARITY_COULD_NOT_CORRECT  3
#define RF_PARITY_COULD_NOT_VERIFY   4

#endif /* !_RF__RF_PARITYSCAN_H_ */
