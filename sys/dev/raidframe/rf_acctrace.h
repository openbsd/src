/*	$OpenBSD: rf_acctrace.h,v 1.1 1999/01/11 14:28:58 niklas Exp $	*/
/*	$NetBSD: rf_acctrace.h,v 1.1 1998/11/13 04:20:26 oster Exp $	*/
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
 * acctrace.h -- header file for acctrace.c
 *
 *****************************************************************************/

/* :  
 *
 * Log: rf_acctrace.h,v 
 * Revision 1.32  1996/08/02 15:12:38  jimz
 * remove dead code
 *
 * Revision 1.31  1996/07/27  14:34:39  jimz
 * remove bogus semicolon
 *
 * Revision 1.30  1996/07/18  22:57:14  jimz
 * port simulator to AIX
 *
 * Revision 1.29  1996/07/17  21:00:58  jimz
 * clean up timer interface, tracing
 *
 * Revision 1.28  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.27  1996/06/09  02:36:46  jimz
 * lots of little crufty cleanup- fixup whitespace
 * issues, comment #ifdefs, improve typing in some
 * places (esp size-related)
 * /
 *
 * Revision 1.26  1996/06/05  18:06:02  jimz
 * Major code cleanup. The Great Renaming is now done.
 * Better modularity. Better typing. Fixed a bunch of
 * synchronization bugs. Made a lot of global stuff
 * per-desc or per-array. Removed dead code.
 *
 * Revision 1.25  1996/05/31  22:26:54  jimz
 * fix a lot of mapping problems, memory allocation problems
 * found some weird lock issues, fixed 'em
 * more code cleanup
 *
 * Revision 1.24  1996/05/30  12:59:18  jimz
 * make etimer happier, more portable
 *
 * Revision 1.23  1996/05/28  12:34:30  jimz
 * nail down size of reconacc
 *
 * Revision 1.22  1996/05/23  00:33:23  jimz
 * code cleanup: move all debug decls to rf_options.c, all extern
 * debug decls to rf_options.h, all debug vars preceded by rf_
 *
 * Revision 1.21  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.20  1996/05/02  14:57:24  jimz
 * change to boolean_t
 *
 * Revision 1.19  1995/12/14  18:37:06  jimz
 * convert to rf_types.h types
 *
 * Revision 1.18  1995/11/30  16:26:49  wvcii
 * added copyright info
 *
 * Revision 1.17  1995/09/30  19:49:23  jimz
 * add AccTotals structure, for capturing totals in kernel
 *
 * Revision 1.16  1995/09/12  00:20:55  wvcii
 * added support for tracing disk queue time
 *
 * Revision 1.15  95/09/06  19:23:12  wvcii
 * increased MAX_IOS_PER_TRACE_ENTRY from 1 to 4
 * 
 */

#ifndef _RF__RF_ACCTRACE_H_
#define _RF__RF_ACCTRACE_H_

#include "rf_types.h"
#include "rf_hist.h"
#include "rf_etimer.h"

typedef struct RF_user_acc_stats_s {
  RF_uint64 suspend_ovhd_us;           /* us spent mucking in the access-suspension code */
  RF_uint64 map_us;                    /* us spent mapping the access */
  RF_uint64 lock_us;                   /* us spent locking & unlocking stripes, including time spent blocked */
  RF_uint64 dag_create_us;             /* us spent creating the DAGs */
  RF_uint64 dag_retry_us;              /* _total_ us spent retrying the op -- not broken down into components */
  RF_uint64 exec_us;                   /* us spent in DispatchDAG */
  RF_uint64 exec_engine_us;            /* us spent in engine, not including blocking time */
  RF_uint64 cleanup_us;                /* us spent tearing down the dag & maps, and generally cleaning up */
} RF_user_acc_stats_t;

typedef struct RF_recon_acc_stats_s {
  RF_uint32 recon_start_to_fetch_us;
  RF_uint32 recon_fetch_to_return_us;
  RF_uint32 recon_return_to_submit_us;
} RF_recon_acc_stats_t;

typedef struct RF_acctrace_entry_s {
  union {
    RF_user_acc_stats_t    user;
    RF_recon_acc_stats_t   recon;
  } specific;
  RF_uint8     reconacc;         /* whether  this is a tracerec for a user acc or a recon acc */
  RF_uint64    xor_us;           /* us spent doing XORs */
  RF_uint64    q_us;             /* us spent doing XORs */
  RF_uint64    plog_us;          /* us spent waiting to stuff parity into log */
  RF_uint64    diskqueue_us;     /* _total_ us spent in disk queue(s), incl concurrent ops */
  RF_uint64    diskwait_us;      /* _total_ us spent waiting actually waiting on the disk, incl concurrent ops */
  RF_uint64    total_us;         /* total us spent on this access */
  RF_uint64    num_phys_ios;     /* number of physical I/Os invoked */
  RF_uint64    phys_io_us;       /* time of physical I/O */
  RF_Etimer_t  tot_timer;        /* a timer used to compute total access time */
  RF_Etimer_t  timer;            /* a generic timer val for timing events that live across procedure boundaries */
  RF_Etimer_t  recon_timer;      /* generic timer for recon stuff */
  RF_uint64    index;
} RF_AccTraceEntry_t;

typedef struct RF_AccTotals_s {
	/* user acc stats */
	RF_uint64      suspend_ovhd_us;
	RF_uint64      map_us;
	RF_uint64      lock_us;
	RF_uint64      dag_create_us;
	RF_uint64      dag_retry_us;
	RF_uint64      exec_us;
	RF_uint64      exec_engine_us;
	RF_uint64      cleanup_us;
	RF_uint64      user_reccount;
	/* recon acc stats */
	RF_uint64      recon_start_to_fetch_us;
	RF_uint64      recon_fetch_to_return_us;
	RF_uint64      recon_return_to_submit_us;
	RF_uint64      recon_io_overflow_count;
	RF_uint64      recon_phys_io_us;
	RF_uint64      recon_num_phys_ios;
	RF_uint64      recon_diskwait_us;
	RF_uint64      recon_reccount;
	/* trace entry stats */
	RF_uint64      xor_us;
	RF_uint64      q_us;
	RF_uint64      plog_us;
	RF_uint64      diskqueue_us;
	RF_uint64      diskwait_us;
	RF_uint64      total_us;
	RF_uint64      num_log_ents;
	RF_uint64      phys_io_overflow_count;
	RF_uint64      num_phys_ios;
	RF_uint64      phys_io_us;
	RF_uint64      bigvals;
	/* histograms */
	RF_Hist_t dw_hist[RF_HIST_NUM_BUCKETS];
	RF_Hist_t tot_hist[RF_HIST_NUM_BUCKETS];
} RF_AccTotals_t;

#if RF_UTILITY == 0
RF_DECLARE_EXTERN_MUTEX(rf_tracing_mutex)
#endif /* RF_UTILITY == 0 */

int  rf_ConfigureAccessTrace(RF_ShutdownList_t **listp);
void rf_LogTraceRec(RF_Raid_t *raid, RF_AccTraceEntry_t *rec);
void rf_FlushAccessTraceBuf(void);

#endif /* !_RF__RF_ACCTRACE_H_ */
