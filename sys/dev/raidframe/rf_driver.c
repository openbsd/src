/*	$OpenBSD: rf_driver.c,v 1.1 1999/01/11 14:29:18 niklas Exp $	*/
/*	$NetBSD: rf_driver.c,v 1.2 1998/11/13 13:45:15 drochner Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland, Khalil Amiri, Claudson Bornstein, William V. Courtright II,
 *         Robby Findler, Daniel Stodolsky, Rachad Youssef, Jim Zelenka
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

/******************************************************************************
 *
 * rf_driver.c -- main setup, teardown, and access routines for the RAID driver
 *
 * all routines are prefixed with rf_ (raidframe), to avoid conficts.
 *
 ******************************************************************************/

/*
 * :  
 * Log: rf_driver.c,v 
 * Revision 1.147  1996/08/21 04:12:46  jimz
 * added hook for starting out req_hist w/ more distributed values
 * (currently not done)
 *
 * Revision 1.146  1996/07/29  14:05:12  jimz
 * fix numPUs/numRUs confusion (everything is now numRUs)
 * clean up some commenting, return values
 *
 * Revision 1.145  1996/07/28  20:31:39  jimz
 * i386netbsd port
 * true/false fixup
 *
 * Revision 1.144  1996/07/27  18:40:24  jimz
 * cleanup sweep
 *
 * Revision 1.143  1996/07/22  21:11:53  jimz
 * fix formatting on DoAccess error msg
 *
 * Revision 1.142  1996/07/19  16:10:06  jimz
 * added call to rf_ResetDebugOptions() in rf_ConfigureDebug()
 *
 * Revision 1.141  1996/07/18  22:57:14  jimz
 * port simulator to AIX
 *
 * Revision 1.140  1996/07/17  21:00:58  jimz
 * clean up timer interface, tracing
 *
 * Revision 1.139  1996/07/15  05:40:41  jimz
 * some recon datastructure cleanup
 * better handling of multiple failures
 * added undocumented double-recon test
 *
 * Revision 1.138  1996/07/11  19:08:00  jimz
 * generalize reconstruction mechanism
 * allow raid1 reconstructs via copyback (done with array
 * quiesced, not online, therefore not disk-directed)
 *
 * Revision 1.137  1996/07/10  22:28:00  jimz
 * get rid of obsolete row statuses (dead,degraded2)
 *
 * Revision 1.136  1996/06/17  14:38:33  jimz
 * properly #if out RF_DEMO code
 * fix bug in MakeConfig that was causing weird behavior
 * in configuration routines (config was not zeroed at start)
 * clean up genplot handling of stacks
 *
 * Revision 1.135  1996/06/17  03:20:32  jimz
 * move out raidframe_attr_default
 * don't monkey with stack sizes
 *
 * Revision 1.134  1996/06/14  23:15:38  jimz
 * attempt to deal with thread GC problem
 *
 * Revision 1.133  1996/06/14  21:24:08  jimz
 * new ConfigureEtimer init
 * moved out timer vars
 *
 * Revision 1.132  1996/06/14  16:19:03  jimz
 * remove include of pdllib.h (beginning of PDL cleanup)
 *
 * Revision 1.131  1996/06/14  14:35:24  jimz
 * clean up dfstrace protection
 *
 * Revision 1.130  1996/06/14  14:16:09  jimz
 * engine config is now array-specific
 *
 * Revision 1.129  1996/06/13  19:08:10  jimz
 * add debug var to force keep_acc_totals on
 *
 * Revision 1.128  1996/06/11  10:57:08  jimz
 * init recon_done_proc_mutex
 *
 * Revision 1.127  1996/06/10  14:18:58  jimz
 * move user, throughput stats into per-array structure
 *
 * Revision 1.126  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.125  1996/06/09  02:36:46  jimz
 * lots of little crufty cleanup- fixup whitespace
 * issues, comment #ifdefs, improve typing in some
 * places (esp size-related)
 *
 * Revision 1.124  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.123  1996/06/05  19:38:32  jimz
 * fixed up disk queueing types config
 * added sstf disk queueing
 * fixed exit bug on diskthreads (ref-ing bad mem)
 *
 * Revision 1.122  1996/06/05  18:06:02  jimz
 * Major code cleanup. The Great Renaming is now done.
 * Better modularity. Better typing. Fixed a bunch of
 * synchronization bugs. Made a lot of global stuff
 * per-desc or per-array. Removed dead code.
 *
 * Revision 1.121  1996/06/03  23:28:26  jimz
 * more bugfixes
 * check in tree to sync for IPDS runs with current bugfixes
 * there still may be a problem with threads in the script test
 * getting I/Os stuck- not trivially reproducible (runs ~50 times
 * in a row without getting stuck)
 *
 * Revision 1.120  1996/06/02  17:31:48  jimz
 * Moved a lot of global stuff into array structure, where it belongs.
 * Fixed up paritylogging, pss modules in this manner. Some general
 * code cleanup. Removed lots of dead code, some dead files.
 *
 * Revision 1.119  1996/05/31  22:26:54  jimz
 * fix a lot of mapping problems, memory allocation problems
 * found some weird lock issues, fixed 'em
 * more code cleanup
 *
 * Revision 1.118  1996/05/30  23:22:16  jimz
 * bugfixes of serialization, timing problems
 * more cleanup
 *
 * Revision 1.117  1996/05/30  16:28:33  jimz
 * typo in rf_SignalQuiescenceLock() fixed
 *
 * Revision 1.116  1996/05/30  12:59:18  jimz
 * make etimer happier, more portable
 *
 * Revision 1.115  1996/05/30  11:29:41  jimz
 * Numerous bug fixes. Stripe lock release code disagreed with the taking code
 * about when stripes should be locked (I made it consistent: no parity, no lock)
 * There was a lot of extra serialization of I/Os which I've removed- a lot of
 * it was to calculate values for the cache code, which is no longer with us.
 * More types, function, macro cleanup. Added code to properly quiesce the array
 * on shutdown. Made a lot of stuff array-specific which was (bogusly) general
 * before. Fixed memory allocation, freeing bugs.
 *
 * Revision 1.114  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.113  1996/05/24  22:17:04  jimz
 * continue code + namespace cleanup
 * typed a bunch of flags
 *
 * Revision 1.112  1996/05/24  04:28:55  jimz
 * release cleanup ckpt
 *
 * Revision 1.111  1996/05/24  01:59:45  jimz
 * another checkpoint in code cleanup for release
 * time to sync kernel tree
 *
 * Revision 1.110  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.109  1996/05/23  00:39:56  jimz
 * demoMode -> rf_demoMode
 *
 * Revision 1.108  1996/05/23  00:33:23  jimz
 * code cleanup: move all debug decls to rf_options.c, all extern
 * debug decls to rf_options.h, all debug vars preceded by rf_
 *
 * Revision 1.107  1996/05/21  14:30:04  jimz
 * idler_desc_mutex should be ifndef SIMULATE
 *
 * Revision 1.106  1996/05/20  19:31:12  jimz
 * add atomic debug (mutex and cond leak finder) stuff
 *
 * Revision 1.105  1996/05/20  16:12:45  jimz
 * switch to rf_{mutex,cond}_{init,destroy}
 *
 * Revision 1.104  1996/05/18  20:09:41  jimz
 * bit of cleanup to compile cleanly in kernel, once again
 *
 * Revision 1.103  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.102  1996/05/16  21:20:51  jimz
 * use FREELIST stuff to manage access descriptors
 *
 * Revision 1.101  1996/05/16  14:21:10  jimz
 * remove bogus copies from write path on user
 *
 * Revision 1.100  1996/05/15  22:33:54  jimz
 * appropriately #ifdef cache stuff
 *
 * Revision 1.99  1996/05/08  21:34:41  jimz
 * #if 0 ShutdownCache() and ConfigureCache()
 *
 * Revision 1.98  1996/05/08  21:01:24  jimz
 * fixed up enum type names that were conflicting with other
 * enums and function names (ie, "panic")
 * future naming trends will be towards RF_ and rf_ for
 * everything raidframe-related
 *
 * Revision 1.97  1996/05/07  19:02:58  wvcii
 * corrected header comment of rf_DoAccess()
 * reordered free of desc in FreeRaidAccDesc()  The desc is now
 * freed last.
 *
 * Revision 1.96  1996/05/07  17:40:50  jimz
 * add doDebug
 *
 * Revision 1.95  1996/05/06  21:35:23  jimz
 * fixed ordering of cleanup and removed extra decrement of configureCount
 *
 * Revision 1.94  1996/05/06  18:44:14  jimz
 * reorder cleanup to not blow alloclist out from under various modules
 * zero raidPtr contents on config
 *
 * Revision 1.93  1996/05/04  17:06:53  jimz
 * Fail the I/O with ENOSPC if reading past end of the array in the kernel.
 *
 * Revision 1.92  1996/05/03  19:44:22  wvcii
 * debug vars degDagDebug and enableAtomicRMW now defined
 * in this file.
 *
 * Revision 1.91  1995/12/12  18:10:06  jimz
 * MIN -> RF_MIN, MAX -> RF_MAX, ASSERT -> RF_ASSERT
 * fix 80-column brain damage in comments
 *
 * Revision 1.90  1995/12/08  15:07:03  arw
 * cache code cleanup
 *
 * Revision 1.89  1995/12/06  20:53:58  wvcii
 * created debug var forceParityLogReint
 * this variable forces reintegration of all parity logs at shutdown
 *
 * Revision 1.88  1995/12/01  15:59:10  root
 * added copyright info
 *
 * Revision 1.87  1995/11/28  21:34:02  amiri
 * modified SetReconfiguredMode so that it installs the
 * spare table only if arch is declustered based on block designs
 *
 * Revision 1.86  1995/11/21  23:06:11  amiri
 * added division by zero check in printing
 * throughput stats.
 *
 * Revision 1.85  1995/11/19  16:27:25  wvcii
 * disableParityVerify now defined locally, only read from config
 * file for !KERNEL compiles
 *
 * Revision 1.84  1995/11/17  15:08:31  wvcii
 * added debug var disableParityVerify
 * used in RealLoopTest to disable parity verification
 *
 * Revision 1.83  1995/11/07  15:48:43  wvcii
 * deleted debug vars: suppressAtomicRMW, enableRollAway, concatDagDebug
 * deleted debug vars: debugSelectUnit, debugSelectBlock
 * added debug var: enableAtomicRMW
 *
 * Revision 1.82  1995/10/18  19:28:45  amiri
 * added support for reconstruction demos in the
 * simulator, by updating some simulator
 * variables in Faildisk.
 *
 * Revision 1.81  1995/10/09  18:36:33  jimz
 * move rf_StopThroughputStats() into FreeAccDesc()
 * changed throughput output print format
 * added user-level copy to write path to emulate kernel hack
 *
 * Revision 1.80  1995/10/09  18:07:47  wvcii
 * moved call to rf_StopThroughputStats to rf_states.c
 *
 * Revision 1.79  1995/10/09  17:38:53  jimz
 * quiesce an array for user-level testing before shutting it down
 * (should this also be done in the kernel?)
 *
 * Revision 1.78  1995/10/09  15:35:43  wvcii
 * added code to measure throughput in user mode
 *
 * Revision 1.77  1995/10/05  06:18:59  jimz
 * Changed DDEventRequest() to take additional arg, used by simulator
 * to cache diskid so queue length can be decremented on io complete
 * (this is a hack to get around the fact that the event mechanism
 * assumes it can dereference arbitrary handles on enqueued events)
 *
 * Revision 1.76  1995/10/04  07:25:10  jimz
 * turn off bigstacks by default
 *
 * Revision 1.75  1995/10/04  07:24:34  jimz
 * code for bigstacks in user process
 *
 * Revision 1.74  1995/09/26  21:42:51  wvcii
 * removed calls to ConfigureCache, ShutdownCache when building kernel
 * kernel currently does not support any cached architectures
 *
 * Revision 1.73  1995/09/20  21:05:35  jimz
 * add missing unit arg to IO_BUF_ERR() in non-kernel case
 *
 * Revision 1.72  1995/09/19  23:02:44  jimz
 * call RF_DKU_END_IO in the appropriate places
 *
 * Revision 1.71  1995/09/07  19:02:31  jimz
 * mods to get raidframe to compile and link
 * in kernel environment
 *
 * Revision 1.70  1995/09/06  19:24:01  wvcii
 * added debug vars enableRollAway and debugRecovery
 *
 * Revision 1.69  1995/08/24  19:25:36  rachad
 * Fixes to LSS GC in the simulater
 *
 * Revision 1.68  1995/07/28  21:43:42  robby
 * checkin after leaving for Rice. Bye
 *
 * Revision 1.67  1995/07/26  18:06:52  cfb
 * *** empty log message ***
 *
 * Revision 1.66  1995/07/26  03:25:24  robby
 * fixed accesses mutex and updated call to ConfigureCache
 *
 * Revision 1.65  1995/07/25  14:36:52  rachad
 * *** empty log message ***
 *
 * Revision 1.64  1995/07/21  19:29:05  robby
 * added total_accesses
 *
 * Revision 1.63  1995/07/20  19:43:35  cfb
 * *** empty log message ***
 *
 * Revision 1.62  1995/07/20  16:10:24  rachad
 * *** empty log message ***
 *
 * Revision 1.61  1995/07/20  03:36:53  rachad
 * Added suport for cache warming
 *
 * Revision 1.60  1995/07/17  22:31:31  cfb
 * *** empty log message ***
 *
 * Revision 1.59  1995/07/16  17:02:23  cfb
 * *** empty log message ***
 *
 * Revision 1.58  1995/07/16  15:19:27  cfb
 * *** empty log message ***
 *
 * Revision 1.57  1995/07/16  03:17:01  cfb
 * *** empty log message ***
 *
 * Revision 1.56  1995/07/13  16:11:59  cfb
 * *** empty log message ***
 *
 * Revision 1.55  1995/07/13  15:42:40  cfb
 * added cacheDebug variable ...
 *
 * Revision 1.54  1995/07/13  14:28:27  rachad
 * *** empty log message ***
 *
 * Revision 1.53  1995/07/10  21:48:52  robby
 * added virtualStripingWarnings
 *
 * Revision 1.52  1995/07/10  20:41:13  rachad
 * *** empty log message ***
 *
 * Revision 1.51  1995/07/09  19:46:49  cfb
 * Added cache Shutdown
 *
 * Revision 1.50  1995/07/08  21:38:53  rachad
 * Added support for interactive traces
 * in the simulator
 *
 * Revision 1.49  1995/07/08  18:05:39  rachad
 * Linked up Claudsons code with the real cache
 *
 * Revision 1.48  1995/07/07  16:00:22  cfb
 * Added initialization of cacheDesc to AllocRaidAccDesc
 *
 * Revision 1.47  1995/07/06  14:22:37  rachad
 * Merge complete
 *
 * Revision 1.46.50.2  1995/06/21  17:48:30  robby
 * test
 *
 * Revision 1.46.50.1  1995/06/21  17:34:49  robby
 * branching to work on "meta-dag" capabilities
 *
 * Revision 1.46.10.5  1995/07/03  21:58:34  holland
 * added support for suppressing both stripe locks & large writes
 *
 * Revision 1.46.10.4  1995/06/27  03:42:48  holland
 * typo fix
 *
 * Revision 1.46.10.3  1995/06/27  03:31:42  holland
 * prototypes
 *
 * Revision 1.46.10.2  1995/06/27  03:17:57  holland
 * fixed callback bug in kernel rf_DoAccess
 *
 * Revision 1.46.10.1  1995/06/25  14:32:44  holland
 * initial checkin on new branch
 *
 * Revision 1.46  1995/06/13  17:52:41  holland
 * added UserStats stuff
 *
 * Revision 1.45  1995/06/13  16:03:41  rachad
 * *** empty log message ***
 *
 * Revision 1.44  1995/06/12  15:54:40  rachad
 * Added garbege collection for log structured storage
 *
 * Revision 1.43  1995/06/09  18:01:09  holland
 * various changes related to in-kernel recon, multiple-row arrays,
 * trace extraction from kernel, etc.
 *
 * Revision 1.42  1995/06/08  19:52:28  rachad
 * *** empty log message ***
 *
 * Revision 1.41  1995/06/08  00:11:49  robby
 * added a debug variable -- showVirtualSizeRequirements
 *
 * Revision 1.40  1995/06/05  00:33:30  holland
 * protectedSectors bug fix
 *
 * Revision 1.39  1995/06/01  22:45:03  holland
 * made compilation of parity logging and virtual striping
 * stuff conditional on some constants defined in rf_archs.h
 *
 * Revision 1.38  1995/06/01  21:52:37  holland
 * replaced NULL sizes in calls to Free() by -1, and caused this
 * to suppress the size-mismatch error
 *
 * Revision 1.37  1995/05/26  20:04:54  wvcii
 * modified parity logging debug vars
 *
 * Revision 1.36  95/05/21  15:32:41  wvcii
 * added debug vars: parityLogDebug, numParityRegions, numParityLogs,
 * numReintegrationThreads
 * 
 * Revision 1.35  95/05/19  20:58:21  holland
 * cleanups on error cases in rf_DoAccess
 * 
 * Revision 1.34  1995/05/16  17:35:53  holland
 * added rf_copyback_in_progress.  this is debug-only.
 *
 * Revision 1.33  1995/05/15  12:25:35  holland
 * bug fix in test code: no stripe locks were getting acquired in RAID0 mode
 *
 * Revision 1.32  1995/05/10  18:54:12  holland
 * bug fixes related to deadlock problem at time of disk failure
 * eliminated read-op-write code
 * beefed up parity checking in loop test
 * various small changes & new ASSERTs
 *
 * Revision 1.31  1995/05/02  22:49:02  holland
 * add shutdown calls for each architecture
 *
 * Revision 1.30  1995/05/01  14:43:37  holland
 * merged changes from Bill
 *
 * Revision 1.29  1995/05/01  13:28:00  holland
 * parity range locks, locking disk requests, recon+parityscan in kernel, etc.
 *
 * Revision 1.28  1995/04/24  13:25:51  holland
 * rewrite to move disk queues, recon, & atomic RMW to kernel
 *
 * Revision 1.27  1995/04/06  14:47:56  rachad
 * merge completed
 *
 * Revision 1.26  1995/04/03  20:32:35  rachad
 * added reconstruction to simulator
 *
 * Revision 1.25.10.2  1995/04/03  20:41:00  holland
 * misc changes related to distributed sparing
 *
 * Revision 1.25.10.1  1995/03/17  20:04:01  holland
 * initial checkin on new branch
 *
 * Revision 1.25  1995/03/15  20:34:30  holland
 * changes for distributed sparing.
 *
 * Revision 1.24  1995/03/09  19:53:05  rachad
 * *** empty log message ***
 *
 * Revision 1.23  1995/03/03  18:36:16  rachad
 *  Simulator mechanism added
 *
 * Revision 1.22  1995/03/01  20:25:48  holland
 * kernelization changes
 *
 * Revision 1.21  1995/02/17  19:39:56  holland
 * added size param to all calls to Free().
 * this is ignored at user level, but necessary in the kernel.
 *
 * Revision 1.20  1995/02/17  13:37:49  holland
 * kernelization changes -- not yet complete
 *
 * Revision 1.19  1995/02/10  18:08:07  holland
 * fixed a few things I broke during kernelization
 *
 * Revision 1.18  1995/02/10  17:34:10  holland
 * kernelization changes
 *
 * Revision 1.17  1995/02/04  15:51:35  holland
 * kernelization changes
 *
 * Revision 1.16  1995/02/03  22:31:36  holland
 * many changes related to kernelization
 *
 * Revision 1.15  1995/02/01  15:13:05  holland
 * moved #include of general.h out of raid.h and into each file
 *
 * Revision 1.14  1995/02/01  14:25:19  holland
 * began changes for kernelization:
 *      changed all instances of mutex_t and cond_t to DECLARE macros
 *      converted configuration code to use config structure
 *
 * Revision 1.13  1995/01/30  14:53:46  holland
 * extensive changes related to making DoIO non-blocking
 *
 * Revision 1.12  1995/01/25  00:26:21  holland
 * eliminated support for aio
 *
 * Revision 1.11  1995/01/24  23:58:46  holland
 * multi-way recon XOR, plus various small changes
 *
 * Revision 1.10  1995/01/11  19:27:02  holland
 * various changes related to performance tuning
 *
 * Revision 1.9  1994/12/05  15:29:09  holland
 * added trace run time limitation (maxTraceRunTimeSec)
 *
 * Revision 1.8  1994/12/05  04:18:12  holland
 * various new control vars in the config file
 *
 * Revision 1.7  1994/11/29  23:11:36  holland
 * tracerec bug on dag retry fixed
 *
 * Revision 1.6  1994/11/29  22:11:38  danner
 * holland updates
 *
 * Revision 1.5  1994/11/29  21:09:47  danner
 * Detailed tracing support (holland).
 *
 * Revision 1.4  1994/11/29  20:36:02  danner
 * Added suppressAtomicRMW option.
 *
 * Revision 1.3  1994/11/21  15:34:06  danner
 * Added ConfigureAllocList() call.
 * 
 */

