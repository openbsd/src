/*	$OpenBSD: rf_dagflags.h,v 1.1 1999/01/11 14:29:10 niklas Exp $	*/
/*	$NetBSD: rf_dagflags.h,v 1.1 1998/11/13 04:20:27 oster Exp $	*/
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

/**************************************************************************************
 *
 * dagflags.h -- flags that can be given to DoAccess
 * I pulled these out of dag.h because routines that call DoAccess may need these flags,
 * but certainly do not need the declarations related to the DAG data structures.
 *
 **************************************************************************************/

/* :  
 * Log: rf_dagflags.h,v 
 * Revision 1.10  1996/06/13 19:08:23  jimz
 * remove unused BD flag
 *
 * Revision 1.9  1996/05/30  11:29:41  jimz
 * Numerous bug fixes. Stripe lock release code disagreed with the taking code
 * about when stripes should be locked (I made it consistent: no parity, no lock)
 * There was a lot of extra serialization of I/Os which I've removed- a lot of
 * it was to calculate values for the cache code, which is no longer with us.
 * More types, function, macro cleanup. Added code to properly quiesce the array
 * on shutdown. Made a lot of stuff array-specific which was (bogusly) general
 * before. Fixed memory allocation, freeing bugs.
 *
 * Revision 1.8  1996/05/24  22:17:04  jimz
 * continue code + namespace cleanup
 * typed a bunch of flags
 *
 * Revision 1.7  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.6  1995/12/01  15:59:40  root
 * added copyright info
 *
 */

#ifndef _RF__RF_DAGFLAGS_H_
#define _RF__RF_DAGFLAGS_H_

/*
 * Bitmasks for the "flags" parameter (RF_RaidAccessFlags_t) used
 * by DoAccess, SelectAlgorithm, and the DAG creation routines.
 *
 * If USE_DAG or USE_ASM is specified, neither the DAG nor the ASM
 * will be modified, which means that you can't SUPRESS if you
 * specify USE_DAG.
 */

#define RF_DAG_FLAGS_NONE             0  /* no flags */
#define RF_DAG_SUPPRESS_LOCKS     (1<<0) /* supress all stripe locks in the DAG */
#define RF_DAG_RETURN_ASM         (1<<1) /* create an ASM and return it instead of freeing it */
#define RF_DAG_RETURN_DAG         (1<<2) /* create a DAG and return it instead of freeing it */
#define RF_DAG_NONBLOCKING_IO     (1<<3) /* cause DoAccess to be non-blocking */
#define RF_DAG_ACCESS_COMPLETE    (1<<4) /* the access is complete */
#define RF_DAG_DISPATCH_RETURNED  (1<<5) /* used to handle the case where the dag invokes no I/O */
#define RF_DAG_TEST_ACCESS        (1<<6) /* this access came through rf_ioctl instead of rf_strategy */

#endif /* !_RF__RF_DAGFLAGS_H_ */
