/*	$OpenBSD: rf_nwayxor.h,v 1.3 2002/12/16 07:01:04 tdeval Exp $	*/
/*	$NetBSD: rf_nwayxor.h,v 1.3 1999/02/05 00:06:13 oster Exp $	*/

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
 * rf_nwayxor.h -- Types and prototypes for nwayxor module.
 */

#ifndef	_RF__RF_NWAYXOR_H_
#define	_RF__RF_NWAYXOR_H_

#include "rf_types.h"
#include "rf_raid.h"
#include "rf_reconstruct.h"

int  rf_ConfigureNWayXor(RF_ShutdownList_t **);
void rf_nWayXor1(RF_ReconBuffer_t **, RF_ReconBuffer_t *, int);
void rf_nWayXor2(RF_ReconBuffer_t **, RF_ReconBuffer_t *, int);
void rf_nWayXor3(RF_ReconBuffer_t **, RF_ReconBuffer_t *, int);
void rf_nWayXor4(RF_ReconBuffer_t **, RF_ReconBuffer_t *, int);
void rf_nWayXor5(RF_ReconBuffer_t **, RF_ReconBuffer_t *, int);
void rf_nWayXor6(RF_ReconBuffer_t **, RF_ReconBuffer_t *, int);
void rf_nWayXor7(RF_ReconBuffer_t **, RF_ReconBuffer_t *, int);
void rf_nWayXor8(RF_ReconBuffer_t **, RF_ReconBuffer_t *, int);
void rf_nWayXor9(RF_ReconBuffer_t **, RF_ReconBuffer_t *, int);

#endif	/* !_RF__RF_NWAYXOR_H_ */