#ifdef _KERNEL
#define KERNEL
#endif

#if (defined(__NetBSD__) || defined(__OpenBSD__)) && defined(_KERNEL)
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#ifdef __NETBSD__
#include <sys/vnode.h>
#endif
#endif

#ifdef KERNEL
#if !defined(__NetBSD__) && !defined(__OpenBSD__)
#include <dkusage.h>
#include <dfstrace.h>
#endif /* !__NetBSD__ && !__OpenBSD__ */
#endif /* KERNEL */

#include "rf_archs.h"
#include "rf_threadstuff.h"

#ifndef KERNEL
#include <stdio.h>
#include <stdlib.h>
#endif /* KERNEL */

#include <sys/errno.h>

#include "rf_raid.h"
#include "rf_dag.h"
#include "rf_aselect.h"
#include "rf_diskqueue.h"
#include "rf_parityscan.h"
#include "rf_alloclist.h"
#include "rf_threadid.h"
#include "rf_dagutils.h"
#include "rf_utils.h"
#include "rf_etimer.h"
#include "rf_acctrace.h"
#include "rf_configure.h"
#include "rf_general.h"
#include "rf_desc.h"
#include "rf_states.h"
#include "rf_freelist.h"
#include "rf_decluster.h"
#include "rf_map.h"
#include "rf_diskthreads.h"
#include "rf_revent.h"
#include "rf_callback.h"
#include "rf_engine.h"
#include "rf_memchunk.h"
#include "rf_mcpair.h"
#include "rf_nwayxor.h"
#include "rf_debugprint.h"
#include "rf_copyback.h"
#if !defined(__NetBSD__) && !defined(__OpenBSD__)
#include "rf_camlayer.h"
#endif
#include "rf_driver.h"
#include "rf_options.h"
#include "rf_shutdown.h"
#include "rf_sys.h"
#include "rf_cpuutil.h"

