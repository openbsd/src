/*	$OpenBSD: rf_utility.h,v 1.1 1999/01/11 14:49:45 niklas Exp $	*/

/*
 * rf_utility.h
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
 * defs for raidframe utils which share .c files with
 * raidframe proper
 */

#ifndef _RF__RF_UTILITY_H_
#define _RF__RF_UTILITY_H_

#include "rf_options.h"

#define rf_create_managed_mutex(a,b) 0
#define rf_create_managed_cond(a,b) 0

#define RF_DECLARE_MUTEX(m) int m;
#define RF_DECLARE_EXTERN_MUTEX(m) extern int m;
#define RF_LOCK_MUTEX(m)
#define RF_UNLOCK_MUTEX(m)

#endif /* !_RF__RF_UTILITY_H_ */
