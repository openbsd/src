/*	$OpenBSD: rf_raid.h,v 1.8 2007/04/10 17:47:55 miod Exp $	*/
/*	$NetBSD: rf_raid.h,v 1.12 2000/02/24 17:12:10 oster Exp $	*/

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

/*************************************************
 * rf_raid.h -- Main header file for RAID driver.
 *************************************************/


#ifndef	_RF__RF_RAID_H_
#define	_RF__RF_RAID_H_

#include "rf_archs.h"
#include "rf_types.h"
#include "rf_threadstuff.h"

#if	defined(__NetBSD__)
#include "rf_netbsd.h"
#elif	defined(__OpenBSD__)
#include "rf_openbsd.h"
#endif

#include <sys/disklabel.h>
#include <sys/types.h>

#include "rf_alloclist.h"
#include "rf_stripelocks.h"
#include "rf_layout.h"
#include "rf_disks.h"
#include "rf_debugMem.h"
#include "rf_diskqueue.h"
#include "rf_reconstruct.h"
#include "rf_acctrace.h"

#if	RF_INCLUDE_PARITYLOGGING > 0
#include "rf_paritylog.h"
#endif	/* RF_INCLUDE_PARITYLOGGING > 0 */

#define	RF_MAX_DISKS			128	/* Max disks per array. */
#define	RF_DEV2RAIDID(_dev)		(DISKUNIT(_dev))

#define	RF_COMPONENT_LABEL_VERSION_1	1
#define	RF_COMPONENT_LABEL_VERSION	2
#define	RF_RAID_DIRTY			0
#define	RF_RAID_CLEAN			1

/*
 * Each row in the array is a distinct parity group, so
 * each has its own status, which is one of the following.
 */
typedef enum RF_RowStatus_e {
	rf_rs_optimal,
	rf_rs_degraded,
	rf_rs_reconstructing,
	rf_rs_reconfigured
} RF_RowStatus_t;

struct RF_CumulativeStats_s {
	struct timeval		start;		/*
						 * The time when the stats were
						 * last started.
						 */
	struct timeval		stop;		/*
						 * The time when the stats were
						 * last stopped.
						 */
	long			sum_io_us;	/*
						 * Sum of all user response
						 * times (us).
						 */
	long			num_ios;	/*
						 * Total number of I/Os
						 * serviced.
						 */
	long			num_sect_moved;	/*
						 * Total number of sectors read
						 * or written.
						 */
};

struct RF_ThroughputStats_s {
	RF_DECLARE_MUTEX	(mutex);	/*
						 * A mutex used to lock the
						 * configuration stuff.
						 */
	struct timeval		 start;		/*
						 * Timer started when
						 * numOutstandingRequests
						 * moves from 0 to 1.
						 */
	struct timeval		 stop;		/*
						 * Timer stopped when
						 * numOutstandingRequests
						 * moves from 1 to 0.
						 */
	RF_uint64		 sum_io_us;	/*
						 * Total time timer is enabled.
						 */
	RF_uint64		 num_ios;	/*
						 * Total number of I/Os
						 * processed by RAIDframe.
						 */
	long			 num_out_ios;	/*
						 * Number of outstanding I/Os.
						 */
};

struct RF_Raid_s {
	/* This portion never changes, and can be accessed without locking */
	/*
	 * An exception is Disks[][].status, which requires locking when it is
	 * changed. XXX This is no longer true. numSpare and friends can
	 * change now.
	 */
	u_int			  numRow;	/*
						 * Number of rows of disks,
						 * typically == # of ranks.
						 */
	u_int			  numCol;	/*
						 * Number of columns of disks,
						 * typically == # of disks/rank.
						 */
	u_int			  numSpare;	/* Number of spare disks. */
	int			  maxQueueDepth;/* Max disk queue depth. */
	RF_SectorCount_t	  totalSectors;	/*
						 * Total number of sectors
						 * in the array.
						 */
	RF_SectorCount_t	  sectorsPerDisk;
						/*
						 * Number of sectors on each
						 * disk.
						 */
	u_int			  logBytesPerSector;
						/*
						 * Base-2 log of the number
						 * of bytes in a sector.
						 */
	u_int			  bytesPerSector;
						/* Bytes in a sector. */
	RF_int32		  sectorMask;	/* Mask of bytes-per-sector. */