#ifdef SIMULATE
#include "rf_diskevent.h"
#endif /* SIMULATE */

#ifdef KERNEL
#include <sys/buf.h>
#if !defined(__NetBSD__) && !defined(__OpenBSD__)
#include <io/common/devdriver.h>
#endif /* !__NetBSD__ && !__OpenBSD__ */

#if DFSTRACE > 0
#include <sys/dfs_log.h>
#include <sys/dfstracebuf.h>
#endif /* DFSTRACE > 0 */

#if DKUSAGE > 0
#include <sys/dkusage.h>
#include <io/common/iotypes.h>
#include <io/cam/dec_cam.h>
#include <io/cam/cam.h>
#include <io/cam/pdrv.h>
#endif /* DKUSAGE > 0 */
#endif /* KERNEL */

#if RF_DEMO > 0
#include "rf_demo.h"
#endif /* RF_DEMO > 0 */

/* rad == RF_RaidAccessDesc_t */
static RF_FreeList_t *rf_rad_freelist;
#define RF_MAX_FREE_RAD 128
#define RF_RAD_INC       16
#define RF_RAD_INITIAL   32

/* debug variables */
char rf_panicbuf[2048];       /* a buffer to hold an error msg when we panic */ 

/* main configuration routines */
static int raidframe_booted = 0;

static void rf_ConfigureDebug(RF_Config_t *cfgPtr);
static void set_debug_option(char *name, long val);
static void rf_UnconfigureArray(void);
static int init_rad(RF_RaidAccessDesc_t *);
static void clean_rad(RF_RaidAccessDesc_t *);
static void rf_ShutdownRDFreeList(void *);
static int rf_ConfigureRDFreeList(RF_ShutdownList_t **);


