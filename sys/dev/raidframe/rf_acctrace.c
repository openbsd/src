/*	$OpenBSD: rf_acctrace.c,v 1.1 1999/01/11 14:28:58 niklas Exp $	*/
/*	$NetBSD: rf_acctrace.c,v 1.1 1998/11/13 04:20:26 oster Exp $	*/
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

/*****************************************************************************
 * 
 * acctrace.c -- code to support collecting information about each access
 *
 *****************************************************************************/

/* :  
 * Log: rf_acctrace.c,v 
 * Revision 1.29  1996/07/27 23:36:08  jimz
 * Solaris port of simulator
 *
 * Revision 1.28  1996/07/17  21:00:58  jimz
 * clean up timer interface, tracing
 *
 * Revision 1.27  1996/06/14  14:35:24  jimz
 * clean up dfstrace protection
 *
 * Revision 1.26  1996/06/13  19:09:04  jimz
 * remove trace.dat file before beginning
 *
 * Revision 1.25  1996/06/12  04:41:26  jimz
 * tweaks to make genplot work with user-level driver
 * (mainly change stat collection)
 *
 * Revision 1.24  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.23  1996/06/09  02:36:46  jimz
 * lots of little crufty cleanup- fixup whitespace
 * issues, comment #ifdefs, improve typing in some
 * places (esp size-related)
 *
 * Revision 1.22  1996/06/05  18:06:02  jimz
 * Major code cleanup. The Great Renaming is now done.
 * Better modularity. Better typing. Fixed a bunch of
 * synchronization bugs. Made a lot of global stuff
 * per-desc or per-array. Removed dead code.
 *
 * Revision 1.21  1996/05/31  22:26:54  jimz
 * fix a lot of mapping problems, memory allocation problems
 * found some weird lock issues, fixed 'em
 * more code cleanup
 *
 * Revision 1.20  1996/05/30  23:22:16  jimz
 * bugfixes of serialization, timing problems
 * more cleanup
 *
 * Revision 1.19  1996/05/30  12:59:18  jimz
 * make etimer happier, more portable
 *
 * Revision 1.18  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.17  1996/05/23  00:33:23  jimz
 * code cleanup: move all debug decls to rf_options.c, all extern
 * debug decls to rf_options.h, all debug vars preceded by rf_
 *
 * Revision 1.16  1996/05/20  16:15:49  jimz
 * switch to rf_{mutex,cond}_{init,destroy}
 *
 * Revision 1.15  1996/05/18  20:10:00  jimz
 * bit of cleanup to compile cleanly in kernel, once again
 *
 * Revision 1.14  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.13  1995/11/30  16:26:43  wvcii
 * added copyright info
 *
 */

#ifdef _KERNEL
#define KERNEL
#endif

#include "rf_threadstuff.h"
#include "rf_types.h"
#include <sys/stat.h>
#include <sys/types.h>

#ifdef KERNEL
#if !defined(__NetBSD__) && !defined(__OpenBSD__)
#include <dfstrace.h>
#endif /* !__NetBSD__ && !__OpenBSD__ */
#if DFSTRACE > 0
#include <sys/dfs_log.h>
#include <sys/dfstracebuf.h>
#endif /* DFSTRACE > 0 */
#endif /* KERNEL */

#include "rf_debugMem.h"
#include "rf_acctrace.h"
#include "rf_general.h"
#include "rf_raid.h"
#include "rf_etimer.h"
#include "rf_hist.h"
#include "rf_shutdown.h"
#include "rf_sys.h"

static long numTracesSoFar;
static int accessTraceBufCount = 0;
static RF_AccTraceEntry_t *access_tracebuf;
static long traceCount;

int  rf_stopCollectingTraces;
RF_DECLARE_MUTEX(rf_tracing_mutex)
int rf_trace_fd;

static void rf_ShutdownAccessTrace(void *);

static void rf_ShutdownAccessTrace(ignored)
  void  *ignored;
{
  if (rf_accessTraceBufSize) {
    if (accessTraceBufCount) rf_FlushAccessTraceBuf();
#ifndef KERNEL
    close(rf_trace_fd);
#endif /* !KERNEL */
    RF_Free(access_tracebuf, rf_accessTraceBufSize * sizeof(RF_AccTraceEntry_t));
  }
  rf_mutex_destroy(&rf_tracing_mutex);
#if defined(KERNEL) && DFSTRACE > 0
  printf("RAIDFRAME: %d trace entries were sent to dfstrace\n",traceCount);
#endif /* KERNEL && DFSTRACE > 0 */
}

