/*	$OpenBSD: rf_archs.h,v 1.1 1999/01/11 14:28:59 niklas Exp $	*/
/*	$NetBSD: rf_archs.h,v 1.1 1998/11/13 04:20:26 oster Exp $	*/
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

/* rf_archs.h -- defines for which architectures you want to
 * include is some particular build of raidframe.  Unfortunately,
 * it's difficult to exclude declustering, P+Q, and distributed
 * sparing because the code is intermixed with RAID5 code.  This
 * should be fixed.
 *
 * this is really intended only for use in the kernel, where I
 * am worried about the size of the object module.  At user level and
 * in the simulator, I don't really care that much, so all the
 * architectures can be compiled together.  Note that by itself, turning
 * off these defines does not affect the size of the executable; you
 * have to edit the makefile for that.
 *
 * comment out any line below to eliminate that architecture.
 * the list below includes all the modules that can be compiled
 * out.
 *
 * :  
 * Log: rf_archs.h,v 
 * Revision 1.32  1996/08/20 23:05:40  jimz
 * define RF_KEEP_DISKSTATS to 1
 *
 * Revision 1.31  1996/07/31  15:34:04  jimz
 * include evenodd
 *
 * Revision 1.30  1996/07/27  23:36:08  jimz
 * Solaris port of simulator
 *
 * Revision 1.29  1996/07/26  20:11:46  jimz
 * only define RF_DEMO for CMU_PDL
 *
 * Revision 1.28  1996/07/26  20:10:57  jimz
 * define RF_CMU_PDL only if it isn't already defined
 *
 * Revision 1.27  1996/07/18  22:57:14  jimz
 * port simulator to AIX
 *
 * Revision 1.26  1996/06/17  14:38:33  jimz
 * properly #if out RF_DEMO code
 * fix bug in MakeConfig that was causing weird behavior
 * in configuration routines (config was not zeroed at start)
 * clean up genplot handling of stacks
 *
 * Revision 1.25  1996/06/14  21:24:59  jimz
 * turn on RF_CMU_PDL by default
 *
 * Revision 1.24  1996/06/13  20:41:57  jimz
 * add RF_INCLUDE_QUEUE_RANDOM (0)
 *
 * Revision 1.23  1996/06/11  18:12:36  jimz
 * get rid of JOIN operations
 * use ThreadGroup stuff instead
 * fix some allocation/deallocation and sync bugs
 *
 * Revision 1.22  1996/06/10  22:24:55  wvcii
 * added symbols for enabling forward or backward error
 * recovery experiments
 *
 * Revision 1.21  1996/06/05  18:06:02  jimz
 * Major code cleanup. The Great Renaming is now done.
 * Better modularity. Better typing. Fixed a bunch of
 * synchronization bugs. Made a lot of global stuff
 * per-desc or per-array. Removed dead code.
 *
 * Revision 1.20  1996/05/30  11:29:41  jimz
 * Numerous bug fixes. Stripe lock release code disagreed with the taking code
 * about when stripes should be locked (I made it consistent: no parity, no lock)
 * There was a lot of extra serialization of I/Os which I've removed- a lot of
 * it was to calculate values for the cache code, which is no longer with us.
 * More types, function, macro cleanup. Added code to properly quiesce the array
 * on shutdown. Made a lot of stuff array-specific which was (bogusly) general
 * before. Fixed memory allocation, freeing bugs.
 *
 * Revision 1.19  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.18  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.17  1996/05/23  00:33:23  jimz
 * code cleanup: move all debug decls to rf_options.c, all extern
 * debug decls to rf_options.h, all debug vars preceded by rf_
 *
 * Revision 1.16  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.15  1996/05/15  22:32:59  jimz
 * remove cache and vs stuff
 *
 * Revision 1.14  1995/11/30  16:27:34  wvcii
 * added copyright info
 *
 * Revision 1.13  1995/11/28  21:23:44  amiri
 * added the interleaved declustering architecture
 * ('I'), with distributed sparing.
 *
 * Revision 1.12  1995/11/17  16:59:45  amiri
 * don't INCLUDE_CHAINDECLUSTER in the kernel
 * source.
 *
 * Revision 1.11  1995/11/16  16:15:21  amiri
 * don't include RAID5 with rotated sparing (INCLUDE_RAID5_RS) in kernel
 *
 * Revision 1.10  1995/10/12  17:40:47  jimz
 * define INCLUDE_LS
 *
 * Revision 1.9  1995/10/11  06:56:47  jimz
 * define INCLUDE_VS (sanity check for compilation)
 *
 * Revision 1.8  1995/10/05  18:56:24  jimz
 * don't INCLUDE_VS
 *
 * Revision 1.7  1995/10/04  03:51:20  wvcii
 * added raid 1
 *
 * Revision 1.6  1995/09/07  09:59:29  wvcii
 * unstable archs conditionally defined for !KERNEL makes
 *
 *
 */

#ifndef _RF__RF_ARCHS_H_
#define _RF__RF_ARCHS_H_

/*
 * Turn off if you do not have CMU PDL support compiled
 * into your kernel.
 */
#ifndef RF_CMU_PDL
#define RF_CMU_PDL 0
#endif /* !RF_CMU_PDL */

/*
 * Khalil's performance-displaying demo stuff.
 * Relies on CMU meter tools.
 */
#ifndef KERNEL
#if RF_CMU_PDL > 0
#define RF_DEMO 1
#endif /* RF_CMU_PDL > 0 */
#endif /* !KERNEL */

#define RF_INCLUDE_EVENODD       1

#define RF_INCLUDE_RAID5_RS      1
#define RF_INCLUDE_PARITYLOGGING 1

#define RF_INCLUDE_CHAINDECLUSTER 1
#define RF_INCLUDE_INTERDECLUSTER 1

#define RF_INCLUDE_RAID0   1
#define RF_INCLUDE_RAID1   1
#define RF_INCLUDE_RAID4   1
#define RF_INCLUDE_RAID5   1
#define RF_INCLUDE_RAID6   0
#define RF_INCLUDE_DECL_PQ 0

#define RF_MEMORY_REDZONES 0
#define RF_RECON_STATS     1

#define RF_INCLUDE_QUEUE_RANDOM 0

#define RF_KEEP_DISKSTATS 1

/* These two symbols enable nonstandard forms of error recovery.
 * These modes are only valid for performance measurements and
 * data corruption will occur if an error occurs when either
 * forward or backward error recovery are enabled.  In general
 * both of the following two definitions should be commented
 * out--this forces RAIDframe to use roll-away error recovery
 * which does guarantee proper error recovery without data corruption
 */
/* #define RF_FORWARD 1 */
/* #define RF_BACKWARD 1 */

#include "rf_options.h"

#endif /* !_RF__RF_ARCHS_H_ */