RF_DECLARE_MUTEX(rf_printf_mutex)          /* debug only:  avoids interleaved printfs by different stripes */
RF_DECLARE_GLOBAL_THREADID                 /* declarations for threadid.h */

#if !defined(KERNEL) && !defined(SIMULATE)
static int rf_InitThroughputStats(RF_ShutdownList_t **listp, RF_Raid_t *raidPtr, RF_Config_t *cfgPtr);
static void rf_StopThroughputStats(RF_Raid_t *raidPtr);
static void rf_PrintThroughputStats(RF_Raid_t *raidPtr);
#endif /* !KERNEL && !SIMULATE */

#ifdef KERNEL
#define SIGNAL_QUIESCENT_COND(_raid_)  wakeup(&((_raid_)->accesses_suspended))
#if !defined(__NetBSD__) && !defined(__OpenBSD__)
#define WAIT_FOR_QUIESCENCE(_raid_) \
  mpsleep(&((_raid_)->accesses_suspended), PZERO, "raidframe quiesce", 0, \
      (void *) simple_lock_addr((_raid_)->access_suspend_mutex), MS_LOCK_SIMPLE)
#else
#define WAIT_FOR_QUIESCENCE(_raid_) \
	tsleep(&((_raid_)->accesses_suspended),PRIBIO|PCATCH,"raidframe quiesce", 0);

#endif
#if DKUSAGE > 0
#define IO_BUF_ERR(bp, err, unit) { \
	bp->b_flags |= B_ERROR; \
	bp->b_resid = bp->b_bcount; \
	bp->b_error = err; \
	RF_DKU_END_IO(unit, bp); \
	biodone(bp); \
}
#else
#define IO_BUF_ERR(bp, err, unit) { \
	bp->b_flags |= B_ERROR; \
	bp->b_resid = bp->b_bcount; \
	bp->b_error = err; \
	RF_DKU_END_IO(unit); \
	biodone(bp); \
}
#endif /* DKUSAGE > 0 */
#else /* KERNEL */

#define SIGNAL_QUIESCENT_COND(_raid_)  RF_SIGNAL_COND((_raid_)->quiescent_cond)
#define WAIT_FOR_QUIESCENCE(_raid_)    RF_WAIT_COND((_raid_)->quiescent_cond, (_raid_)->access_suspend_mutex)
#define IO_BUF_ERR(bp, err, unit)

#endif /* KERNEL */

static int configureCount=0;         /* number of active configurations */
static int isconfigged=0;            /* is basic raidframe (non per-array) stuff configged */
RF_DECLARE_STATIC_MUTEX(configureMutex) /* used to lock the configuration stuff */

static RF_ShutdownList_t *globalShutdown; /* non array-specific stuff */

static int rf_ConfigureRDFreeList(RF_ShutdownList_t **listp);

/* called at system boot time */
int rf_BootRaidframe()
{
#if 0
  long stacksize;
#endif
  int rc;

  if (raidframe_booted)
    return(EBUSY);
  raidframe_booted = 1;

#if RF_DEBUG_ATOMIC > 0
  rf_atent_init();
#endif /* RF_DEBUG_ATOMIC > 0 */

  rf_setup_threadid();
  rf_assign_threadid();

#if !defined(KERNEL) && !defined(SIMULATE)
  if (RF_THREAD_ATTR_CREATE(raidframe_attr_default)) {
    fprintf(stderr, "Unable to create default thread attr\n");
    exit(1);
  }
#if 0
  stacksize = RF_THREAD_ATTR_GETSTACKSIZE(raidframe_attr_default);
  if (stacksize < 0) {
    fprintf(stderr, "Unable to get stack size of default thread attr\n");
    exit(1);
  }
  stacksize += 16384;
  rc = RF_THREAD_ATTR_SETSTACKSIZE(raidframe_attr_default, stacksize);
  if (rc) {
    fprintf(stderr, "Unable to set stack size of default thread attr\n");
    exit(1);
  }
#endif /* 0 */
#endif /* !KERNEL && !SIMULATE */
  rc = rf_mutex_init(&configureMutex);
  if (rc) {
    RF_ERRORMSG3("Unable to init mutex file %s line %d rc=%d\n", __FILE__,
      __LINE__, rc);
    RF_PANIC();
  }
  configureCount = 0;
  isconfigged = 0;
  globalShutdown = NULL;
  return(0);
}

/*
 * This function is really just for debugging user-level stuff: it
 * frees up all memory, other RAIDframe resources which might otherwise
 * be kept around. This is used with systems like "sentinel" to detect
 * memory leaks.
 */
int rf_UnbootRaidframe()
{
	int rc;

	RF_LOCK_MUTEX(configureMutex);
	if (configureCount) {
		RF_UNLOCK_MUTEX(configureMutex);
		return(EBUSY);
	}
	raidframe_booted = 0;
	RF_UNLOCK_MUTEX(configureMutex);
	rc = rf_mutex_destroy(&configureMutex);
	if (rc) {
		RF_ERRORMSG3("Unable to destroy mutex file %s line %d rc=%d\n", __FILE__,
			__LINE__, rc);
		RF_PANIC();
	}
#if RF_DEBUG_ATOMIC > 0
	rf_atent_shutdown();
#endif /* RF_DEBUG_ATOMIC > 0 */
	return(0);
}

/*
 * Called whenever an array is shutdown
 */
static void rf_UnconfigureArray()
{
  int rc;

  RF_LOCK_MUTEX(configureMutex);
  if (--configureCount == 0) {              /* if no active configurations, shut everything down */
    isconfigged = 0;

    rc = rf_ShutdownList(&globalShutdown);
    if (rc) {
      RF_ERRORMSG1("RAIDFRAME: unable to do global shutdown, rc=%d\n", rc);
    }

    rf_shutdown_threadid();

    /*
     * We must wait until now, because the AllocList module
     * uses the DebugMem module.
     */
    if (rf_memDebug)
      rf_print_unfreed();
  }
  RF_UNLOCK_MUTEX(configureMutex);
}

/*
 * Called to shut down an array.
 */
int rf_Shutdown(raidPtr)
  RF_Raid_t   *raidPtr;
{
#if !defined(__NetBSD__) && !defined(__OpenBSD__) && !defined(_KERNEL)
  int rc;
#endif
  int r,c;

#if (defined(__NetBSD__) || defined(__OpenBSD__)) && defined(_KERNEL)
  struct proc *p;
#endif

  if (!raidPtr->valid) {
    RF_ERRORMSG("Attempt to shut down unconfigured RAIDframe driver.  Aborting shutdown\n");
    return(EINVAL);
  }

  /*
   * wait for outstanding IOs to land
   * As described in rf_raid.h, we use the rad_freelist lock
   * to protect the per-array info about outstanding descs
   * since we need to do freelist locking anyway, and this
   * cuts down on the amount of serialization we've got going
   * on.
   */
  RF_FREELIST_DO_LOCK(rf_rad_freelist);
  if (raidPtr->waitShutdown) {
    RF_FREELIST_DO_UNLOCK(rf_rad_freelist);
    return(EBUSY);
  }
  raidPtr->waitShutdown = 1;
  while (raidPtr->nAccOutstanding) {
    RF_WAIT_COND(raidPtr->outstandingCond, RF_FREELIST_MUTEX_OF(rf_rad_freelist));
  }
  RF_FREELIST_DO_UNLOCK(rf_rad_freelist);

#if !defined(KERNEL) && !defined(SIMULATE)
  rf_PrintThroughputStats(raidPtr);
#endif /* !KERNEL && !SIMULATE */

  raidPtr->valid = 0;

#if !defined(KERNEL) && !defined(SIMULATE)
  rf_TerminateDiskQueues(raidPtr);           /* tell all disk queues to release any waiting threads */
  rf_ShutdownDiskThreads(raidPtr);           /* wait for all threads to exit */
#endif /* !KERNEL && !SIMULATE */

#if (defined(__NetBSD__) || defined(__OpenBSD__)) && defined(_KERNEL)
  /* We take this opportunity to close the vnodes like we should.. */

  p = raidPtr->proc; /* XXX */

  for(r=0;r<raidPtr->numRow;r++) {
	  for(c=0;c<raidPtr->numCol;c++) {
		  printf("Closing vnode for row: %d col: %d\n",r,c);
		  if (raidPtr->raid_cinfo[r][c].ci_vp) {
			  (void)vn_close(raidPtr->raid_cinfo[r][c].ci_vp, 
					 FREAD|FWRITE,  p->p_ucred, p); 
		  } else {
			  printf("vnode was NULL\n");
		  }
		  
	  }
  }
  for(r=0;r<raidPtr->numSpare;r++) {
	  printf("Closing vnode for spare: %d\n",r);
	  if (raidPtr->raid_cinfo[0][raidPtr->numCol+r].ci_vp) {
		  (void)vn_close(raidPtr->raid_cinfo[0][raidPtr->numCol+r].ci_vp,
				 FREAD|FWRITE,  p->p_ucred, p); 
	  } else {
		  printf("vnode was NULL\n");
	  }
  }


#endif

  rf_ShutdownList(&raidPtr->shutdownList);

  rf_UnconfigureArray();

  return(0);
}

