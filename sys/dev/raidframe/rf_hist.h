/*	$OpenBSD: rf_hist.h,v 1.1 1999/01/11 14:29:25 niklas Exp $	*/
/*	$NetBSD: rf_hist.h,v 1.1 1998/11/13 04:20:30 oster Exp $	*/
/*
 * rf_hist.h
 *
 * Histgram operations for RAIDframe stats
 */
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
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
/* :  
 * Log: rf_hist.h,v 
 * Revision 1.3  1996/06/09 02:36:46  jimz
 * lots of little crufty cleanup- fixup whitespace
 * issues, comment #ifdefs, improve typing in some
 * places (esp size-related)
 *
 * Revision 1.2  1996/05/31  22:26:54  jimz
 * fix a lot of mapping problems, memory allocation problems
 * found some weird lock issues, fixed 'em
 * more code cleanup
 *
 * Revision 1.1  1996/05/31  10:33:05  jimz
 * Initial revision
 *
 */

#ifndef _RF__RF_HIST_H_
#define _RF__RF_HIST_H_

#include "rf_types.h"

#define RF_HIST_RESOLUTION   5
#define RF_HIST_MIN_VAL      0
#define RF_HIST_MAX_VAL      1000
#define RF_HIST_RANGE        (RF_HIST_MAX_VAL - RF_HIST_MIN_VAL)
#define RF_HIST_NUM_BUCKETS  (RF_HIST_RANGE / RF_HIST_RESOLUTION + 1)

typedef RF_uint32 RF_Hist_t;

#define RF_HIST_ADD(_hist_,_val_) { \
	RF_Hist_t val; \
	val = ((RF_Hist_t)(_val_)) / 1000; \
	if (val >= RF_HIST_MAX_VAL) \
		_hist_[RF_HIST_NUM_BUCKETS-1]++; \
	else \
		_hist_[(val - RF_HIST_MIN_VAL) / RF_HIST_RESOLUTION]++; \
}

#endif /* !_RF__RF_HIST_H_ */
