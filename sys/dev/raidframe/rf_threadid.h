/*	$OpenBSD: rf_threadid.h,v 1.2 1999/02/16 00:03:31 niklas Exp $	*/
/*	$NetBSD: rf_threadid.h,v 1.3 1999/02/05 00:06:18 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Daniel Stodolsky, Mark Holland
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

/* rf_threadid.h
 *
 * simple macros to register and lookup integer identifiers for threads.
 * must include pthread.h before including this
 *
 * This is one of two places where the pthreads package is used explicitly.
 * The other is in threadstuff.h
 *
 * none of this is used in the kernel, so it all gets compiled out if KERNEL is defined
 */

#ifndef _RF__RF_THREADID_H_
#define _RF__RF_THREADID_H_

/*
 * Kernel
 */

#define RF_DECLARE_GLOBAL_THREADID
#define rf_setup_threadid()
#define rf_shutdown_threadid()
#define rf_assign_threadid()

#define rf_get_threadid(_id_) _id_ = 0;

#endif				/* !_RF__RF_THREADID_H_ */