	RF_RaidLayout_t		  Layout;	/*
						 * All information related to
						 * layout.
						 */
	RF_RaidDisk_t		**Disks;	/*
						 * All information related to
						 * physical disks.
						 */
	RF_DiskQueue_t		**Queues;	/*
						 * All information related to
						 * disk queues.
						 */
	RF_DiskQueueSW_t	 *qType;	/*
						 * Pointer to the DiskQueueSW
						 * used for the component
						 * queues.
						 */
	/*
	 * NOTE:  This is an anchor point via which the queues can be
	 * accessed, but the enqueue/dequeue routines in diskqueue.c use a
	 * local copy of this pointer for the actual accesses.
	 */
	/*
	 * The remainder of the structure can change, and therefore requires
	 * locking on reads and updates.
	 */
	RF_DECLARE_MUTEX	 (mutex);	/*
						 * Mutex used to serialize
						 * access to the fields below.
						 */
	RF_RowStatus_t		 *status;	/*
						 * The status of each row in
						 * the array.
						 */
	int			  valid;	/*
						 * Indicates successful
						 * configuration.
						 */
	RF_LockTableEntry_t	 *lockTable;	/* Stripe-lock table. */
	RF_LockTableEntry_t	 *quiesceLock;	/* Quiescence table. */
	int			  numFailures;	/*
						 * Total number of failures
						 * in the array.
						 */
	int			  numNewFailures;
						/*
						 * Number of *new* failures
						 * (that haven't caused a
						 * mod_counter update).
						 */

	int			  parity_good;	/*
						 * !0 if parity is known to be
						 * correct.
						 */
	int			  serial_number;/*
						 * A "serial number" for this
						 * set.
						 */
	int			  mod_counter;	/*
						 * Modification counter for
						 * component labels.
						 */
	int			  clean;	/*
						 * The clean bit for this array.
						 */

	int			  openings;	/*
						 * Number of I/Os that can be
						 * scheduled simultaneously
						 * (high-level - not a
				 		 * per-component limit).
						 */

	int			  maxOutstanding;
						/*
						 * maxOutstanding requests
						 * (per-component).
						 */
	int			  autoconfigure;
						/*
						 * Automatically configure
						 * this RAID set.
						 * 0 == no, 1 == yes
						 */
	int			  root_partition;
						/*
						 * Use this set as
						 * 0 == no, 1 == yes.
						 */
	int			  last_unit;	/*
						 * Last unit number (e.g. 0
						 * for /dev/raid0) of this
						 * component. Used for
						 * autoconfigure only.
						 */
	int			  config_order;	/*
						 * 0 .. n. The order in which
						 * the component should be
						 * auto-configured.
						 * E.g. 0 is will done first,
						 * (and would become raid0).
						 * This may be in conflict
						 * with last_unit !!?!
						 */
						/* Not currently used. */

	/*
	 * Cleanup stuff.
	 */
	RF_ShutdownList_t	 *shutdownList;	/* Shutdown activities. */
	RF_AllocListElem_t	 *cleanupList;	/*
						 * Memory to be freed at
						 * shutdown time.
						 */

	/*
	 * Recon stuff.
	 */
	RF_HeadSepLimit_t	  headSepLimit;
	int			  numFloatingReconBufs;
	int			  reconInProgress;
	RF_DECLARE_COND		 (waitForReconCond);
	RF_RaidReconDesc_t	 *reconDesc;	/* Reconstruction descriptor. */
	RF_ReconCtrl_t		**reconControl;	/*
						 * Reconstruction control
						 * structure pointers for each
						 * row in the array.
						 */

	/*
	 * Array-quiescence stuff.
	 */
	RF_DECLARE_MUTEX	 (access_suspend_mutex);
	RF_DECLARE_COND		 (quiescent_cond);
	RF_IoCount_t		  accesses_suspended;
	RF_IoCount_t		  accs_in_flight;
	int			  access_suspend_release;
	int			  waiting_for_quiescence;
	RF_CallbackDesc_t	 *quiesce_wait_list;