#define DO_INIT_CONFIGURE(f) { \
	rc = f (&globalShutdown); \
	if (rc) { \
		RF_ERRORMSG2("RAIDFRAME: failed %s with %d\n", RF_STRING(f), rc); \
		rf_ShutdownList(&globalShutdown); \
		configureCount--; \
		RF_UNLOCK_MUTEX(configureMutex); \
		return(rc); \
	} \
}

#define DO_RAID_FAIL() { \
	rf_ShutdownList(&raidPtr->shutdownList); \
	rf_UnconfigureArray(); \
}

#define DO_RAID_INIT_CONFIGURE(f) { \
	rc = f (&raidPtr->shutdownList, raidPtr, cfgPtr); \
	if (rc) { \
		RF_ERRORMSG2("RAIDFRAME: failed %s with %d\n", RF_STRING(f), rc); \
		DO_RAID_FAIL(); \
		return(rc); \
	} \
}

#define DO_RAID_MUTEX(_m_) { \
	rc = rf_create_managed_mutex(&raidPtr->shutdownList, (_m_)); \
	if (rc) { \
		RF_ERRORMSG3("Unable to init mutex file %s line %d rc=%d\n", \
			__FILE__, __LINE__, rc); \
		DO_RAID_FAIL(); \
		return(rc); \
	} \
}

#define DO_RAID_COND(_c_) { \
	rc = rf_create_managed_cond(&raidPtr->shutdownList, (_c_)); \
	if (rc) { \
		RF_ERRORMSG3("Unable to init cond file %s line %d rc=%d\n", \
			__FILE__, __LINE__, rc); \
		DO_RAID_FAIL(); \
		return(rc); \
	} \
}

int rf_Configure(raidPtr, cfgPtr)
  RF_Raid_t    *raidPtr;
  RF_Config_t  *cfgPtr;
{
  RF_RowCol_t row, col;
  int i, rc;
  int unit;
  struct proc *p;

  if (raidPtr->valid) {
    RF_ERRORMSG("RAIDframe configuration not shut down.  Aborting configure.\n");
    return(EINVAL);
  }

  RF_LOCK_MUTEX(configureMutex);
  configureCount++;
  if (isconfigged == 0) {
    rc = rf_create_managed_mutex(&globalShutdown, &rf_printf_mutex);
    if (rc) {
      RF_ERRORMSG3("Unable to init mutex file %s line %d rc=%d\n", __FILE__,
        __LINE__, rc);
      rf_ShutdownList(&globalShutdown);
      return(rc);
    }

    /* initialize globals */
    printf("RAIDFRAME: protectedSectors is %ld\n",rf_protectedSectors);

    rf_clear_debug_print_buffer();

    DO_INIT_CONFIGURE(rf_ConfigureAllocList);
    DO_INIT_CONFIGURE(rf_ConfigureEtimer);
    /*
     * Yes, this does make debugging general to the whole system instead
     * of being array specific. Bummer, drag.
     */
    rf_ConfigureDebug(cfgPtr);
    DO_INIT_CONFIGURE(rf_ConfigureDebugMem);
#ifdef SIMULATE
    rf_default_disk_names();
    DO_INIT_CONFIGURE(rf_DDEventInit);
#endif /* SIMULATE */
    DO_INIT_CONFIGURE(rf_ConfigureAccessTrace);
    DO_INIT_CONFIGURE(rf_ConfigureMapModule);
    DO_INIT_CONFIGURE(rf_ConfigureReconEvent);
    DO_INIT_CONFIGURE(rf_ConfigureCallback);
    DO_INIT_CONFIGURE(rf_ConfigureMemChunk);
    DO_INIT_CONFIGURE(rf_ConfigureRDFreeList);
    DO_INIT_CONFIGURE(rf_ConfigureNWayXor);
    DO_INIT_CONFIGURE(rf_ConfigureStripeLockFreeList);
    DO_INIT_CONFIGURE(rf_ConfigureMCPair);
#ifndef SIMULATE
#if !defined(__NetBSD__) && !defined(__OpenBSD__)
    DO_INIT_CONFIGURE(rf_ConfigureCamLayer);
#endif
#endif /* !SIMULATE */
    DO_INIT_CONFIGURE(rf_ConfigureDAGs);
    DO_INIT_CONFIGURE(rf_ConfigureDAGFuncs);
    DO_INIT_CONFIGURE(rf_ConfigureDebugPrint);
    DO_INIT_CONFIGURE(rf_ConfigureReconstruction);
    DO_INIT_CONFIGURE(rf_ConfigureCopyback);
    DO_INIT_CONFIGURE(rf_ConfigureDiskQueueSystem);
    DO_INIT_CONFIGURE(rf_ConfigureCpuMonitor);
    isconfigged = 1;
  }
  RF_UNLOCK_MUTEX(configureMutex);

  /*
   * Null out the entire raid descriptor to avoid problems when we reconfig.
   * This also clears the valid bit.
   */
  /* XXX this clearing should be moved UP to outside of here.... that, or 
     rf_Configure() needs to take more arguments... XXX */
#if (defined(__NetBSD__) || defined(__OpenBSD__)) && defined(_KERNEL)
  unit = raidPtr->raidid;
  p = raidPtr->proc;   /* XXX save these... */
#endif
  bzero((char *)raidPtr, sizeof(RF_Raid_t));
#if (defined(__NetBSD__) || defined(__OpenBSD__)) && defined(_KERNEL)
  raidPtr->raidid = unit;
  raidPtr->proc = p;   /* XXX and then recover them..*/
#endif
  DO_RAID_MUTEX(&raidPtr->mutex);
  /* set up the cleanup list.  Do this after ConfigureDebug so that value of memDebug will be set */

  rf_MakeAllocList(raidPtr->cleanupList);
  if (raidPtr->cleanupList == NULL) {
    DO_RAID_FAIL();
    return(ENOMEM);
  }

  rc = rf_ShutdownCreate(&raidPtr->shutdownList, 
			 (void (*)(void *))rf_FreeAllocList, 
			 raidPtr->cleanupList);
  if (rc) {
    RF_ERRORMSG3("Unable to add to shutdown list file %s line %d rc=%d\n",
      __FILE__, __LINE__, rc);
    DO_RAID_FAIL();
    return(rc);
  }

  raidPtr->numRow = cfgPtr->numRow;
  raidPtr->numCol = cfgPtr->numCol;
  raidPtr->numSpare = cfgPtr->numSpare;

#if (defined(__NetBSD__) || defined(__OpenBSD__)) && defined(_KERNEL)
  /* XXX we don't even pretend to support more than one row 
     in the kernel... */
  if (raidPtr->numRow != 1) {
	  RF_ERRORMSG("Only one row supported in kernel.\n");
	  DO_RAID_FAIL();
	  return(EINVAL);
  }
#endif



  RF_CallocAndAdd(raidPtr->status, raidPtr->numRow, sizeof(RF_RowStatus_t),
    (RF_RowStatus_t *), raidPtr->cleanupList);
  if (raidPtr->status == NULL) {
    DO_RAID_FAIL();
    return(ENOMEM);
  }

  RF_CallocAndAdd(raidPtr->reconControl, raidPtr->numRow,
    sizeof(RF_ReconCtrl_t *), (RF_ReconCtrl_t **), raidPtr->cleanupList);
  if (raidPtr->reconControl == NULL) {
    DO_RAID_FAIL();
    return(ENOMEM);
  }
  for (i=0; i<raidPtr->numRow; i++) {
    raidPtr->status[i] = rf_rs_optimal;
    raidPtr->reconControl[i] = NULL;
  }

  DO_RAID_INIT_CONFIGURE(rf_ConfigureEngine);
#if !defined(KERNEL) && !defined(SIMULATE)
  DO_RAID_INIT_CONFIGURE(rf_InitThroughputStats);
#endif /* !KERNEL && !SIMULATE */

  DO_RAID_INIT_CONFIGURE(rf_ConfigureStripeLocks);

  DO_RAID_COND(&raidPtr->outstandingCond);

  raidPtr->nAccOutstanding = 0;
  raidPtr->waitShutdown = 0;

  DO_RAID_MUTEX(&raidPtr->access_suspend_mutex);
  DO_RAID_COND(&raidPtr->quiescent_cond);

  DO_RAID_COND(&raidPtr->waitForReconCond);

  DO_RAID_MUTEX(&raidPtr->recon_done_proc_mutex);
  DO_RAID_INIT_CONFIGURE(rf_ConfigureDisks);
  DO_RAID_INIT_CONFIGURE(rf_ConfigureSpareDisks);
  /* do this after ConfigureDisks & ConfigureSpareDisks to be sure dev no. is set */
  DO_RAID_INIT_CONFIGURE(rf_ConfigureDiskQueues);
#ifndef KERNEL
  DO_RAID_INIT_CONFIGURE(rf_ConfigureDiskThreads);
#endif /* !KERNEL */

  DO_RAID_INIT_CONFIGURE(rf_ConfigureLayout);

  DO_RAID_INIT_CONFIGURE(rf_ConfigurePSStatus);

  for(row=0;row<raidPtr->numRow;row++) {
    for(col=0;col<raidPtr->numCol;col++) {
      /*
       * XXX better distribution
       */
      raidPtr->hist_diskreq[row][col] = 0;
    }
  }

  if (rf_keepAccTotals) {
    raidPtr->keep_acc_totals = 1;
  }

  rf_StartUserStats(raidPtr);

  raidPtr->valid = 1;
  return(0);
}

