/*	$OpenBSD: rf_optnames.h,v 1.1 1999/01/11 14:29:33 niklas Exp $	*/
/*	$NetBSD: rf_optnames.h,v 1.1 1998/11/13 04:20:31 oster Exp $	*/
/*
 * rf_optnames.h
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
 * Don't protect against multiple inclusion here- we actually want this.
 */

#ifdef _KERNEL
#define KERNEL
#endif

RF_DBG_OPTION(accSizeKB,0) /* if nonzero, the fixed access size to run */
RF_DBG_OPTION(accessDebug,0)
RF_DBG_OPTION(accessTraceBufSize,0)
RF_DBG_OPTION(alignAccesses,0) /* whether accs should be aligned to their size */
RF_DBG_OPTION(camlayerIOs,0)
RF_DBG_OPTION(camlayerDebug,0) /* debug CAM activity */
RF_DBG_OPTION(cscanDebug,0) /* debug CSCAN sorting */
RF_DBG_OPTION(dagDebug,0)
RF_DBG_OPTION(debugPrintUseBuffer,0)
RF_DBG_OPTION(degDagDebug,0)
RF_DBG_OPTION(disableAsyncAccs,0)
RF_DBG_OPTION(diskDebug,0)
RF_DBG_OPTION(doDebug,0)
RF_DBG_OPTION(dtDebug,0)
RF_DBG_OPTION(enableAtomicRMW,0) /* this debug var enables locking of the disk
                                  * arm during small-write operations.  Setting
                                  * this variable to anything other than 0 will
                                  * result in deadlock.  (wvcii)
                                  */
RF_DBG_OPTION(engineDebug,0)
RF_DBG_OPTION(fifoDebug,0) /* debug fifo queueing */
RF_DBG_OPTION(floatingRbufDebug,0)
RF_DBG_OPTION(forceHeadSepLimit,-1)
RF_DBG_OPTION(forceNumFloatingReconBufs,-1) /* wire down number of extra recon buffers to use */
RF_DBG_OPTION(keepAccTotals,0) /* turn on keep_acc_totals */
RF_DBG_OPTION(lockTableSize,RF_DEFAULT_LOCK_TABLE_SIZE)
RF_DBG_OPTION(mapDebug,0)
RF_DBG_OPTION(maxNumTraces,-1)
RF_DBG_OPTION(maxRandomSizeKB,128) /* if rf_accSizeKB==0, acc sizes are uniform in [ (1/2)..maxRandomSizeKB ] */
RF_DBG_OPTION(maxTraceRunTimeSec,0)
RF_DBG_OPTION(memAmtDebug,0) /* trace amount of memory allocated */
RF_DBG_OPTION(memChunkDebug,0)
RF_DBG_OPTION(memDebug,0)
RF_DBG_OPTION(memDebugAddress,0)
RF_DBG_OPTION(numBufsToAccumulate,1) /* number of buffers to accumulate before doing XOR */
RF_DBG_OPTION(prReconSched,0)
RF_DBG_OPTION(printDAGsDebug,0)
RF_DBG_OPTION(printStatesDebug,0)
RF_DBG_OPTION(protectedSectors,64L) /* # of sectors at start of disk to
                                       exclude from RAID address space */
RF_DBG_OPTION(pssDebug,0)
RF_DBG_OPTION(queueDebug,0)
RF_DBG_OPTION(quiesceDebug,0)
RF_DBG_OPTION(raidSectorOffset,0) /* added to all incoming sectors to
                                     debug alignment problems */
RF_DBG_OPTION(reconDebug,0)
RF_DBG_OPTION(reconbufferDebug,0)
RF_DBG_OPTION(rewriteParityStripes,0) /* debug flag that causes parity rewrite at startup */
RF_DBG_OPTION(scanDebug,0) /* debug SCAN sorting */
RF_DBG_OPTION(showXorCallCounts,0) /* show n-way Xor call counts */
RF_DBG_OPTION(shutdownDebug,0) /* show shutdown calls */
RF_DBG_OPTION(sizePercentage,100)
RF_DBG_OPTION(sstfDebug,0) /* turn on debugging info for sstf queueing */
RF_DBG_OPTION(stripeLockDebug,0)
RF_DBG_OPTION(suppressLocksAndLargeWrites,0)
RF_DBG_OPTION(suppressTraceDelays,0)
RF_DBG_OPTION(testDebug,0)
RF_DBG_OPTION(useMemChunks,1)
RF_DBG_OPTION(validateDAGDebug,0)
RF_DBG_OPTION(validateVisitedDebug,1) /* XXX turn to zero by default? */
RF_DBG_OPTION(verifyParityDebug,0)
RF_DBG_OPTION(warnLongIOs,0)

#ifdef KERNEL
RF_DBG_OPTION(debugKernelAccess,0) /* DoAccessKernel debugging */
#endif /* KERNEL */

#ifndef KERNEL
RF_DBG_OPTION(disableParityVerify,0) /* supress verification of parity */
RF_DBG_OPTION(interactiveScript,0) /* set as a debug option for now */
RF_DBG_OPTION(looptestShowWrites,0) /* user-level loop test write debugging */
RF_DBG_OPTION(traceDebug,0)
#endif /* !KERNEL */

#ifdef SIMULATE
RF_DBG_OPTION(addrSizePercentage,100)
RF_DBG_OPTION(diskTrace,0) /* ised to turn the timing traces on and of */
RF_DBG_OPTION(eventDebug,0)
RF_DBG_OPTION(mWactive,1500)
RF_DBG_OPTION(mWidle,625)
RF_DBG_OPTION(mWsleep,15)
RF_DBG_OPTION(mWspinup,3500)
#endif /* SIMULATE */

#if RF_INCLUDE_PARITYLOGGING > 0
RF_DBG_OPTION(forceParityLogReint,0)
RF_DBG_OPTION(numParityRegions,0) /* number of regions in the array */
RF_DBG_OPTION(numReintegrationThreads,1)
RF_DBG_OPTION(parityLogDebug,0) /* if nonzero, enables debugging of parity logging */
RF_DBG_OPTION(totalInCoreLogCapacity,1024*1024) /* target bytes available for in-core logs */
#endif /* RF_INCLUDE_PARITYLOGGING > 0 */

#if DFSTRACE > 0
RF_DBG_OPTION(DFSTraceAccesses,0)
#endif /* DFSTRACE > 0 */

#if RF_DEMO > 0
RF_DBG_OPTION(demoMeterHpos,0) /* horizontal position of meters for demo mode */
RF_DBG_OPTION(demoMeterTag,0)
RF_DBG_OPTION(demoMeterVpos,0) /* vertical position of meters for demo mode */
RF_DBG_OPTION(demoMode,0)
RF_DBG_OPTION(demoSMM,0)
RF_DBG_OPTION(demoSuppressReconInitVerify,0) /* supress initialization & verify for recon */
#endif /* RF_DEMO > 0 */
