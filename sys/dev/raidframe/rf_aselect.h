/*	$OpenBSD: rf_aselect.h,v 1.1 1999/01/11 14:29:00 niklas Exp $	*/
/*	$NetBSD: rf_aselect.h,v 1.1 1998/11/13 04:20:26 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland, William V. Courtright II
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

/*****************************************************************************
 *
 * aselect.h -- header file for algorithm selection code
 *
 *****************************************************************************/
/* :  
 * Log: rf_aselect.h,v 
 * Revision 1.5  1996/05/24 22:17:04  jimz
 * continue code + namespace cleanup
 * typed a bunch of flags
 *
 * Revision 1.4  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.3  1995/11/30  16:28:00  wvcii
 * added copyright info
 *
 * Revision 1.2  1995/11/19  16:20:46  wvcii
 * changed SelectAlgorithm prototype
 *
 */

#ifndef _RF__RF_ASELECT_H_
#define _RF__RF_ASELECT_H_
 
#include "rf_desc.h"

int rf_SelectAlgorithm(RF_RaidAccessDesc_t *desc, RF_RaidAccessFlags_t flags);

#endif /* !_RF__RF_ASELECT_H_ */