static int init_rad(desc)
  RF_RaidAccessDesc_t  *desc;
{
	int rc;

	rc = rf_mutex_init(&desc->mutex);
	if (rc) {
		RF_ERRORMSG3("Unable to init mutex file %s line %d rc=%d\n", __FILE__,
			__LINE__, rc);
		return(rc);
	}
	rc = rf_cond_init(&desc->cond);
	if (rc) {
		RF_ERRORMSG3("Unable to init cond file %s line %d rc=%d\n", __FILE__,
			__LINE__, rc);
		rf_mutex_destroy(&desc->mutex);
		return(rc);
	}
	return(0);
}

static void clean_rad(desc)
  RF_RaidAccessDesc_t  *desc;
{
	rf_mutex_destroy(&desc->mutex);
	rf_cond_destroy(&desc->cond);
}

static void rf_ShutdownRDFreeList(ignored)
  void  *ignored;
{
	RF_FREELIST_DESTROY_CLEAN(rf_rad_freelist,next,(RF_RaidAccessDesc_t *),clean_rad);
}

static int rf_ConfigureRDFreeList(listp)
  RF_ShutdownList_t **listp;
{
	int rc;

	RF_FREELIST_CREATE(rf_rad_freelist, RF_MAX_FREE_RAD,
		RF_RAD_INC, sizeof(RF_RaidAccessDesc_t));
	if (rf_rad_freelist == NULL) {
		return(ENOMEM);
	}
	rc = rf_ShutdownCreate(listp, rf_ShutdownRDFreeList, NULL);
	if (rc) {
		RF_ERRORMSG3("Unable to add to shutdown list file %s line %d rc=%d\n", __FILE__,
			__LINE__, rc);
		rf_ShutdownRDFreeList(NULL);
		return(rc);
	}
	RF_FREELIST_PRIME_INIT(rf_rad_freelist, RF_RAD_INITIAL,next,
		(RF_RaidAccessDesc_t *),init_rad);
	return(0);
}

RF_RaidAccessDesc_t *rf_AllocRaidAccDesc(
  RF_Raid_t                    *raidPtr,
  RF_IoType_t                   type,
  RF_RaidAddr_t                 raidAddress,
  RF_SectorCount_t              numBlocks,
  caddr_t                       bufPtr,
  void                         *bp,
  RF_DagHeader_t              **paramDAG,
  RF_AccessStripeMapHeader_t  **paramASM,
  RF_RaidAccessFlags_t          flags,
  void                        (*cbF)(struct buf *),
  void                         *cbA,
  RF_AccessState_t             *states)
{
  RF_RaidAccessDesc_t *desc;

  RF_FREELIST_GET_INIT_NOUNLOCK(rf_rad_freelist,desc,next,(RF_RaidAccessDesc_t *),init_rad);
  if (raidPtr->waitShutdown) {
    /*
     * Actually, we're shutting the array down. Free the desc
     * and return NULL.
     */
    RF_FREELIST_DO_UNLOCK(rf_rad_freelist);
    RF_FREELIST_FREE_CLEAN(rf_rad_freelist,desc,next,clean_rad);
    return(NULL);
  }
  raidPtr->nAccOutstanding++;
  RF_FREELIST_DO_UNLOCK(rf_rad_freelist);
  
  desc->raidPtr     = (void*)raidPtr;
  desc->type        = type;
  desc->raidAddress = raidAddress;
  desc->numBlocks   = numBlocks;
  desc->bufPtr      = bufPtr;
  desc->bp          = bp;
  desc->paramDAG    = paramDAG;
  desc->paramASM    = paramASM;
  desc->flags       = flags;
  desc -> states    = states;
  desc -> state     = 0;

  desc->status      = 0;
  bzero((char *)&desc->tracerec, sizeof(RF_AccTraceEntry_t));
  desc->callbackFunc= (void (*)(RF_CBParam_t))cbF; /* XXX */
  desc->callbackArg = cbA;
  desc->next        = NULL;
  desc->head	    = desc;
  desc->numPending  = 0;
  desc->cleanupList = NULL;
  rf_MakeAllocList(desc->cleanupList);
  rf_get_threadid(desc->tid);
#ifdef SIMULATE
  desc->owner = rf_GetCurrentOwner();
#endif /* SIMULATE */
  return(desc);
}

void rf_FreeRaidAccDesc(RF_RaidAccessDesc_t *desc)
{
  RF_Raid_t *raidPtr = desc->raidPtr;

  RF_ASSERT(desc);

#if !defined(KERNEL) && !defined(SIMULATE)
  rf_StopThroughputStats(raidPtr);
#endif /* !KERNEL && !SIMULATE */

  rf_FreeAllocList(desc->cleanupList);
  RF_FREELIST_FREE_CLEAN_NOUNLOCK(rf_rad_freelist,desc,next,clean_rad);
    raidPtr->nAccOutstanding--;
    if (raidPtr->waitShutdown) {
      RF_SIGNAL_COND(raidPtr->outstandingCond);
    }
  RF_FREELIST_DO_UNLOCK(rf_rad_freelist);
}

#ifdef JIMZ
#define THREAD_NUMDESC 1024
#define THREAD_NUM 600
static RF_RaidAccessDesc_t *dest_hist[THREAD_NUM*THREAD_NUMDESC];
int jimz_access_num[THREAD_NUM];
#endif /* JIMZ */

/*********************************************************************
 * Main routine for performing an access.
 * Accesses are retried until a DAG can not be selected.  This occurs
 * when either the DAG library is incomplete or there are too many
 * failures in a parity group.
 ********************************************************************/
int rf_DoAccess(
  RF_Raid_t                    *raidPtr,
  RF_IoType_t                   type,
  int                           async_flag,
  RF_RaidAddr_t                 raidAddress, 
  RF_SectorCount_t              numBlocks,
  caddr_t                       bufPtr,
  void                         *bp_in,
  RF_DagHeader_t              **paramDAG,
  RF_AccessStripeMapHeader_t  **paramASM,
  RF_RaidAccessFlags_t          flags,
  RF_RaidAccessDesc_t         **paramDesc,
  void                        (*cbF)(struct buf *),
  void                         *cbA)