int rf_ConfigureAccessTrace(listp)
  RF_ShutdownList_t  **listp;
{
  int rc;

  numTracesSoFar = accessTraceBufCount = rf_stopCollectingTraces = 0;
  if (rf_accessTraceBufSize) {
    RF_Malloc(access_tracebuf, rf_accessTraceBufSize * sizeof(RF_AccTraceEntry_t), (RF_AccTraceEntry_t *));
    accessTraceBufCount = 0;
#ifndef KERNEL
    rc = unlink("trace.dat");
    if (rc && (errno != ENOENT)) {
      perror("unlink");
      RF_ERRORMSG("Unable to remove existing trace.dat\n");
      return(errno);
    }
    if ((rf_trace_fd = open("trace.dat",O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0 ) {
      perror("Unable to open trace.dat for output");
      return(errno);
    }
#endif /* !KERNEL */
  }
  traceCount = 0;
  numTracesSoFar = 0;
  rc = rf_mutex_init(&rf_tracing_mutex);
  if (rc) {
    RF_ERRORMSG3("Unable to init mutex file %s line %d rc=%d\n", __FILE__,
      __LINE__, rc);
  }
  rc = rf_ShutdownCreate(listp, rf_ShutdownAccessTrace, NULL);
  if (rc) {
    RF_ERRORMSG3("Unable to add to shutdown list file %s line %d rc=%d\n", __FILE__,
      __LINE__, rc);
    if (rf_accessTraceBufSize) {
      RF_Free(access_tracebuf, rf_accessTraceBufSize * sizeof(RF_AccTraceEntry_t));
#ifndef KERNEL
      close(rf_trace_fd);
#endif /* !KERNEL */
      rf_mutex_destroy(&rf_tracing_mutex);
    }
  }
  return(rc);
}

/* install a trace record.  cause a flush to disk or to the trace collector daemon
 * if the trace buffer is at least 1/2 full.
 */
void rf_LogTraceRec(raid, rec)
  RF_Raid_t           *raid;
  RF_AccTraceEntry_t  *rec;
{
	RF_AccTotals_t *acc = &raid->acc_totals;
#if 0
	RF_Etimer_t timer;
	int i, n;
#endif

  if (rf_stopCollectingTraces || ((rf_maxNumTraces >= 0) && (numTracesSoFar >= rf_maxNumTraces)))
    return;

#ifndef KERNEL
  if (rf_accessTraceBufSize) {
    RF_LOCK_MUTEX(rf_tracing_mutex);
    numTracesSoFar++;
    bcopy((char *)rec, (char *)&access_tracebuf[ accessTraceBufCount++ ], sizeof(RF_AccTraceEntry_t));
    if (accessTraceBufCount == rf_accessTraceBufSize)
      rf_FlushAccessTraceBuf();
    RF_UNLOCK_MUTEX(rf_tracing_mutex);
  }
#endif /* !KERNEL */
#if defined(KERNEL) && DFSTRACE > 0
  rec->index = traceCount++;
  if (traceon & DFS_TRACE_RAIDFRAME) {
    dfs_log(DFS_NOTE, (char *) rec, (int) sizeof(*rec), 0);
  }
#endif /* KERNEL && DFSTRACE > 0 */
	/* update AccTotals for this device */
	if (!raid->keep_acc_totals)
		return;
	acc->num_log_ents++;
	if (rec->reconacc) {
		acc->recon_start_to_fetch_us += rec->specific.recon.recon_start_to_fetch_us;
		acc->recon_fetch_to_return_us += rec->specific.recon.recon_fetch_to_return_us;
		acc->recon_return_to_submit_us += rec->specific.recon.recon_return_to_submit_us;
		acc->recon_num_phys_ios += rec->num_phys_ios;
		acc->recon_phys_io_us += rec->phys_io_us;
		acc->recon_diskwait_us += rec->diskwait_us;
		acc->recon_reccount++;
	}
	else {
		RF_HIST_ADD(acc->tot_hist, rec->total_us);
		RF_HIST_ADD(acc->dw_hist, rec->diskwait_us);
		/* count of physical ios which are too big.  often due to thermal recalibration */
		/* if bigvals > 0, you should probably ignore this data set */
		if (rec->diskwait_us > 100000)
			acc->bigvals++;
		acc->total_us += rec->total_us;
		acc->suspend_ovhd_us += rec->specific.user.suspend_ovhd_us;
		acc->map_us += rec->specific.user.map_us;
		acc->lock_us += rec->specific.user.lock_us;
		acc->dag_create_us += rec->specific.user.dag_create_us;
		acc->dag_retry_us += rec->specific.user.dag_retry_us;
		acc->exec_us += rec->specific.user.exec_us;
		acc->cleanup_us += rec->specific.user.cleanup_us;
		acc->exec_engine_us += rec->specific.user.exec_engine_us;
		acc->xor_us += rec->xor_us;
		acc->q_us += rec->q_us;
		acc->plog_us += rec->plog_us;
		acc->diskqueue_us += rec->diskqueue_us;
		acc->diskwait_us += rec->diskwait_us;
		acc->num_phys_ios += rec->num_phys_ios;
		acc->phys_io_us = rec->phys_io_us;
		acc->user_reccount++;
	}
}


/* assumes the tracing mutex is locked at entry.  In order to allow this to be called
 * from interrupt context, we don't do any copyouts here, but rather just wake trace
 * buffer collector thread.
 */
void rf_FlushAccessTraceBuf()
{
#ifndef KERNEL
  int size = accessTraceBufCount * sizeof(RF_AccTraceEntry_t);
  
  if (write(rf_trace_fd, (char *) access_tracebuf, size) < size ) {
    fprintf(stderr, "Unable to write traces to file.  tracing disabled\n");
    RF_Free(access_tracebuf, rf_accessTraceBufSize * sizeof(RF_AccTraceEntry_t));
    rf_accessTraceBufSize = 0;
    close(rf_trace_fd);
  }
#endif /* !KERNEL */
  accessTraceBufCount = 0;
}