	/*
	 * Statistics.
	 */
#if	!defined(_KERNEL) && !defined(SIMULATE)
	RF_ThroughputStats_t	  throughputstats;
#endif	/* !_KERNEL && !SIMULATE */
	RF_CumulativeStats_t	  userstats;
	int			  parity_rewrite_stripes_done;
	int			  recon_stripes_done;
	int			  copyback_stripes_done;

	int			  recon_in_progress;
	int			  parity_rewrite_in_progress;
	int			  copyback_in_progress;

	/*
	 * Engine thread control.
	 */
	RF_DECLARE_MUTEX	 (node_queue_mutex);
	RF_DECLARE_COND		 (node_queue_cond);
	RF_DagNode_t		 *node_queue;
	RF_Thread_t		  parity_rewrite_thread;
	RF_Thread_t		  copyback_thread;
	RF_Thread_t		  engine_thread;
	RF_Thread_t		  recon_thread;
	RF_ThreadGroup_t	  engine_tg;
	int			  shutdown_engine;
	int			  dags_in_flight;	/* Debug. */

	/*
	 * PSS (Parity Stripe Status) stuff.
	 */
	RF_FreeList_t		 *pss_freelist;
	long			  pssTableSize;

	/*
	 * Reconstruction stuff.
	 */
	int			  procsInBufWait;
	int			  numFullReconBuffers;
	RF_AccTraceEntry_t	 *recon_tracerecs;
	unsigned long		  accumXorTimeUs;
	RF_ReconDoneProc_t	 *recon_done_procs;
	RF_DECLARE_MUTEX	 (recon_done_proc_mutex);
	/*
	 * nAccOutstanding, waitShutdown protected by desc freelist lock
	 * (This may seem strange, since that's a central serialization point
	 * for a per-array piece of data, but otherwise, it'd be an extra
	 * per-array lock, and that'd only be less efficient...)
	 */
	RF_DECLARE_COND		 (outstandingCond);
	int			  waitShutdown;
	int			  nAccOutstanding;

	RF_DiskId_t		**diskids;
	RF_DiskId_t		 *sparediskids;

	int			  raidid;
	RF_AccTotals_t		  acc_totals;
	int			  keep_acc_totals;

	struct raidcinfo	**raid_cinfo;	/* Array of component info. */

	int			  terminate_disk_queues;

	/*
	 * XXX
	 *
	 * Config-specific information should be moved
	 * somewhere else, or at least hung off this
	 * in some generic way.
	 */

	/* Used by rf_compute_workload_shift. */
	RF_RowCol_t		  hist_diskreq[RF_MAXROW][RF_MAXCOL];

	/* Used by declustering. */
	int			  noRotate;

#if	RF_INCLUDE_PARITYLOGGING > 0
	/* used by parity logging */
	RF_SectorCount_t	  regionLogCapacity;
	RF_ParityLogQueue_t	  parityLogPool;/*
						 * Pool of unused parity logs.
						 */
	RF_RegionInfo_t		 *regionInfo;	/* Array of region state. */
	int			  numParityLogs;
	int			  numSectorsPerLog;
	int			  regionParityRange;
	int			  logsInUse;	/* Debugging. */
	RF_ParityLogDiskQueue_t	  parityLogDiskQueue;
						/*
						 * State of parity logging
						 * disk work.
						 */
	RF_RegionBufferQueue_t	  regionBufferPool;
					 	/*
						 * buffers for holding region
						 * log.
						 */
	RF_RegionBufferQueue_t	  parityBufferPool;
						/*
						 * Buffers for holding parity.
						 */
	caddr_t			  parityLogBufferHeap;
						/*
						 * Pool of unused parity logs.
						 */
	RF_Thread_t		  pLogDiskThreadHandle;

#endif	/* RF_INCLUDE_PARITYLOGGING > 0 */
};

#endif	/* !_RF__RF_RAID_H_ */