/*
type should be read or write
async_flag should be RF_TRUE or RF_FALSE
bp_in is a buf pointer.  void * to facilitate ignoring it outside the kernel
*/
{
  int tid;
  RF_RaidAccessDesc_t *desc;
  caddr_t lbufPtr = bufPtr;
#ifdef KERNEL
  struct buf *bp = (struct buf *) bp_in;
#if DFSTRACE > 0
  struct { RF_uint64 raidAddr; int numBlocks; char type;} dfsrecord;  
#endif /* DFSTRACE > 0 */
#else /* KERNEL */
  void *bp = bp_in;
#endif /* KERNEL */

  raidAddress += rf_raidSectorOffset;

  if (!raidPtr->valid) {
    RF_ERRORMSG("RAIDframe driver not successfully configured.  Rejecting access.\n");
    IO_BUF_ERR(bp, EINVAL, raidPtr->raidid);
    return(EINVAL);
  } 

#if defined(KERNEL) && DFSTRACE > 0
  if (rf_DFSTraceAccesses) {
    dfsrecord.raidAddr  = raidAddress;
    dfsrecord.numBlocks = numBlocks;
    dfsrecord.type      = type;
    dfs_log(DFS_NOTE, (char *) &dfsrecord, sizeof(dfsrecord), 0);
  }
#endif /* KERNEL && DFSTRACE > 0 */

  rf_get_threadid(tid);
  if (rf_accessDebug) {

	  printf("logBytes is: %d %d %d\n",raidPtr->raidid,
		 raidPtr->logBytesPerSector,
		 (int)rf_RaidAddressToByte(raidPtr,numBlocks));
    printf("[%d] %s raidAddr %d (stripeid %d-%d) numBlocks %d (%d bytes) buf 0x%lx\n",tid,
	   (type==RF_IO_TYPE_READ) ? "READ":"WRITE", (int)raidAddress, 
	   (int) rf_RaidAddressToStripeID(&raidPtr->Layout, raidAddress),
	   (int) rf_RaidAddressToStripeID(&raidPtr->Layout, raidAddress+numBlocks-1),
	   (int) numBlocks, 
	   (int) rf_RaidAddressToByte(raidPtr,numBlocks), 
	   (long) bufPtr);
  }

  if (raidAddress + numBlocks > raidPtr->totalSectors) {

    printf("DoAccess: raid addr %lu too large to access %lu sectors.  Max legal addr is %lu\n",
           (u_long)raidAddress,(u_long)numBlocks,(u_long)raidPtr->totalSectors);

#ifdef KERNEL
    if (type == RF_IO_TYPE_READ) {
      IO_BUF_ERR(bp, ENOSPC, raidPtr->raidid);
      return(ENOSPC);
    } else {
      IO_BUF_ERR(bp, ENOSPC, raidPtr->raidid);
      return(ENOSPC);
    }
#elif defined(SIMULATE)
    RF_PANIC();
#else /* SIMULATE */
    return(EIO);
#endif /* SIMULATE */
  }

#if !defined(KERNEL) && !defined(SIMULATE)
  rf_StartThroughputStats(raidPtr);
#endif /* !KERNEL && !SIMULATE */

  desc = rf_AllocRaidAccDesc(raidPtr, type, raidAddress,
			  numBlocks, lbufPtr, bp, paramDAG, paramASM,
			  flags, cbF, cbA, raidPtr->Layout.map->states);

  if (desc == NULL) {
    return(ENOMEM);
  }
#ifdef JIMZ
  dest_hist[(tid*THREAD_NUMDESC)+jimz_access_num[tid]]; jimz_access_num[tid]++;
#endif /* JIMZ */

  RF_ETIMER_START(desc->tracerec.tot_timer);

#ifdef SIMULATE
  /* simulator uses paramDesc to continue dag from test function */
  desc->async_flag=async_flag;

  *paramDesc=desc;
  
  return(0);
#endif /* SIMULATE */

  rf_ContinueRaidAccess(desc);

#ifndef KERNEL
  if (!(flags & RF_DAG_NONBLOCKING_IO)) {
    RF_LOCK_MUTEX(desc->mutex);
    while (!(desc->flags & RF_DAG_ACCESS_COMPLETE)) {
      RF_WAIT_COND(desc->cond, desc->mutex);
    }
    RF_UNLOCK_MUTEX(desc->mutex);
    rf_FreeRaidAccDesc(desc);
  }
#endif /* !KERNEL */

  return(0);
}

/* force the array into reconfigured mode without doing reconstruction */
int rf_SetReconfiguredMode(raidPtr, row, col)
  RF_Raid_t  *raidPtr;
  int         row;
  int         col;
{
  if (!(raidPtr->Layout.map->flags & RF_DISTRIBUTE_SPARE)) {
    printf("Can't set reconfigured mode in dedicated-spare array\n");
    RF_PANIC();
  }
  RF_LOCK_MUTEX(raidPtr->mutex);
  raidPtr->numFailures++;
  raidPtr->Disks[row][col].status = rf_ds_dist_spared;
  raidPtr->status[row] = rf_rs_reconfigured;
  /* install spare table only if declustering + distributed sparing architecture. */
  if ( raidPtr->Layout.map->flags & RF_BD_DECLUSTERED )
     rf_InstallSpareTable(raidPtr, row, col);
  RF_UNLOCK_MUTEX(raidPtr->mutex);
  return(0);
}

extern int fail_row, fail_col, fail_time;
extern int delayed_recon;

int rf_FailDisk(
  RF_Raid_t  *raidPtr,
  int         frow,
  int         fcol,
  int         initRecon)
{
  int tid;

  rf_get_threadid(tid);
  printf("[%d] Failing disk r%d c%d\n",tid,frow,fcol);
  RF_LOCK_MUTEX(raidPtr->mutex);
  raidPtr->numFailures++;
  raidPtr->Disks[frow][fcol].status = rf_ds_failed;
  raidPtr->status[frow] = rf_rs_degraded;
  RF_UNLOCK_MUTEX(raidPtr->mutex);
#ifdef SIMULATE
#if RF_DEMO > 0
  if (rf_demoMode) {
    rf_demo_update_mode (RF_DEMO_DEGRADED);
    fail_col = fcol; fail_row = frow; 
    fail_time = rf_CurTime(); /* XXX */
    if (initRecon)
      delayed_recon = RF_TRUE;
  }
  else {
    if (initRecon)
      rf_ReconstructFailedDisk(raidPtr, frow, fcol);
  }
#else /* RF_DEMO > 0 */
  if (initRecon)
    rf_ReconstructFailedDisk(raidPtr, frow, fcol);
#endif /* RF_DEMO > 0 */
#else /* SIMULATE */
  if (initRecon)
    rf_ReconstructFailedDisk(raidPtr, frow, fcol);
#endif /* SIMULATE */
  return(0);
}

#ifdef SIMULATE
extern RF_Owner_t recon_owner;

void rf_ScheduleContinueReconstructFailedDisk(reconDesc)
  RF_RaidReconDesc_t  *reconDesc;
{
  rf_DDEventRequest(rf_CurTime(), rf_ContinueReconstructFailedDisk,
    (void *) reconDesc, recon_owner, -4, -4, reconDesc->raidPtr, NULL);
}
#endif /* SIMULATE */

/* releases a thread that is waiting for the array to become quiesced.
 * access_suspend_mutex should be locked upon calling this
 */
void rf_SignalQuiescenceLock(raidPtr, reconDesc)
  RF_Raid_t           *raidPtr;
  RF_RaidReconDesc_t  *reconDesc;
{
  int tid;

  if (rf_quiesceDebug) {
    rf_get_threadid(tid);
    printf("[%d] Signalling quiescence lock\n", tid);
  }
  raidPtr->access_suspend_release = 1;

  if (raidPtr->waiting_for_quiescence) {
#ifndef SIMULATE
    SIGNAL_QUIESCENT_COND(raidPtr);
#else /* !SIMULATE */
    if (reconDesc) {
      rf_ScheduleContinueReconstructFailedDisk(reconDesc);
    }
#endif /* !SIMULATE */
  }
}

