/*	$OpenBSD: rf_nwayxor.h,v 1.1 1999/01/11 14:29:31 niklas Exp $	*/
/*	$NetBSD: rf_nwayxor.h,v 1.1 1998/11/13 04:20:31 oster Exp $	*/
/*
 * rf_nwayxor.h
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
 * rf_nwayxor.h -- types and prototypes for nwayxor module
 */
/*
 * :  
 * Log: rf_nwayxor.h,v 
 * Revision 1.4  1996/06/10 11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.3  1996/06/02  17:31:48  jimz
 * Moved a lot of global stuff into array structure, where it belongs.
 * Fixed up paritylogging, pss modules in this manner. Some general
 * code cleanup. Removed lots of dead code, some dead files.
 *
 * Revision 1.2  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.1  1996/05/18  19:56:47  jimz
 * Initial revision
 *
 */

#ifndef _RF__RF_NWAYXOR_H_
#define _RF__RF_NWAYXOR_H_

#include "rf_types.h"
#include "rf_raid.h"
#include "rf_reconstruct.h"

int rf_ConfigureNWayXor(RF_ShutdownList_t **listp);
void rf_nWayXor1(RF_ReconBuffer_t **src_rbs, RF_ReconBuffer_t *dest_rb, int len);
void rf_nWayXor2(RF_ReconBuffer_t **src_rbs, RF_ReconBuffer_t *dest_rb, int len);
void rf_nWayXor3(RF_ReconBuffer_t **src_rbs, RF_ReconBuffer_t *dest_rb, int len);
void rf_nWayXor4(RF_ReconBuffer_t **src_rbs, RF_ReconBuffer_t *dest_rb, int len);
void rf_nWayXor5(RF_ReconBuffer_t **src_rbs, RF_ReconBuffer_t *dest_rb, int len);
void rf_nWayXor6(RF_ReconBuffer_t **src_rbs, RF_ReconBuffer_t *dest_rb, int len);
void rf_nWayXor7(RF_ReconBuffer_t **src_rbs, RF_ReconBuffer_t *dest_rb, int len);
void rf_nWayXor8(RF_ReconBuffer_t **src_rbs, RF_ReconBuffer_t *dest_rb, int len);
void rf_nWayXor9(RF_ReconBuffer_t **src_rbs, RF_ReconBuffer_t *dest_rb, int len);

#endif /* !_RF__RF_NWAYXOR_H_ */