/* suspends all new requests to the array.  No effect on accesses that are in flight.  */
int rf_SuspendNewRequestsAndWait(raidPtr)
  RF_Raid_t  *raidPtr;
{
  if (rf_quiesceDebug)
    printf("Suspending new reqs\n");

  RF_LOCK_MUTEX(raidPtr->access_suspend_mutex);
  raidPtr->accesses_suspended++;
  raidPtr->waiting_for_quiescence = (raidPtr->accs_in_flight == 0) ? 0 : 1;
  
#ifndef SIMULATE
  if (raidPtr->waiting_for_quiescence) {
    raidPtr->access_suspend_release=0;
    while (!raidPtr->access_suspend_release) {
	    printf("Suspending: Waiting for Quiescence\n");
      WAIT_FOR_QUIESCENCE(raidPtr);
      raidPtr->waiting_for_quiescence = 0;
    }
  }
  printf("Quiescence reached..\n");
#endif /* !SIMULATE */

  RF_UNLOCK_MUTEX(raidPtr->access_suspend_mutex);
  return (raidPtr->waiting_for_quiescence);
}

/* wake up everyone waiting for quiescence to be released */
void rf_ResumeNewRequests(raidPtr)
  RF_Raid_t  *raidPtr;
{
  RF_CallbackDesc_t *t, *cb;

  if (rf_quiesceDebug)
    printf("Resuming new reqs\n");
  
  RF_LOCK_MUTEX(raidPtr->access_suspend_mutex);
  raidPtr->accesses_suspended--;
  if (raidPtr->accesses_suspended == 0)
    cb = raidPtr->quiesce_wait_list;
  else
    cb = NULL;
  raidPtr->quiesce_wait_list = NULL;
  RF_UNLOCK_MUTEX(raidPtr->access_suspend_mutex);
  
  while (cb) {
    t = cb;
    cb = cb->next;
    (t->callbackFunc)(t->callbackArg);
    rf_FreeCallbackDesc(t);
  }
}

/*****************************************************************************************
 *
 * debug routines
 *
 ****************************************************************************************/

static void set_debug_option(name, val)
  char  *name;
  long   val;
{
  RF_DebugName_t *p;

  for (p = rf_debugNames; p->name; p++) {
    if (!strcmp(p->name, name)) {
      *(p->ptr) = val;
      printf("[Set debug variable %s to %ld]\n",name,val);
      return;
    }
  }
  RF_ERRORMSG1("Unknown debug string \"%s\"\n",name);
}


/* would like to use sscanf here, but apparently not available in kernel */
/*ARGSUSED*/
static void rf_ConfigureDebug(cfgPtr)
  RF_Config_t  *cfgPtr;
{
  char *val_p, *name_p, *white_p;
  long val;
  int i;

  rf_ResetDebugOptions();
  for (i=0; cfgPtr->debugVars[i][0] && i < RF_MAXDBGV; i++) {
    name_p  = rf_find_non_white(&cfgPtr->debugVars[i][0]);
    white_p = rf_find_white(name_p);                                   /* skip to start of 2nd word */
    val_p   = rf_find_non_white(white_p);
    if (*val_p == '0' && *(val_p+1) == 'x') val = rf_htoi(val_p+2);
    else val = rf_atoi(val_p);
    *white_p = '\0';
    set_debug_option(name_p, val);
  }
}

/* performance monitoring stuff */

#define TIMEVAL_TO_US(t) (((long) t.tv_sec) * 1000000L + (long) t.tv_usec)

#if !defined(KERNEL) && !defined(SIMULATE)

/*
 * Throughput stats currently only used in user-level RAIDframe
 */

static int rf_InitThroughputStats(
  RF_ShutdownList_t  **listp,
  RF_Raid_t           *raidPtr,
  RF_Config_t         *cfgPtr)
{
  int rc;

  /* these used by user-level raidframe only */
  rc = rf_create_managed_mutex(listp, &raidPtr->throughputstats.mutex);
  if (rc) {
    RF_ERRORMSG3("Unable to init mutex file %s line %d rc=%d\n", __FILE__,
      __LINE__, rc);
    return(rc);
  }
  raidPtr->throughputstats.sum_io_us = 0;
  raidPtr->throughputstats.num_ios = 0;
  raidPtr->throughputstats.num_out_ios = 0;
  return(0);
}

void rf_StartThroughputStats(RF_Raid_t *raidPtr)
{
  RF_LOCK_MUTEX(raidPtr->throughputstats.mutex);
  raidPtr->throughputstats.num_ios++;
  raidPtr->throughputstats.num_out_ios++;
  if (raidPtr->throughputstats.num_out_ios == 1)
    RF_GETTIME(raidPtr->throughputstats.start);
  RF_UNLOCK_MUTEX(raidPtr->throughputstats.mutex);
}

static void rf_StopThroughputStats(RF_Raid_t *raidPtr)
{
  struct timeval diff;

  RF_LOCK_MUTEX(raidPtr->throughputstats.mutex);
  raidPtr->throughputstats.num_out_ios--;
  if (raidPtr->throughputstats.num_out_ios == 0) {
    RF_GETTIME(raidPtr->throughputstats.stop);
    RF_TIMEVAL_DIFF(&raidPtr->throughputstats.start, &raidPtr->throughputstats.stop, &diff);
    raidPtr->throughputstats.sum_io_us += TIMEVAL_TO_US(diff);
  }
  RF_UNLOCK_MUTEX(raidPtr->throughputstats.mutex);  
}

static void rf_PrintThroughputStats(RF_Raid_t *raidPtr)
{
  RF_ASSERT(raidPtr->throughputstats.num_out_ios == 0);
  if ( raidPtr->throughputstats.sum_io_us != 0 ) {
     printf("[Througphut: %8.2f IOs/second]\n", raidPtr->throughputstats.num_ios
       / (raidPtr->throughputstats.sum_io_us / 1000000.0));
  }
}

#endif /* !KERNEL && !SIMULATE */

void rf_StartUserStats(RF_Raid_t *raidPtr)
{
  RF_GETTIME(raidPtr->userstats.start);
  raidPtr->userstats.sum_io_us = 0;
  raidPtr->userstats.num_ios = 0;
  raidPtr->userstats.num_sect_moved = 0;
}

void rf_StopUserStats(RF_Raid_t *raidPtr)
{
  RF_GETTIME(raidPtr->userstats.stop);
}

void rf_UpdateUserStats(raidPtr, rt, numsect)
  RF_Raid_t  *raidPtr;
  int         rt;       /* resp time in us */
  int         numsect;  /* number of sectors for this access */
{
  raidPtr->userstats.sum_io_us += rt;
  raidPtr->userstats.num_ios++;
  raidPtr->userstats.num_sect_moved += numsect;
}

void rf_PrintUserStats(RF_Raid_t *raidPtr)
{
  long elapsed_us, mbs, mbs_frac;
  struct timeval diff;

  RF_TIMEVAL_DIFF(&raidPtr->userstats.start, &raidPtr->userstats.stop, &diff);
  elapsed_us = TIMEVAL_TO_US(diff);

  /* 2000 sectors per megabyte, 10000000 microseconds per second */
  if (elapsed_us)
    mbs = (raidPtr->userstats.num_sect_moved / 2000) / (elapsed_us / 1000000);
  else
    mbs = 0;

  /* this computes only the first digit of the fractional mb/s moved */
  if (elapsed_us) {
    mbs_frac = ((raidPtr->userstats.num_sect_moved / 200) / (elapsed_us / 1000000))
      - (mbs * 10);
  }
  else {
    mbs_frac = 0;
  }

  printf("Number of I/Os:             %ld\n",raidPtr->userstats.num_ios);
  printf("Elapsed time (us):          %ld\n",elapsed_us);
  printf("User I/Os per second:       %ld\n",RF_DB0_CHECK(raidPtr->userstats.num_ios, (elapsed_us/1000000)));
  printf("Average user response time: %ld us\n",RF_DB0_CHECK(raidPtr->userstats.sum_io_us, raidPtr->userstats.num_ios));
  printf("Total sectors moved:        %ld\n",raidPtr->userstats.num_sect_moved);
  printf("Average access size (sect): %ld\n",RF_DB0_CHECK(raidPtr->userstats.num_sect_moved, raidPtr->userstats.num_ios));
  printf("Achieved data rate:         %ld.%ld MB/sec\n",mbs,mbs_frac);
}
